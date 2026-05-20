#include "quadruple.h"

/* 四元式操作码名称 (在 grammar.h 中声明为 extern) */
const char *op_names[] = {
    "", "program", ":=", "+", "-", "*", "/",
    "jmp", "jnz", "je", "jne", "jl", "jg", "jle", "jge",
    "and", "or", "not", "end", "label", "write"
};

/* 初始化四元式序列 */
void quad_init(Compiler *c) {
    c->quad_count = 0;
    c->temp_count = 0;
    c->label_count = 0;
    memset(c->quads, 0, sizeof(c->quads));
}

/* 生成一条四元式, 返回四元式序号 */
int quad_emit(Compiler *c, int op, const char *a1, const char *a2, const char *r) {
    if (c->quad_count >= MAX_QUADS) return -1;
    Quadruple *q = &c->quads[c->quad_count];
    q->op = op;
    snprintf(q->arg1, MAX_STR, "%s", (a1 && a1[0]) ? a1 : "_");
    snprintf(q->arg2, MAX_STR, "%s", (a2 && a2[0]) ? a2 : "_");
    snprintf(q->result, MAX_STR, "%s", (r && r[0]) ? r : "_");
    return c->quad_count++;
}

/* 输出四元式序列 */
void quad_dump(Compiler *c, char *buf, int bufsize) {
    int i, pos = 0;
    pos += snprintf(buf + pos, bufsize - pos,
        "========== 四元式序列 (中间代码) ==========\n"
        "%-4s %-10s %-8s %-8s %s\n",
        "序号", "操作", "arg1", "arg2", "result");
    pos += snprintf(buf + pos, bufsize - pos,
        "---- ---------- -------- -------- --------\n");
    for (i = 0; i < c->quad_count; i++) {
        Quadruple *q = &c->quads[i];
        const char *op_name = (q->op > 0 && q->op <= 20) ? op_names[q->op] : "?";
        pos += snprintf(buf + pos, bufsize - pos,
            "%-4d %-10s %-8s %-8s %s\n",
            i + 1, op_name, q->arg1, q->arg2, q->result);
    }
}

/* 回填: 将指定四元式的 result 字段替换为目标标号 */
void quad_backpatch(Compiler *c, int quad_idx, const char *label) {
    if (quad_idx >= 0 && quad_idx < c->quad_count) {
        snprintf(c->quads[quad_idx].result, MAX_STR, "%s", label);
    }
}
