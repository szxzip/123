#ifndef SCANNER_H
#define SCANNER_H
#include "grammar.h"

/* ========== 模块说明 ==========
 * scanner.c/h — 词法分析器
 *   对应课程要求: 扫描器设计实现
 *   功能:
 *     - InitScanner: 初始化, 加载源程序
 *     - NextToken:   读取下一个 Token
 *     - ScanAll:     扫描全部 Token (词法分析阶段输出)
 *   实现技术:
 *     - 关键字/界符: 查 keys_table
 *     - 标识符:      DFA (字母开头, 后跟字母数字)
 *     - 常数:        Pascal 常数自动机 (整数、实数、科学计数法)
 *     - 双界符:      :=  <=  >=  <>
 * ================================ */

void scanner_init(Compiler *c, const char *source);
void scanner_next_token(Compiler *c);
void scanner_scan_all(Compiler *c);
void scanner_dump_tokens(Compiler *c, char *buf, int bufsize);

#endif
