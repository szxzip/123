#ifndef SEMANTIC_H
#define SEMANTIC_H
#include "grammar.h"

// ========== 翻译文法 a1~a6 (声明语句语义) ==========
void sem_a1(Compiler *c);
void sem_a2(Compiler *c);
void sem_a3(Compiler *c);
void sem_a4(Compiler *c);
void sem_a5(Compiler *c);
void sem_a6(Compiler *c);

// ========== 表达式语义 ==========
void sem_emit_binop(Compiler *c, int quad_op, SemValue *left, SemValue *right);
void sem_emit_unary_not(Compiler *c, SemValue *sv);
void sem_emit_unary_minus(Compiler *c, SemValue *sv);
void sem_emit_comparison(Compiler *c, int rel_op, SemValue *left, SemValue *right);

// ========== 赋值 + 输出语义 ==========
void sem_emit_assign(Compiler *c, const char *src, const char *dst);
void sem_emit_write(Compiler *c, const char *id);

// ========== 程序结构语义 ==========
void sem_mark_program(Compiler *c, int idx);
void sem_emit_end(Compiler *c);

// ========== IF 语句语义 ==========
void sem_if_begin(Compiler *c, SemValue *cond,
                  int *out_true, int *out_false, int *out_end);
void sem_if_then_label(Compiler *c, int label);
void sem_if_then_end(Compiler *c, int label_end);
void sem_if_false_label(Compiler *c, int label);
void sem_if_end_label(Compiler *c, int label);

// ========== WHILE 语句语义 ==========
void sem_while_begin(Compiler *c, int *out_loop, int *out_body, int *out_exit);
void sem_while_loop_label(Compiler *c, int label);
void sem_while_check(Compiler *c, SemValue *cond, int label_body, int label_exit);
void sem_while_body_label(Compiler *c, int label);
void sem_while_end(Compiler *c, int label_loop, int label_exit);

// ========== 语义值工具 ==========
void sem_init_sv(SemValue *sv);
void sem_value_from_id(Compiler *c, int idx, SemValue *sv);
void sem_value_from_const(Compiler *c, int idx, SemValue *sv);

#endif
