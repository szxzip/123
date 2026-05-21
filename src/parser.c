#include "parser.h"
#include "scanner.h"
#include "semantic.h"

// ================================================================
// parser.c — 语法分析 (递归下降)
//
// 只负责语法结构识别与错误检查。
// 所有语义动作通过 semantic.h 接口调用。
// ================================================================

static Token peek;
static int peek_valid;

// 读取下一个 Token
static void next_token(Compiler *c) {
    if (peek_valid) {
        c->token = peek;
        peek_valid = 0;
    } else {
        scanner_next_token(c);
    }
}

// ========== 表达式分析 (递归下降, 7级优先级) ==========
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
        sem_emit_binop(c, OP_OR_OP, sv, &right);
    }
}

static void parse_and_expr(Compiler *c, SemValue *sv) {
    parse_not_expr(c, sv);
    while (c->token.code == TOK_AND) {
        next_token(c);
        SemValue right;
        parse_not_expr(c, &right);
        sem_emit_binop(c, OP_AND_OP, sv, &right);
    }
}

static void parse_not_expr(Compiler *c, SemValue *sv) {
    if (c->token.code == TOK_NOT) {
        next_token(c);
        parse_not_expr(c, sv);
        sem_emit_unary_not(c, sv);
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
        sem_emit_comparison(c, rel_op, sv, &right);
    }
}

static void parse_arith_expr(Compiler *c, SemValue *sv) {
    parse_term(c, sv);
    while (c->token.code == TOK_PLUS || c->token.code == TOK_MINUS) {
        int op = c->token.code;
        next_token(c);
        SemValue right;
        parse_term(c, &right);
        int quad_op = (op == TOK_PLUS) ? OP_ADD : OP_SUB;
        sem_emit_binop(c, quad_op, sv, &right);
    }
}

static void parse_term(Compiler *c, SemValue *sv) {
    parse_factor(c, sv);
    while (c->token.code == TOK_MUL || c->token.code == TOK_DIV) {
        int op = c->token.code;
        next_token(c);
        SemValue right;
        parse_factor(c, &right);
        int quad_op = (op == TOK_MUL) ? OP_MUL : OP_DIV;
        sem_emit_binop(c, quad_op, sv, &right);
    }
}

static void parse_factor(Compiler *c, SemValue *sv) {
    if (c->token.code == TOK_ID) {
        sem_value_from_id(c, c->token.value, sv);
        next_token(c);
    } else if (c->token.code == TOK_CONST) {
        sem_value_from_const(c, c->token.value, sv);
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
        next_token(c);
        parse_factor(c, sv);
        sem_emit_unary_minus(c, sv);
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

// ========== 递归下降子程序 ==========

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
static void PROGRAM(Compiler *c) {
    if (c->token.code == TOK_PROGRAM) {
        next_token(c);
        if (c->token.code == TOK_ID) {
            int idx = c->token.value;
            next_token(c);
            sem_mark_program(c, idx);
            SUB_PROGRAM(c);
            if (c->token.code == TOK_DOT) {
                next_token(c);
                sem_emit_end(c);
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
static void SUB_PROGRAM(Compiler *c) {
    VARIABLE(c);
    COM_SENTENCE(c);
}

// VARIABLE -> var ID_SEQUENCE : TYPE ; | ε
static void VARIABLE(Compiler *c) {
    if (c->token.code == TOK_VAR) {
        next_token(c);
        sem_a1(c);
        ID_SEQUENCE(c);
        if (c->token.code == TOK_COLON) {
            next_token(c);
            TYPE(c);
            if (c->token.code == TOK_SEMICOLON) {
                sem_a6(c);
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
}

// ID_SEQUENCE -> id { , id }
static void ID_SEQUENCE(Compiler *c) {
    if (c->token.code == TOK_ID) {
        sem_a2(c);
        next_token(c);
        while (c->token.code == TOK_COMMA) {
            next_token(c);
            if (c->token.code == TOK_ID) {
                sem_a2(c);
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
static void TYPE(Compiler *c) {
    switch (c->token.code) {
        case TOK_INTEGER: sem_a3(c); next_token(c); break;
        case TOK_REAL:    sem_a4(c); next_token(c); break;
        case TOK_CHAR:    sem_a5(c); next_token(c); break;
        default:
            c->error_flag = 1;
            snprintf(c->error_msg, sizeof(c->error_msg),
                "语法错误: 期望类型名 (integer/real/char)");
    }
}

// COM_SENTENCE -> begin STATEMENT { ; STATEMENT } end
static void COM_SENTENCE(Compiler *c) {
    if (c->token.code == TOK_BEGIN) {
        next_token(c);
        STATEMENT(c);
        while (c->token.code == TOK_SEMICOLON) {
            next_token(c);
            STATEMENT(c);
        }
        if (c->token.code == TOK_END)
            next_token(c);
        else {
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

// STATEMENT -> EVA | IF | WHILE | write | begin
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
                sem_emit_write(c, id_name);
                next_token(c);
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
            break;
    }
}

// EVA_SENTENCE -> id := EXPRESSION
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
            sem_emit_assign(c, sv.name, id_name);
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
static void IF_STATEMENT(Compiler *c) {
    next_token(c);

    SemValue cond;
    parse_expression(c, &cond);

    int label_true, label_false, label_end;
    sem_if_begin(c, &cond, &label_true, &label_false, &label_end);

    sem_if_then_label(c, label_true);

    if (c->token.code == TOK_THEN) {
        next_token(c);
        STATEMENT(c);

        if (c->token.code == TOK_ELSE) {
            next_token(c);
            sem_if_then_end(c, label_end);
            sem_if_false_label(c, label_false);
            STATEMENT(c);
            sem_if_end_label(c, label_end);
        } else {
            sem_if_false_label(c, label_false);
        }
    } else {
        c->error_flag = 1;
        snprintf(c->error_msg, sizeof(c->error_msg),
            "语法错误: if 后期望 'then'");
    }
}

// WHILE_STATEMENT -> while EXPRESSION do STATEMENT
static void WHILE_STATEMENT(Compiler *c) {
    next_token(c);

    int label_loop, label_body, label_exit;
    sem_while_begin(c, &label_loop, &label_body, &label_exit);

    sem_while_loop_label(c, label_loop);

    SemValue cond;
    parse_expression(c, &cond);

    sem_while_check(c, &cond, label_body, label_exit);
    sem_while_body_label(c, label_body);

    if (c->token.code == TOK_DO) {
        next_token(c);
        STATEMENT(c);
        sem_while_end(c, label_loop, label_exit);
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

    scanner_scan_all(c);
    if (c->error_flag) return 0;

    int saved_tok_count = c->token_count;

    scanner_init(c, c->source);
    peek_valid = 0;
    c->token_count = saved_tok_count;

    c->temp_count = 0;
    c->label_count = 0;
    c->quad_count = 0;
    c->sem_top = 0;

    next_token(c);
    PROGRAM(c);

    if (c->error_flag) return 0;

    if (c->token.code != TOK_EOF) {
        c->error_flag = 1;
        snprintf(c->error_msg, sizeof(c->error_msg),
            "语法错误: 多余输入 Token %d", c->token.code);
        return 0;
    }

    return 1;
}
