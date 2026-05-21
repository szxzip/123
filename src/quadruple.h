#ifndef QUADRUPLE_H
#define QUADRUPLE_H
#include "grammar.h"

// ========== 模块说明 ==========
// * quadruple.c/h — 四元式序列管理
// *   对应课程要求: 中间代码设计 (四元式)
// *   功能:
// *     - quad_emit:   生成一条四元式
// *     - quad_dump:   输出四元式序列
// *     - quad_backpatch: 回填跳转目标
// * ================================

void quad_init(Compiler *c);
int  quad_emit(Compiler *c, int op, const char *a1, const char *a2, const char *r);
void quad_dump(Compiler *c, char *buf, int bufsize);
void quad_backpatch(Compiler *c, int quad_idx, const char *label);

#endif
