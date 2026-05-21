#include "symbol.h"
#include <math.h>

// 类型名称 (在 grammar.h 中声明为 extern)
const char *type_names[] = { "integer", "real", "char" };

// 初始化符号表
void sym_init(Compiler *c) {
    c->sym_count = 0;
    c->const_count = 0;
    c->temp_count = 0;
    c->label_count = 0;
    c->sem_top = 0;
    c->cur_type = TY_INTEGER;
    c->cur_offset = 0;
    memset(c->sym_table, 0, sizeof(c->sym_table));
    memset(c->const_table, 0, sizeof(c->const_table));
    memset(c->sem_stack, 0, sizeof(c->sem_stack));
}

// 填写符号表: 查到了返回索引, 否则插入尾部
int sym_enter_id(Compiler *c, const char *name, int type, int kind, int offset) {
    int i;
    // 先查找
    for (i = 0; i < c->sym_count; i++) {
        if (strcmp(c->sym_table[i].name, name) == 0)
            return i;
    }
    // 未找到, 插入
    if (c->sym_count >= MAX_SYMBOLS) return -1;
    i = c->sym_count++;
    strncpy(c->sym_table[i].name, name, MAX_NAME - 1);
    c->sym_table[i].type = type;
    c->sym_table[i].kind = kind;
    c->sym_table[i].offset = offset;
    c->sym_table[i].len = type_len(type);
    return i;
}

// 查找标识符, 返回索引, 未找到返回 -1
int sym_lookup_id(Compiler *c, const char *name) {
    int i;
    for (i = 0; i < c->sym_count; i++) {
        if (strcmp(c->sym_table[i].name, name) == 0)
            return i;
    }
    return -1;
}

// 设置符号类型信息 (用于变量声明中的 a3/a4/a5 语义动作)
void sym_set_type(Compiler *c, int index, int type, int len, int offset) {
    if (index < 0 || index >= c->sym_count) return;
    c->sym_table[index].type = type;
    c->sym_table[index].len = len;
    c->sym_table[index].offset = offset;
}

// 常数表: 查到了返回索引, 否则插入
int sym_enter_const(Compiler *c, double val) {
    int i;
    for (i = 0; i < c->const_count; i++) {
        if (fabs(c->const_table[i] - val) < 1e-12)
            return i;
    }
    if (c->const_count >= MAX_CONSTANTS) return -1;
    i = c->const_count++;
    c->const_table[i] = val;
    return i;
}

// 查找常数
int sym_lookup_const(Compiler *c, double val) {
    int i;
    for (i = 0; i < c->const_count; i++) {
        if (fabs(c->const_table[i] - val) < 1e-12)
            return i;
    }
    return -1;
}

// 分配临时变量, 返回名称 (如 "t1", "t2")
char *sym_new_temp(Compiler *c) {
    static char buf[MAX_NAME];
    c->temp_count++;
    snprintf(buf, MAX_NAME, "t%d", c->temp_count);
    // 在符号表中登记临时变量
    sym_enter_id(c, buf, c->cur_type, KIND_TEMP, c->cur_offset);
    return buf;
}

// 分配新标号, 返回编号
int sym_new_label(Compiler *c) {
    return ++c->label_count;
}

// 输出符号表
void sym_dump(Compiler *c, char *buf, int bufsize) {
    int i, pos = 0;
    pos += snprintf(buf + pos, bufsize - pos,
        "========== 符号表 (Symbol Table) ==========\n"
        "%-6s %-16s %-8s %-8s %-6s %s\n",
        "索引", "名字", "种类", "类型", "偏移", "宽度");
    pos += snprintf(buf + pos, bufsize - pos,
        "------ ---------------- -------- -------- ------ ----\n");
    for (i = 0; i < c->sym_count; i++) {
        Symbol *s = &c->sym_table[i];
        const char *kind_str = (s->kind == KIND_PROGRAM) ? "程序" :
                               (s->kind == KIND_VARIABLE) ? "变量" : "临时";
        pos += snprintf(buf + pos, bufsize - pos,
            "%-6d %-16s %-8s %-8s %-6d %d\n",
            i, s->name, kind_str, type_names[s->type], s->offset, s->len);
    }
}

// 输出常数表
void const_dump(Compiler *c, char *buf, int bufsize) {
    int i, pos = 0;
    pos += snprintf(buf + pos, bufsize - pos,
        "\n========== 常数表 (Constant Table) ==========\n"
        "%-6s %s\n", "索引", "值");
    for (i = 0; i < c->const_count; i++) {
        pos += snprintf(buf + pos, bufsize - pos,
            "%-6d %g\n", i, c->const_table[i]);
    }
}
