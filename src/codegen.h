#ifndef CODEGEN_H
#define CODEGEN_H
#include "grammar.h"

/*
 * codegen.h  --  x86-64 Linux target code generation (AT&T asm, GCC/GAS)
 *
 * Quadruple to asm mapping:
 *   program      -> .globl main / push rbp / ...
 *   assign       -> mov src, dst
 *   add/sub/etc  -> mov a1,rax; op a2,rax; mov rax,r
 *   jmp          -> jmp L
 *   jnz          -> cmpl $0,t; jne L
 *   comparison   -> cmp a2,a1; setX al; movzbl al,eax; mov eax,t
 *   write        -> mov id,esi; lea fmt(rip),rdi; xor eax,eax; call printf
 *   end          -> xor eax,eax; leave; ret
 *
 * Output: assemblable .s file, run with: gcc -no-pie x.s -o x
 */

void codegen_generate(Compiler *c, const char *outfile);

#endif
