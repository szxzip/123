#ifndef PARSER_H
#define PARSER_H
#include "grammar.h"

// * parser.h -- syntax and semantic analysis
// *
// * Parsing strategy:
// *   - Statements: recursive descent (PROGRAM, SUB_PROGRAM, VARIABLE,
// *     ID_SEQUENCE, TYPE, COM_SENTENCE, STATEMENT, EVA_SENTENCE,
// *     IF_STATEMENT, WHILE_STATEMENT)
// *   - Expressions: recursive descent (7 precedence levels)
// *
// * Semantic actions (translation grammar):
// *   - VAR declaration: a1-a6 semantic routines
// *   - Assignment: generate (:=, src, _, dst)
// *   - Expression: generate arithmetic/logic quadruples
// *   - IF/WHILE: generate jump quadruples + backpatching

void parser_init(Compiler *c);
int  parser_parse(Compiler *c);

#endif
