#ifndef SYMBOL_H
#define SYMBOL_H
#include "grammar.h"

/* ========== 模块说明 ==========
 * symbol.c/h — 符号表与常数表管理
 *   对应课程要求: 符号表系统（活动记录、主表、常数表）
 *   功能:
 *     - 标识符查填 (enter_id / lookup_id)
 *     - 常数查填 (enter_const / lookup_const)
 *     - 临时变量分配 (new_temp)
 *     - 符号表打印
 * ================================ */

void sym_init(Compiler *c);
int  sym_enter_id(Compiler *c, const char *name, int type, int kind, int offset);
int  sym_lookup_id(Compiler *c, const char *name);
void sym_set_type(Compiler *c, int index, int type, int len, int offset);
int  sym_enter_const(Compiler *c, double val);
int  sym_lookup_const(Compiler *c, double val);
char *sym_new_temp(Compiler *c);
int  sym_new_label(Compiler *c);
void sym_dump(Compiler *c, char *buf, int bufsize);
void const_dump(Compiler *c, char *buf, int bufsize);

#endif
