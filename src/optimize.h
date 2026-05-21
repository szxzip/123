#ifndef OPTIMIZE_H
#define OPTIMIZE_H
#include "grammar.h"

// * optimize.c — 中间代码优化 (四元式级别)
// *
// * 实现:
// *   1. 常量折叠: (+ 2 3 t) → (:= 5 _ t)
// *   2. 常量传播: x:=5; y:=x+2 → y:=7
// *   3. 死代码消除: 未使用的临时变量赋值删除
// *
// * 调用: optimize_run(c)  在 parser_parse() 之后、codegen 之前

void optimize_run(Compiler *c);

#endif
