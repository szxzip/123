#include "optimize.h"
#include <stdlib.h>

// 判断字符串是否为纯数字常数 (不含小数点)
static int is_int_const(const char *s) {
    if (!s || s[0] == '\0' || s[0] == '_') return 0;
    if (s[0] == '-') s++;
    if (s[0] == '\0') return 0;
    for (; *s; s++)
        if (*s < '0' || *s > '9') return 0;
    return 1;
}

// 解析整常数值
static int const_val(const char *s) {
    return atoi(s);
}

// ========== 1. 常量折叠 ==========
// * 扫描四元式序列，对纯常数运算直接计算结果
// *   (* 2 5 t1)  →  (:= 10 _ t1)
// *   (+ 3 4 t2)  →  (:= 7 _ t2)
// *   (- 10 3 t3) →  (:= 7 _ t3)
// *   (/ 8 2 t4)  →  (:= 4 _ t4)
static int fold_constants(Compiler *c) {
    int changed = 0;
    for (int i = 0; i < c->quad_count; i++) {
        Quadruple *q = &c->quads[i];
        if (q->op == OP_ADD || q->op == OP_SUB ||
            q->op == OP_MUL || q->op == OP_DIV) {
            if (is_int_const(q->arg1) && is_int_const(q->arg2)) {
                int a = const_val(q->arg1);
                int b = const_val(q->arg2);
                int r = 0;
                switch (q->op) {
                    case OP_ADD: r = a + b; break;
                    case OP_SUB: r = a - b; break;
                    case OP_MUL: r = a * b; break;
                    case OP_DIV:
                        if (b == 0) continue;
                        r = a / b;
                        break;
                }
                // 替换为赋值四元式
                q->op = OP_ASSIGN;
                snprintf(q->arg1, MAX_STR, "%d", r);
                q->arg2[0] = '_'; q->arg2[1] = '\0';
                changed = 1;
            }
        }
    }
    return changed;
}

// ========== 2. 常量传播 ==========
// * x := 常数 ;  ... ; y := x op z   →  y := 常数 op z
// * x := 常数 ;  ... ; y := z op x   →  y := z op 常数
static int propagate_constants(Compiler *c) {
    int changed = 0;
    int i, j;
    for (i = 0; i < c->quad_count; i++) {
        Quadruple *q = &c->quads[i];
        // 找到常数赋值: (:= C _ var)
        if (q->op == OP_ASSIGN && is_int_const(q->arg1) &&
            q->arg2[0] == '_' && !is_int_const(q->result)) {
            const char *var = q->result;
            const char *val = q->arg1;
            // 向后搜索使用该变量的运算
            for (j = i + 1; j < c->quad_count; j++) {
                Quadruple *u = &c->quads[j];
                // 如果变量被重新赋值, 停止传播
                if (u->op == OP_ASSIGN && strcmp(u->result, var) == 0)
                    break;
                // 如果变量被用作操作数, 替换为常数
                if (u->op == OP_ADD || u->op == OP_SUB ||
                    u->op == OP_MUL || u->op == OP_DIV) {
                    if (strcmp(u->arg1, var) == 0) {
                        snprintf(u->arg1, MAX_STR, "%s", val);
                        changed = 1;
                    }
                    if (strcmp(u->arg2, var) == 0) {
                        snprintf(u->arg2, MAX_STR, "%s", val);
                        changed = 1;
                    }
                }
                // write 使用也算
                if (u->op == OP_WRITE) break;
                if (u->op == OP_END) break;
                if (u->op == OP_PROGRAM) continue;
            }
        }
    }
    return changed;
}

// ========== 3. 死代码消除 ==========
// * 删除无用的赋值: 赋值结果从未被使用 → 删掉
// * 标记每个临时变量的使用次数, 删除使用次数为 0 的赋值
static int eliminate_dead_code(Compiler *c) {
    int i, j;
    // 统计每个变量的使用次数 (arg1, arg2, write)
    int use_count[MAX_SYMBOLS] = {0};
    for (i = 0; i < c->quad_count; i++) {
        Quadruple *q = &c->quads[i];
        if (q->op == OP_WRITE) {
            // write 使用 arg1
            for (j = 0; j < c->sym_count; j++)
                if (strcmp(c->sym_table[j].name, q->arg1) == 0)
                    use_count[j]++;
        }
        // 运算使用 arg1 和 arg2
        if (q->op >= OP_ADD && q->op <= OP_NOT_OP) {
            for (j = 0; j < c->sym_count; j++) {
                if (strcmp(c->sym_table[j].name, q->arg1) == 0)
                    use_count[j]++;
                if (strcmp(c->sym_table[j].name, q->arg2) == 0)
                    use_count[j]++;
            }
        }
        // := 使用 arg1 (源操作数)
        if (q->op == OP_ASSIGN) {
            for (j = 0; j < c->sym_count; j++)
                if (strcmp(c->sym_table[j].name, q->arg1) == 0)
                    use_count[j]++;
        }
        // 跳转使用 cond
        if (q->op == OP_JNZ) {
            for (j = 0; j < c->sym_count; j++)
                if (strcmp(c->sym_table[j].name, q->arg1) == 0)
                    use_count[j]++;
        }
    }
    // 删除无用赋值 (临时变量且使用次数 0)
    int removed = 0;
    for (i = 0; i < c->quad_count; i++) {
        Quadruple *q = &c->quads[i];
        if (q->op == OP_ASSIGN) {
            int idx = -1;
            for (j = 0; j < c->sym_count; j++)
                if (strcmp(c->sym_table[j].name, q->result) == 0)
                    { idx = j; break; }
            if (idx >= 0 && use_count[idx] == 0 &&
                c->sym_table[idx].kind == KIND_TEMP) {
                // 标记删除
                q->op = -1;
                removed++;
            }
        }
    }
    // 压缩: 移除标记为 -1 的四元式
    if (removed > 0) {
        int write = 0;
        for (i = 0; i < c->quad_count; i++) {
            if (c->quads[i].op != -1) {
                if (write != i)
                    c->quads[write] = c->quads[i];
                write++;
            }
        }
        c->quad_count = write;
        return 1;
    }
    return 0;
}

// ========== 主优化入口 ==========
void optimize_run(Compiler *c) {
    int pass;
    for (pass = 0; pass < 5; pass++) {
        int changed = 0;
        changed |= propagate_constants(c);
        changed |= fold_constants(c);
        changed |= eliminate_dead_code(c); // 删无用代码
        if (!changed) break;
    }
    printf("优化完成 (%d 遍)\n", pass + 1);
}
