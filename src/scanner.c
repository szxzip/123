#include "scanner.h"
#include "symbol.h"
#include <math.h>

// ---- 数据表定义 (在 grammar.h 中声明为 extern) ----

// 关键字/界符统一表 (与课程文档 Keys 表一致, code=index+3)
const char *keys_table[] = {
    "program", "var", "integer", "real", "char", "begin", "end",
    ",", ":", ";", ":=", "*", "/", "+", "-", ".", "(", ")",
    "if", "then", "else", "while", "do",
    "and", "or", "not",
    "=", "<", ">", "<=", ">=", "<>",
    "write",
    NULL
};

// Pascal 常数自动机: 状态转换矩阵
// * 行=状态-1, 列: digit(0)/dot(1)/Ee(2)/+-(3)/other(4)
// * 状态 1~8: 1=初态, 2=整数, 3=刚读., 4=小数, 5=刚读E, 6=刚读符号, 7=指数, 8=接受
const int const_aut[8][5] = {
    {2, 0, 0, 0, 0},
    {2, 3, 5, 8, 8},
    {4, 0, 0, 0, 0},
    {4, 0, 5, 8, 8},
    {7, 0, 0, 6, 0},
    {7, 0, 0, 0, 0},
    {7, 0, 0, 8, 8},
    {0, 0, 0, 0, 0}
};

// 获取下一字符
static void next_char(Compiler *c) {
    if (c->pos < c->len)
        c->ch = c->source[c->pos++];
    else
        c->ch = '\0';
}

// 跳过空白
static void skip_whitespace(Compiler *c) {
    while (c->ch == ' ' || c->ch == '\t' || c->ch == '\n' || c->ch == '\r')
        next_char(c);
}

// 判断字母
static int is_letter(char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

// 判断数字
static int is_digit(char ch) {
    return (ch >= '0' && ch <= '9');
}

// 查关键字/界符表, 返回 code (index+3), 未找到返回 0
static int reserve(const char *str) {
    int i;
    for (i = 0; keys_table[i] != NULL; i++) {
        if (strcmp(keys_table[i], str) == 0)
            return i + 3;
    }
    return 0;
}

// ========== 常数自动机 (Pascal 常数处理机) ==========
// * 对应文档中的 PascalCons 类, 实现整数、实数、科学计数法识别
static double constant_automaton(Compiler *c) {
    // 状态 1-8, 映射为 aut[state-1][col]
    int s = 1; // 当前状态
    int n = 0, m = 0; // 尾数值, 小数位数
    int p = 0, e = 1; // 指数值, 指数符号
    double num = 0;
    int col;

    while (1) {
        // 确定列: digit=0, dot=1, E/e=2, +/-=3, other=4
        if (is_digit(c->ch))
            col = 0;
        else if (c->ch == '.')
            col = 1;
        else if (c->ch == 'E' || c->ch == 'e')
            col = 2;
        else if (c->ch == '+' || c->ch == '-')
            col = 3;
        else
            col = 4;

        s = const_aut[s - 1][col];
        if (s == 0) break;

        // 语义动作
        switch (s) {
            case 1: n = 0; m = 0; p = 0; e = 1; num = 0; break;
            case 2: n = 10 * n + (c->ch - '0'); break; // 整数部分
            case 3: break; // 读到小数点
            case 4: n = 10 * n + (c->ch - '0'); m++; break; // 小数部分
            case 5: break; // 读到 E
            case 6: if (c->ch == '-') e = -1; break; // 指数符号
            case 7: p = 10 * p + (c->ch - '0'); break; // 指数值
            case 8: num = n * pow(10, e * p - m); break; // 计算终值
        }

        if (s == 8) break; // 接受态, 结束
        next_char(c);
    }

    if (s == 8)
        return num;
    else {
        c->error_flag = 1;
        snprintf(c->error_msg, sizeof(c->error_msg), "常数格式错误");
        return 0;
    }
}

// 初始化扫描器
void scanner_init(Compiler *c, const char *source) {
    c->source = (char *)source;
    c->len = strlen(source);
    c->pos = 0;
    c->token_count = 0;
    next_char(c);
}

// 读取下一个 Token
void scanner_next_token(Compiler *c) {
    char str_token[MAX_NAME];
    int i_str;

    skip_whitespace(c);

    if (c->ch == '\0') {
        c->token.code = TOK_EOF;
        c->token.value = -1;
        return;
    }

    // ---- 标识符或关键字: 字母开头, 后跟字母或数字 ----
    if (is_letter(c->ch)) {
        i_str = 0;
        while (is_letter(c->ch) || is_digit(c->ch)) {
            if (i_str < MAX_NAME - 1)
                str_token[i_str++] = c->ch;
            next_char(c);
        }
        str_token[i_str] = '\0';

        c->token.code = reserve(str_token);
        if (c->token.code == 0) {
            // 是标识符
            c->token.code = TOK_ID;
            c->token.value = sym_enter_id(c, str_token, TY_INTEGER, KIND_VARIABLE, 0);
        } else {
            c->token.value = -1;
        }
        return;
    }

    // ---- 常数: 数字开头 ----
    if (is_digit(c->ch)) {
        double val = constant_automaton(c);
        if (!c->error_flag) {
            c->token.code = TOK_CONST;
            c->token.value = sym_enter_const(c, val);
            c->token.real_val = val;
            c->token.int_val = (int)val;
        }
        return;
    }

    // ---- 界符处理 ----
    i_str = 0;
    str_token[i_str++] = c->ch;

    // 双界符: :=  <=  >=  <>
    if (c->ch == ':') {
        next_char(c);
        if (c->ch == '=') {
            str_token[i_str++] = c->ch;
            next_char(c);
        }
    } else if (c->ch == '<') {
        next_char(c);
        if (c->ch == '=' || c->ch == '>') {
            str_token[i_str++] = c->ch;
            next_char(c);
        }
    } else if (c->ch == '>') {
        next_char(c);
        if (c->ch == '=') {
            str_token[i_str++] = c->ch;
            next_char(c);
        }
    } else {
        next_char(c);
    }

    str_token[i_str] = '\0';
    c->token.code = reserve(str_token);
    if (c->token.code == 0) {
        c->error_flag = 1;
        snprintf(c->error_msg, sizeof(c->error_msg),
            "未识别的符号: '%c' (0x%02X)", str_token[0], (unsigned char)str_token[0]);
    }
    c->token.value = -1;
}

// 扫描全部 Token 到序列中 (词法分析阶段输出)
void scanner_scan_all(Compiler *c) {
    c->token_count = 0;
    scanner_init(c, c->source);
    do {
        scanner_next_token(c);
        if (c->token_count < MAX_TOKENS - 1 && !c->error_flag)
            c->token_list[c->token_count++] = c->token;
    } while (c->token.code != TOK_EOF && !c->error_flag);
    // 末尾添加 EOF
    if (c->token_count < MAX_TOKENS - 1 && !c->error_flag) {
        c->token_list[c->token_count].code = TOK_EOF;
        c->token_list[c->token_count].value = -1;
    }
}

// 输出 Token 序列
void scanner_dump_tokens(Compiler *c, char *buf, int bufsize) {
    int i, pos = 0;
    pos += snprintf(buf + pos, bufsize - pos,
        "========== Token 序列 (词法分析输出) ==========\n");
    for (i = 0; i < c->token_count; i++) {
        Token *t = &c->token_list[i];
        if (t->code == TOK_EOF) break;
        if (t->code == TOK_ID)
            pos += snprintf(buf + pos, bufsize - pos,
                "(k,1)(i,%d) ", t->value);
        else if (t->code == TOK_CONST)
            pos += snprintf(buf + pos, bufsize - pos,
                "(c,%d) ", t->value);
        else
            pos += snprintf(buf + pos, bufsize - pos,
                "(p,%d) ", t->code);
        if ((i + 1) % 8 == 0)
            pos += snprintf(buf + pos, bufsize - pos, "\n");
    }
    pos += snprintf(buf + pos, bufsize - pos, "\n");
}
