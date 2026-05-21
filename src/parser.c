#include "parser.h"
#include "scanner.h"
#include "symbol.h"
#include "quadruple.h"

// ================================================================
// * parser.c — 语法分析与语义分析 (递归下降)
// *
// * 代码对应关系:
// *   PROGRAM()          -> 对应文档中 Syntax::PROGRAM()
// *   SUB_PROGRAM()      -> 对应文档中 Syntax::SUB_PROGRAM()
// *   VARIABLE()         -> 对应文档中 Syntax::VARIABLE() + 语义动作 a1~a6
// *   ID_SEQUENCE()      -> 对应文档中 Syntax::ID_SEQUENCE()
// *   TYPE()             -> 对应文档中 Syntax::TYPE()
// *   COM_SENTENCE()     -> 对应文档中 Syntax::COM_SENTENCE()
// *   STATEMENT()        -> 扩展: 支持 IF/WHILE 语句
// *   EVA_SENTENCE()     -> 对应文档中 Syntax::EVA_SENTENCE()
// *   EXPRESSION()       -> 递归下降表达式分析 (7级优先级)
// *   FACTOR()           -> 因子分析
// * ================================================================

static Token peek; // 预读 Token
static int peek_valid; // peek 是否有效

// 读取下一个 Token 到 peek
static void next_token(Compiler *c) {
    if (peek_valid) {
        c->token = peek;
        peek_valid = 0;
    } else {
        scanner_next_token(c);
    }
}

// ========== 表达式分析 (递归下降) ==========
// * 优先级层次 (低 -> 高):
// *   L1: or
// *   L2: and
// *   L3: not (unary)
// *   L4: = <> < > <= >=  (比较)
// *   L5: + - (加减)
// *   L6: * / (乘除)
// *   L7: id, const, (E), unary -
static void parse_expression(Compiler *c, SemValue *sv);
static void parse_or_expr(Compiler *c, SemValue *sv);
static void parse_and_expr(Compiler *c, SemValue *sv);
static void parse_not_expr(Compiler *c, SemValue *sv);
static void parse_comparison(Compiler *c, SemValue *sv);
static void parse_arith_expr(Compiler *c, SemValue *sv);
static void parse_term(Compiler *c, SemValue *sv);
static void parse_factor(Compiler *c, SemValue *sv);

static void parse_or_expr(Compiler *c, SemValue *sv) {
    parse_and_expr(c, sv);
    while (c->token.code == TOK_OR) {
        next_token(c);
        SemValue right;
        parse_and_expr(c, &right);
        char *t = sym_new_temp(c);
        quad_emit(c, OP_OR_OP, sv->name, right.name, t);
        strncpy(sv->name, t, MAX_NAME - 1);
        sv->is_temp = 1;
    }
}

static void parse_and_expr(Compiler *c, SemValue *sv) {
    parse_not_expr(c, sv);
    while (c->token.code == TOK_AND) {
        next_token(c);
        SemValue right;
        parse_not_expr(c, &right);
        char *t = sym_new_temp(c);
        quad_emit(c, OP_AND_OP, sv->name, right.name, t);
        strncpy(sv->name, t, MAX_NAME - 1);
        sv->is_temp = 1;
    }
}

static void parse_not_expr(Compiler *c, SemValue *sv) {
    if (c->token.code == TOK_NOT) {
        next_token(c);
        parse_not_expr(c, sv);
        char *t = sym_new_temp(c);
        quad_emit(c, OP_NOT_OP, sv->name, "_", t);
        strncpy(sv->name, t, MAX_NAME - 1);
        sv->is_temp = 1;
    } else {
        parse_comparison(c, sv);
    }
}

static void parse_comparison(Compiler *c, SemValue *sv) {
    parse_arith_expr(c, sv);
    if (c->token.code == TOK_EQ || c->token.code == TOK_LT ||
        c->token.code == TOK_GT || c->token.code == TOK_LE ||
        c->token.code == TOK_GE || c->token.code == TOK_NE) {
        int rel_op = c->token.code;
        next_token(c);
        SemValue right;
        parse_arith_expr(c, &right);
        char *t = sym_new_temp(c);
        int quad_op;
        switch (rel_op) {
            case TOK_EQ: quad_op = OP_JE; break;
            case TOK_LT: quad_op = OP_JL; break;
            case TOK_GT: quad_op = OP_JG; break;
            case TOK_LE: quad_op = OP_JLE; break;
            case TOK_GE: quad_op = OP_JGE; break;
            case TOK_NE: quad_op = OP_JNE; break;
            default: quad_op = OP_JE; break;
        }
        // 比较操作生成四元式, 结果在临时变量
        quad_emit(c, quad_op, sv->name, right.name, t);
        strncpy(sv->name, t, MAX_NAME - 1);
        sv->is_temp = 1;
    }
}

static void parse_arith_expr(Compiler *c, SemValue *sv) {
    parse_term(c, sv);
    while (c->token.code == TOK_PLUS || c->token.code == TOK_MINUS) {
        int op = c->token.code;
        next_token(c);
        SemValue right;
        parse_term(c, &right);
        char *t = sym_new_temp(c);
        if (op == TOK_PLUS)
            quad_emit(c, OP_ADD, sv->name, right.name, t);
        else
            quad_emit(c, OP_SUB, sv->name, right.name, t);
        strncpy(sv->name, t, MAX_NAME - 1);
        sv->is_temp = 1;
    }
}

static void parse_term(Compiler *c, SemValue *sv) {
    parse_factor(c, sv);
    while (c->token.code == TOK_MUL || c->token.code == TOK_DIV) {
        int op = c->token.code;
        next_token(c);
        SemValue right;
        parse_factor(c, &right);
        char *t = sym_new_temp(c);
        if (op == TOK_MUL)
            quad_emit(c, OP_MUL, sv->name, right.name, t);
        else
            quad_emit(c, OP_DIV, sv->name, right.name, t);
        strncpy(sv->name, t, MAX_NAME - 1);
        sv->is_temp = 1;
    }
}

static void parse_factor(Compiler *c, SemValue *sv) {
    if (c->token.code == TOK_ID) {
        // 标识符 -> 返回其名称
        int idx = c->token.value;
        if (idx >= 0 && idx < c->sym_count) {
            strncpy(sv->name, c->sym_table[idx].name, MAX_NAME - 1);
        } else {
            snprintf(sv->name, MAX_NAME, "id%d", idx);
        }
        sv->is_temp = 0;
        next_token(c);
    } else if (c->token.code == TOK_CONST) {
        // 常数 -> 返回其字符串表示
        int idx = c->token.value;
        if (idx >= 0 && idx < c->const_count) {
            snprintf(sv->name, MAX_NAME, "%g", c->const_table[idx]);
        } else {
            snprintf(sv->name, MAX_NAME, "%d", idx);
        }
        sv->is_temp = 0;
        next_token(c);
    } else if (c->token.code == TOK_LPAREN) {
        next_token(c);
        parse_expression(c, sv);
        if (c->token.code == TOK_RPAREN)
            next_token(c);
        else {
            c->error_flag = 1;
            snprintf(c->error_msg, sizeof(c->error_msg), "语法错误: 期望 ')'");
        }
    } else if (c->token.code == TOK_MINUS) {
        // 一元负号: -factor
        next_token(c);
        parse_factor(c, sv);
        char *t = sym_new_temp(c);
        quad_emit(c, OP_SUB, "0", sv->name, t);
        strncpy(sv->name, t, MAX_NAME - 1);
        sv->is_temp = 1;
    } else {
        c->error_flag = 1;
        snprintf(c->error_msg, sizeof(c->error_msg),
            "表达式语法错误: 意外的 Token %d", c->token.code);
        snprintf(sv->name, MAX_NAME, "err");
    }
}

static void parse_expression(Compiler *c, SemValue *sv) {
    parse_or_expr(c, sv);
}

// ========== 声明语句: 翻译文法实现 ==========
// * 语义动作 (对应 PPT 第19-20页):
// *   a1: id.cat := v; id.offset := 0;
// *   a2: push(id_Token.val)
// *   a3: id.type := i; id.len := 4;
// *   a4: id.type := r; id.len := 8;
// *   a5: id.type := c; id.len := 1;
// *   a6: while (栈不空) {
// *           id.entry := pop();
// *           enter(id.entry, id.type, id.cat, id.offset);
// *           id.offset := id.offset + id.len;
// *       }
static void semantic_a1(Compiler *c) {
    c->cur_offset = 0; // a1: id.offset := 0
}

static void semantic_a2(Compiler *c) {
    // a2: push(id_Token.val)
    if (c->sem_top < MAX_SYMBOLS) {
        c->sem_stack[c->sem_top++] = c->token.value;
    }
}

static void semantic_a3(Compiler *c) {
    c->cur_type = TY_INTEGER; // a3: id.type := i; id.len := 4
}

static void semantic_a4(Compiler *c) {
    c->cur_type = TY_REAL; // a4: id.type := r; id.len := 8
}

static void semantic_a5(Compiler *c) {
    c->cur_type = TY_CHAR; // a5: id.type := c; id.len := 1
}

static void semantic_a6(Compiler *c) {
    // a6: 按声明顺序填写符号表 (FIFO: 从栈底到栈顶)
    int type = c->cur_type;
    int len = type_len(type);
    int i;
    for (i = 0; i < c->sem_top; i++) {
        int idx = c->sem_stack[i];
        if (idx >= 0 && idx < c->sym_count) {
            sym_set_type(c, idx, type, len, c->cur_offset);
            c->cur_offset += len;
        }
    }
    c->sem_top = 0; // 清空栈
}

// ========== 递归下降子程序 ==========

// 前向声明
static void PROGRAM(Compiler *c);
static void SUB_PROGRAM(Compiler *c);
static void VARIABLE(Compiler *c);
static void ID_SEQUENCE(Compiler *c);
static void TYPE(Compiler *c);
static void COM_SENTENCE(Compiler *c);
static void STATEMENT(Compiler *c);
static void EVA_SENTENCE(Compiler *c);
static void IF_STATEMENT(Compiler *c);
static void WHILE_STATEMENT(Compiler *c);

// PROGRAM -> program id SUB_PROGRAM .
// * 对应文档: Syntax::PROGRAM()
static void PROGRAM(Compiler *c) {
    if (c->token.code == TOK_PROGRAM) {
        next_token(c);
        if (c->token.code == TOK_ID) {
            // 生成 (program, id, _, _)
            int idx = c->token.value;
            const char *name = (idx >= 0 && idx < c->sym_count) ?
                c->sym_table[idx].name : "?";
            // 将程序标识符标记为程序名
            if (idx >= 0 && idx < c->sym_count) {
                c->sym_table[idx].kind = KIND_PROGRAM;
            }
            next_token(c);
            quad_emit(c, OP_PROGRAM, name, "_", "_");
            SUB_PROGRAM(c);
            if (c->token.code == TOK_DOT) {
                next_token(c);
                quad_emit(c, OP_END, "_", "_", "_");
            } else {
                c->error_flag = 1;
                snprintf(c->error_msg, sizeof(c->error_msg),
                    "语法错误: 期望 '.' 但得到 Token %d", c->token.code);
            }
        } else {
            c->error_flag = 1;
            snprintf(c->error_msg, sizeof(c->error_msg),
                "语法错误: program 后期望标识符");
        }
    } else {
        c->error_flag = 1;
        snprintf(c->error_msg, sizeof(c->error_msg),
            "语法错误: 期望 'program'");
    }
}

// SUB_PROGRAM -> VARIABLE COM_SENTENCE
// * 对应文档: Syntax::SUB_PROGRAM()
static void SUB_PROGRAM(Compiler *c) {
    VARIABLE(c);
    COM_SENTENCE(c);
}

// VARIABLE -> var ID_SEQUENCE : TYPE ;
// *            | ε (空)
// * 对应文档: Syntax::VARIABLE() + 语义动作 a1~a6
static void VARIABLE(Compiler *c) {
    if (c->token.code == TOK_VAR) {
        next_token(c); // 读 var
        semantic_a1(c); // a1: 初始化 offset
        ID_SEQUENCE(c);
        if (c->token.code == TOK_COLON) {
            next_token(c);
            TYPE(c);
            if (c->token.code == TOK_SEMICOLON) {
                semantic_a6(c); // a6: 弹栈, 填写符号表
                next_token(c);
            } else {
                c->error_flag = 1;
                snprintf(c->error_msg, sizeof(c->error_msg),
                    "语法错误: 变量声明后期望 ';'");
            }
        } else {
            c->error_flag = 1;
            snprintf(c->error_msg, sizeof(c->error_msg),
                "语法错误: 标识符表后期望 ':'");
        }
    }
    // 否则 VARIABLE -> ε, 无变量声明
}

// ID_SEQUENCE -> id { , id }
// * 对应文档: Syntax::ID_SEQUENCE() + 语义动作 a2
static void ID_SEQUENCE(Compiler *c) {
    if (c->token.code == TOK_ID) {
        semantic_a2(c); // a2: push(id)
        next_token(c);
        while (c->token.code == TOK_COMMA) {
            next_token(c);
            if (c->token.code == TOK_ID) {
                semantic_a2(c); // a2: push(id)
                next_token(c);
            } else {
                c->error_flag = 1;
                snprintf(c->error_msg, sizeof(c->error_msg),
                    "语法错误: ',' 后期望标识符");
                return;
            }
        }
    } else {
        c->error_flag = 1;
        snprintf(c->error_msg, sizeof(c->error_msg),
            "语法错误: 期望标识符");
    }
}

// TYPE -> integer | real | char
// * 对应文档: Syntax::TYPE() + 语义动作 a3/a4/a5
static void TYPE(Compiler *c) {
    switch (c->token.code) {
        case TOK_INTEGER:
            semantic_a3(c); // a3: type=i, len=4
            next_token(c);
            break;
        case TOK_REAL:
            semantic_a4(c); // a4: type=r, len=8
            next_token(c);
            break;
        case TOK_CHAR:
            semantic_a5(c); // a5: type=c, len=1
            next_token(c);
            break;
        default:
            c->error_flag = 1;
            snprintf(c->error_msg, sizeof(c->error_msg),
                "语法错误: 期望类型名 (integer/real/char)");
    }
}

// COM_SENTENCE -> begin STATEMENT { ; STATEMENT } end
// * 对应文档: Syntax::COM_SENTENCE() + Syntax::SEN_SEQUENCE()
static void COM_SENTENCE(Compiler *c) {
    if (c->token.code == TOK_BEGIN) {
        next_token(c);
        STATEMENT(c);
        while (c->token.code == TOK_SEMICOLON) {
            next_token(c);
            STATEMENT(c);
        }
        if (c->token.code == TOK_END) {
            next_token(c);
        } else {
            c->error_flag = 1;
            snprintf(c->error_msg, sizeof(c->error_msg),
                "语法错误: 期望 'end'");
        }
    } else {
        c->error_flag = 1;
        snprintf(c->error_msg, sizeof(c->error_msg),
            "语法错误: 期望 'begin'");
    }
}

// STATEMENT -> EVA_SENTENCE | IF_STATEMENT | WHILE_STATEMENT
// * 扩展: 支持 IF 和 WHILE
static void STATEMENT(Compiler *c) {
    switch (c->token.code) {
        case TOK_ID:
            EVA_SENTENCE(c);
            break;
        case TOK_IF:
            IF_STATEMENT(c);
            break;
        case TOK_WHILE:
            WHILE_STATEMENT(c);
            break;
        case TOK_WRITE:
            next_token(c);
            if (c->token.code == TOK_ID) {
                int idx = c->token.value;
                const char *id_name = (idx >= 0 && idx < c->sym_count) ?
                    c->sym_table[idx].name : "?";
                quad_emit(c, OP_WRITE, id_name, "_", "_");
                next_token(c);
                // 分号由 COM_SENTENCE 的 while 循环处理
            } else {
                c->error_flag = 1;
                snprintf(c->error_msg, sizeof(c->error_msg),
                    "语法错误: write 后期望标识符");
            }
            break;
        case TOK_BEGIN:
            COM_SENTENCE(c);
            break;
        default:
            // 空语句或错误
            break;
    }
}

// EVA_SENTENCE -> id := EXPRESSION
// * 对应文档: Syntax::EVA_SENTENCE()
// * 语义动作: 生成 (:=, expr_val, _, id)
static void EVA_SENTENCE(Compiler *c) {
    if (c->token.code == TOK_ID) {
        int idx = c->token.value;
        const char *id_name = (idx >= 0 && idx < c->sym_count) ?
            c->sym_table[idx].name : "?";
        next_token(c);
        if (c->token.code == TOK_ASSIGN) {
            next_token(c);
            SemValue sv;
            parse_expression(c, &sv);
            quad_emit(c, OP_ASSIGN, sv.name, "_", id_name);
        } else {
            c->error_flag = 1;
            snprintf(c->error_msg, sizeof(c->error_msg),
                "语法错误: 赋值语句中期望 ':='");
        }
    } else {
        c->error_flag = 1;
        snprintf(c->error_msg, sizeof(c->error_msg),
            "语法错误: 赋值语句中期望标识符");
    }
}

// IF_STATEMENT -> if EXPRESSION then STATEMENT [ else STATEMENT ]
// * 四元式模式:
// *   (jg/... a,b,t)    -- 比较结果
// *   (jnz, t, _, L_t)  -- 条件真跳转到 then
// *   (jmp, _, _, L_f)  -- 跳转到 else/出口
// *   (label, L_t, _, _) -- L_then:
// *   <then body>
// *   (jmp, _, _, L_e)  -- 有 else 时跳过 else
// *   (label, L_f, _, _) -- L_false:
// *   <else body>
// *   (label, L_e, _, _) -- L_end:
static void IF_STATEMENT(Compiler *c) {
    char buf[MAX_STR];

    next_token(c); // 读 if

    // 解析条件表达式
    SemValue cond;
    parse_expression(c, &cond);

    int label_true  = sym_new_label(c);
    int label_false = sym_new_label(c);
    int label_end   = sym_new_label(c);

    // jnz cond _ L_then
    snprintf(buf, MAX_STR, "L%d", label_true);
    quad_emit(c, OP_JNZ, cond.name, "_", buf);

    // jmp _ _ L_false
    snprintf(buf, MAX_STR, "L%d", label_false);
    quad_emit(c, OP_JMP, "_", "_", buf);

    // L_then:
    snprintf(buf, MAX_STR, "L%d", label_true);
    quad_emit(c, OP_LABEL, buf, "_", "_");

    if (c->token.code == TOK_THEN) {
        next_token(c); // 读 then
        STATEMENT(c);

        if (c->token.code == TOK_ELSE) {
            next_token(c); // 读 else

            // then 结束后跳过 else
            snprintf(buf, MAX_STR, "L%d", label_end);
            quad_emit(c, OP_JMP, "_", "_", buf);

            // L_false:
            snprintf(buf, MAX_STR, "L%d", label_false);
            quad_emit(c, OP_LABEL, buf, "_", "_");

            STATEMENT(c); // else 分支

            // L_end:
            snprintf(buf, MAX_STR, "L%d", label_end);
            quad_emit(c, OP_LABEL, buf, "_", "_");
        } else {
            // 无 else: L_false 即为出口
            snprintf(buf, MAX_STR, "L%d", label_false);
            quad_emit(c, OP_LABEL, buf, "_", "_");
        }
    } else {
        c->error_flag = 1;
        snprintf(c->error_msg, sizeof(c->error_msg),
            "语法错误: if 后期望 'then'");
    }
}

// WHILE_STATEMENT -> while EXPRESSION do STATEMENT
// * 四元式模式:
// *   (label, L_loop, _, _)  -- L_loop:
// *   <comparison quads>
// *   (jnz, cond, _, L_body) -- 条件真进入循环体
// *   (jmp, _, _, L_exit)    -- 条件假退出
// *   (label, L_body, _, _)  -- L_body:
// *   <body>
// *   (jmp, _, _, L_loop)    -- 跳回循环入口
// *   (label, L_exit, _, _)  -- L_exit:
static void WHILE_STATEMENT(Compiler *c) {
    char buf[MAX_STR];

    next_token(c); // 读 while

    int label_loop = sym_new_label(c);
    int label_body = sym_new_label(c);
    int label_exit = sym_new_label(c);

    // L_loop:
    snprintf(buf, MAX_STR, "L%d", label_loop);
    quad_emit(c, OP_LABEL, buf, "_", "_");

    // 解析条件表达式
    SemValue cond;
    parse_expression(c, &cond);

    // jnz cond _ L_body
    snprintf(buf, MAX_STR, "L%d", label_body);
    quad_emit(c, OP_JNZ, cond.name, "_", buf);

    // jmp _ _ L_exit
    snprintf(buf, MAX_STR, "L%d", label_exit);
    quad_emit(c, OP_JMP, "_", "_", buf);

    // L_body:
    snprintf(buf, MAX_STR, "L%d", label_body);
    quad_emit(c, OP_LABEL, buf, "_", "_");

    if (c->token.code == TOK_DO) {
        next_token(c); // 读 do
        STATEMENT(c);

        // 跳回循环入口
        snprintf(buf, MAX_STR, "L%d", label_loop);
        quad_emit(c, OP_JMP, "_", "_", buf);

        // L_exit:
        snprintf(buf, MAX_STR, "L%d", label_exit);
        quad_emit(c, OP_LABEL, buf, "_", "_");
    } else {
        c->error_flag = 1;
        snprintf(c->error_msg, sizeof(c->error_msg),
            "语法错误: while 后期望 'do'");
    }
}

// ========== 公开接口 ==========

void parser_init(Compiler *c) {
    (void)c;
    peek_valid = 0;
}

int parser_parse(Compiler *c) {
    c->error_flag = 0;
    c->error_msg[0] = '\0';

    // 先做词法分析, 将 Token 全部读出到序列
    scanner_scan_all(c);
    if (c->error_flag) return 0;

    // 保存词法分析的 token_count (scanner_init 会清零)
    int saved_tok_count = c->token_count;

    // 重置扫描器, 从源程序重新读取做语法分析
    scanner_init(c, c->source);
    peek_valid = 0;
    c->token_count = saved_tok_count; // 恢复 token 计数

    // 初始化符号表和四元式 (保留词法阶段的符号/常数表)
    // 注意: sym_table 和 const_table 保留, 只重置临时计数
    c->temp_count = 0;
    c->label_count = 0;
    c->quad_count = 0;
    c->sem_top = 0;

    // 读取第一个 Token
    next_token(c);

    // 开始递归下降分析
    PROGRAM(c);

    if (c->error_flag) return 0;

    // 检查是否到达文件末尾
    if (c->token.code != TOK_EOF) {
        c->error_flag = 1;
        snprintf(c->error_msg, sizeof(c->error_msg),
            "语法错误: 多余输入 Token %d", c->token.code);
        return 0;
    }

    return 1;
}
