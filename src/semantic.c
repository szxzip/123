#include "semantic.h"
#include "symbol.h"
#include "quadruple.h"

// ========== 翻译文法 a1~a6 ==========

void sem_a1(Compiler *c) {
    c->cur_offset = 0;
}

void sem_a2(Compiler *c) {
    if (c->sem_top < MAX_SYMBOLS)
        c->sem_stack[c->sem_top++] = c->token.value;
}

void sem_a3(Compiler *c) { c->cur_type = TY_INTEGER; }
void sem_a4(Compiler *c) { c->cur_type = TY_REAL; }
void sem_a5(Compiler *c) { c->cur_type = TY_CHAR; }

void sem_a6(Compiler *c) {
    int type = c->cur_type;
    int len = type_len(type);
    for (int i = 0; i < c->sem_top; i++) {
        int idx = c->sem_stack[i];
        if (idx >= 0 && idx < c->sym_count) {
            sym_set_type(c, idx, type, len, c->cur_offset);
            c->cur_offset += len;
        }
    }
    c->sem_top = 0;
}

// ========== 表达式语义 ==========

void sem_emit_binop(Compiler *c, int quad_op, SemValue *left, SemValue *right) {
    char *t = sym_new_temp(c);
    quad_emit(c, quad_op, left->name, right->name, t);
    strncpy(left->name, t, MAX_NAME - 1);
    left->is_temp = 1;
}

void sem_emit_unary_not(Compiler *c, SemValue *sv) {
    char *t = sym_new_temp(c);
    quad_emit(c, OP_NOT_OP, sv->name, "_", t);
    strncpy(sv->name, t, MAX_NAME - 1);
    sv->is_temp = 1;
}

void sem_emit_unary_minus(Compiler *c, SemValue *sv) {
    char *t = sym_new_temp(c);
    quad_emit(c, OP_SUB, "0", sv->name, t);
    strncpy(sv->name, t, MAX_NAME - 1);
    sv->is_temp = 1;
}

void sem_emit_comparison(Compiler *c, int rel_op, SemValue *left, SemValue *right) {
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
    char *t = sym_new_temp(c);
    quad_emit(c, quad_op, left->name, right->name, t);
    strncpy(left->name, t, MAX_NAME - 1);
    left->is_temp = 1;
}

// ========== 赋值 + 输出语义 ==========

void sem_emit_assign(Compiler *c, const char *src, const char *dst) {
    quad_emit(c, OP_ASSIGN, src, "_", dst);
}

void sem_emit_write(Compiler *c, const char *id) {
    quad_emit(c, OP_WRITE, id, "_", "_");
}

// ========== 程序结构语义 ==========

void sem_mark_program(Compiler *c, int idx) {
    if (idx >= 0 && idx < c->sym_count)
        c->sym_table[idx].kind = KIND_PROGRAM;
    const char *name = (idx >= 0 && idx < c->sym_count) ?
        c->sym_table[idx].name : "?";
    quad_emit(c, OP_PROGRAM, name, "_", "_");
}

void sem_emit_end(Compiler *c) {
    quad_emit(c, OP_END, "_", "_", "_");
}

// ========== IF 语句语义 ==========

void sem_if_begin(Compiler *c, SemValue *cond,
                  int *out_true, int *out_false, int *out_end) {
    *out_true  = sym_new_label(c);
    *out_false = sym_new_label(c);
    *out_end   = sym_new_label(c);

    char buf[MAX_STR];
    snprintf(buf, MAX_STR, "L%d", *out_true);
    quad_emit(c, OP_JNZ, cond->name, "_", buf);

    snprintf(buf, MAX_STR, "L%d", *out_false);
    quad_emit(c, OP_JMP, "_", "_", buf);
}

void sem_if_then_label(Compiler *c, int label) {
    char buf[MAX_STR];
    snprintf(buf, MAX_STR, "L%d", label);
    quad_emit(c, OP_LABEL, buf, "_", "_");
}

void sem_if_then_end(Compiler *c, int label_end) {
    char buf[MAX_STR];
    snprintf(buf, MAX_STR, "L%d", label_end);
    quad_emit(c, OP_JMP, "_", "_", buf);
}

void sem_if_false_label(Compiler *c, int label) {
    char buf[MAX_STR];
    snprintf(buf, MAX_STR, "L%d", label);
    quad_emit(c, OP_LABEL, buf, "_", "_");
}

void sem_if_end_label(Compiler *c, int label) {
    char buf[MAX_STR];
    snprintf(buf, MAX_STR, "L%d", label);
    quad_emit(c, OP_LABEL, buf, "_", "_");
}

// ========== WHILE 语句语义 ==========

void sem_while_begin(Compiler *c, int *out_loop, int *out_body, int *out_exit) {
    *out_loop = sym_new_label(c);
    *out_body = sym_new_label(c);
    *out_exit = sym_new_label(c);
}

void sem_while_loop_label(Compiler *c, int label) {
    char buf[MAX_STR];
    snprintf(buf, MAX_STR, "L%d", label);
    quad_emit(c, OP_LABEL, buf, "_", "_");
}

void sem_while_check(Compiler *c, SemValue *cond, int label_body, int label_exit) {
    char buf[MAX_STR];
    snprintf(buf, MAX_STR, "L%d", label_body);
    quad_emit(c, OP_JNZ, cond->name, "_", buf);

    snprintf(buf, MAX_STR, "L%d", label_exit);
    quad_emit(c, OP_JMP, "_", "_", buf);
}

void sem_while_body_label(Compiler *c, int label) {
    char buf[MAX_STR];
    snprintf(buf, MAX_STR, "L%d", label);
    quad_emit(c, OP_LABEL, buf, "_", "_");
}

void sem_while_end(Compiler *c, int label_loop, int label_exit) {
    char buf[MAX_STR];
    snprintf(buf, MAX_STR, "L%d", label_loop);
    quad_emit(c, OP_JMP, "_", "_", buf);

    snprintf(buf, MAX_STR, "L%d", label_exit);
    quad_emit(c, OP_LABEL, buf, "_", "_");
}

// ========== 语义值工具 ==========

void sem_init_sv(SemValue *sv) {
    sv->name[0] = '\0';
    sv->is_temp = 0;
}

void sem_value_from_id(Compiler *c, int idx, SemValue *sv) {
    if (idx >= 0 && idx < c->sym_count)
        strncpy(sv->name, c->sym_table[idx].name, MAX_NAME - 1);
    else
        snprintf(sv->name, MAX_NAME, "id%d", idx);
    sv->is_temp = 0;
}

void sem_value_from_const(Compiler *c, int idx, SemValue *sv) {
    if (idx >= 0 && idx < c->const_count)
        snprintf(sv->name, MAX_NAME, "%g", c->const_table[idx]);
    else
        snprintf(sv->name, MAX_NAME, "%d", idx);
    sv->is_temp = 0;
}
