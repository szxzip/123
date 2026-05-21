#ifndef GRAMMAR_H
#define GRAMMAR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ------------- 模块说明 -------------
// * grammar.h — 编译器全局定义
// *   定义 Token 类型、四元式操作码、符号表条目、
// *   关键字表、界符表、自动机状态表等所有共享常量与结构体。
// *   所有其他模块均依赖此文件。对应课程要求中的“文法定义”部分。
// * ---------------------------------

#define MAX_SYMBOLS     256 // 符号表最大容量
#define MAX_CONSTANTS   256 // 常数表最大容量
#define MAX_QUADS       1024 // 四元式最大条数
#define MAX_TOKENS      2048 // Token 序列最大长度
#define MAX_LINE        256 // 源程序单行最大长度
#define MAX_NAME        32 // 标识符最大长度
#define MAX_STR         64 // 通用字符串长度

// ========== Token 类型码 ==========
enum {
    TOK_EOF = 0, // 文件结束
    TOK_ID = 1, // 标识符
    TOK_CONST = 2, // 常数
    // 关键字 (index+3, 与文档中的 Keys 表一致)
    TOK_PROGRAM   = 3,
    TOK_VAR       = 4,
    TOK_INTEGER   = 5,
    TOK_REAL      = 6,
    TOK_CHAR      = 7,
    TOK_BEGIN     = 8,
    TOK_END       = 9,
    // 界符
    TOK_COMMA     = 10,
    TOK_COLON     = 11,
    TOK_SEMICOLON = 12,
    TOK_ASSIGN    = 13, // :=
    TOK_MUL       = 14,
    TOK_DIV       = 15,
    TOK_PLUS      = 16,
    TOK_MINUS     = 17,
    TOK_DOT       = 18,
    TOK_LPAREN    = 19,
    TOK_RPAREN    = 20,
    // 扩展关键字: If/While/逻辑运算
    TOK_IF        = 21,
    TOK_THEN      = 22,
    TOK_ELSE      = 23,
    TOK_WHILE     = 24,
    TOK_DO        = 25,
    TOK_AND       = 26,
    TOK_OR        = 27,
    TOK_NOT       = 28,
    // 比较运算符
    TOK_EQ        = 29, // =
    TOK_LT        = 30, // <
    TOK_GT        = 31, // >
    TOK_LE        = 32, // <=
    TOK_GE        = 33, // >=
    TOK_NE        = 34, // <>
    TOK_WRITE     = 35, // write 输出语句
    TOK_NUM       = 36
};

// ========== 四元式操作码 ==========
enum {
    OP_PROGRAM = 1, // (program, id, _, _)
    OP_ASSIGN  = 2, // (:=, src, _, dst)
    OP_ADD     = 3, // (+, a1, a2, r)
    OP_SUB     = 4, // (-, a1, a2, r)
    OP_MUL     = 5, // (*, a1, a2, r)
    OP_DIV     = 6, // (/, a1, a2, r)
    OP_JMP     = 7, // (jmp, _, _, label)
    OP_JNZ     = 8, // (jnz, cond, _, label)
    OP_JE      = 9, // (je, a1, a2, label)
    OP_JNE     = 10, // (jne, a1, a2, label)
    OP_JL      = 11, // (jl, a1, a2, label)
    OP_JG      = 12, // (jg, a1, a2, label)
    OP_JLE     = 13, // (jle, a1, a2, label)
    OP_JGE     = 14, // (jge, a1, a2, label)
    OP_AND_OP  = 15, // (and, a1, a2, r)
    OP_OR_OP   = 16, // (or, a1, a2, r)
    OP_NOT_OP  = 17, // (not, a1, _, r)
    OP_END     = 18, // (end, _, _, _)
    OP_LABEL   = 19, // (label, _, _, _)
    OP_WRITE   = 20 // (write, id, _, _)
};

// ========== 符号种类 ==========
enum {
    KIND_PROGRAM = 0,
    KIND_VARIABLE = 1,
    KIND_TEMP = 2
};

// ========== 类型码 ==========
enum {
    TY_INTEGER = 0,
    TY_REAL = 1,
    TY_CHAR = 2
};

// ========== Token 结构体 ==========
typedef struct {
    int code; // 单词类别码
    int value; // 符号表/常数表索引
    double real_val; // 实常数数值 (用于 CONST token)
    int int_val; // 整常数值
} Token;

// ========== 符号表条目 ==========
typedef struct {
    char name[MAX_NAME]; // 标识符名
    int type; // TY_INTEGER / TY_REAL / TY_CHAR
    int kind; // KIND_PROGRAM / KIND_VARIABLE / KIND_TEMP
    int offset; // 活动记录偏移
    int len; // 类型宽度
} Symbol;

// ========== 四元式 ==========
typedef struct {
    int op; // 操作码
    char arg1[MAX_STR];
    char arg2[MAX_STR];
    char result[MAX_STR];
} Quadruple;

// ========== 语义值 (用于表达式求值时传递) ==========
typedef struct {
    char name[MAX_NAME];
    int is_temp; // 1=临时变量, 0=普通标识符/常数
} SemValue;

// ========== 全局编译器上下文 ==========
typedef struct {
    // 词法分析
    char *source; // 源程序字符串
    int pos; // 当前读取位置
    int len; // 源程序总长度
    char ch; // 当前字符
    Token token; // 当前 Token
    Token token_list[MAX_TOKENS]; // Token 序列 (词法分析输出)
    int token_count;

    // 符号表
    Symbol sym_table[MAX_SYMBOLS];
    int sym_count;
    double const_table[MAX_CONSTANTS];
    int const_count;

    // 语义栈 (变量声明用)
    int sem_stack[MAX_SYMBOLS];
    int sem_top;
    int cur_type; // 当前声明的类型
    int cur_offset; // 当前偏移

    // 四元式
    Quadruple quads[MAX_QUADS];
    int quad_count;
    int temp_count; // 临时变量计数器
    int label_count; // 标号计数器

    // 错误
    int error_flag;
    char error_msg[256];
} Compiler;

// ---- 以下数据表声明为 extern, 定义在各 .c 文件中 ----

// 关键字/界符统一表 (与文档中的 Keys 表一致)
extern const char *keys_table[];

// 四元式操作码名称 (用于输出)
extern const char *op_names[];

// 类型宽度
static inline int type_len(int type) {
    return (type == TY_INTEGER) ? 4 : (type == TY_REAL) ? 8 : 1;
}

// 常量自动机状态转换矩阵 (定义在 scanner.c)
extern const int const_aut[8][5];

#endif // GRAMMAR_H
