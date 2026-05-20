#include "codegen.h"

/* 判断操作数是否为常数 (字符串以数字或负号开头) */
static int is_const_operand(const char *s) {
    if (!s || s[0] == '\0' || s[0] == '_') return 0;
    if (s[0] == '-' && s[1] >= '0' && s[1] <= '9') return 1;
    return (s[0] >= '0' && s[0] <= '9');
}

/* 将操作数字符串写入为汇编操作数
 * 常数 → $value ; 变量 → varname(%rip) */
static void write_cmp_set(FILE *f, const char *set_instr,
                          const char *a1, const char *a2, const char *r) {
    /* 将 a1 加载到 %eax */
    if (is_const_operand(a1)) {
        fprintf(f, "    movl    $%s, %%eax\n", a1);
    } else {
        fprintf(f, "    movl    %s(%%rip), %%eax\n", a1);
    }
    /* 与 a2 比较 */
    if (is_const_operand(a2)) {
        fprintf(f, "    cmpl    $%s, %%eax\n", a2);
    } else {
        fprintf(f, "    cmpl    %s(%%rip), %%eax\n", a2);
    }
    fprintf(f, "    %s    %%al\n", set_instr);
    fprintf(f, "    movzbl  %%al, %%eax\n");
    fprintf(f, "    movl    %%eax, %s(%%rip)\n", r);
}

void codegen_generate(Compiler *c, const char *outfile) {
    FILE *f = fopen(outfile, "w");
    if (!f) {
        fprintf(stderr, "无法写入文件: %s\n", outfile);
        return;
    }

    /* ---- 数据段: 格式串与变量 ---- */
    fprintf(f, "    .section .rodata\n");
    fprintf(f, ".fmt:\n");
    fprintf(f, "    .string \"%%d\\n\"\n\n");

    /* 收集需要在 .bss 中分配的变量 (kind != KIND_PROGRAM) */
    fprintf(f, "    .section .bss\n");
    int i, max_offset = 0;
    for (i = 0; i < c->sym_count; i++) {
        Symbol *s = &c->sym_table[i];
        if (s->kind != KIND_PROGRAM) {
            fprintf(f, "%s:\n", s->name);
            fprintf(f, "    .zero %d\n", s->len);
            if (s->offset + s->len > max_offset)
                max_offset = s->offset + s->len;
        }
    }
    fprintf(f, "\n");

    /* ---- 代码段 ---- */
    fprintf(f, "    .section .text\n");
    fprintf(f, "    .globl  main\n");
    fprintf(f, "    .type   main, @function\n");
    fprintf(f, "main:\n");
    fprintf(f, "    pushq   %%rbp\n");
    fprintf(f, "    movq    %%rsp, %%rbp\n\n");

    /* 遍历四元式, 生成汇编 */
    for (i = 0; i < c->quad_count; i++) {
        Quadruple *q = &c->quads[i];
        const char *a1 = q->arg1;
        const char *a2 = q->arg2;
        const char *r  = q->result;

        fprintf(f, "    # %-6s %-6s %-6s %s\n",
                op_names[q->op], a1, a2, r);

        switch (q->op) {

        case OP_PROGRAM:
            /* 无需额外代码, main 入口已在上面 */
            break;

        case OP_ASSIGN:
            /* (:=, src, _, dst)
             * movl src, %eax ; movl %eax, dst */
            if (is_const_operand(a1)) {
                fprintf(f, "    movl    $%s, %%eax\n", a1);
            } else {
                fprintf(f, "    movl    %s(%%rip), %%eax\n", a1);
            }
            fprintf(f, "    movl    %%eax, %s(%%rip)\n", r);
            break;

        case OP_ADD:
            if (is_const_operand(a1)) {
                fprintf(f, "    movl    $%s, %%eax\n", a1);
            } else {
                fprintf(f, "    movl    %s(%%rip), %%eax\n", a1);
            }
            if (is_const_operand(a2)) {
                fprintf(f, "    addl    $%s, %%eax\n", a2);
            } else {
                fprintf(f, "    addl    %s(%%rip), %%eax\n", a2);
            }
            fprintf(f, "    movl    %%eax, %s(%%rip)\n", r);
            break;

        case OP_SUB:
            if (is_const_operand(a1)) {
                fprintf(f, "    movl    $%s, %%eax\n", a1);
            } else {
                fprintf(f, "    movl    %s(%%rip), %%eax\n", a1);
            }
            if (is_const_operand(a2)) {
                fprintf(f, "    subl    $%s, %%eax\n", a2);
            } else {
                fprintf(f, "    subl    %s(%%rip), %%eax\n", a2);
            }
            fprintf(f, "    movl    %%eax, %s(%%rip)\n", r);
            break;

        case OP_MUL:
            if (is_const_operand(a1)) {
                fprintf(f, "    movl    $%s, %%eax\n", a1);
            } else {
                fprintf(f, "    movl    %s(%%rip), %%eax\n", a1);
            }
            if (is_const_operand(a2)) {
                fprintf(f, "    imull   $%s, %%eax, %%eax\n", a2);
            } else {
                fprintf(f, "    imull   %s(%%rip), %%eax\n", a2);
            }
            fprintf(f, "    movl    %%eax, %s(%%rip)\n", r);
            break;

        case OP_DIV:
            if (is_const_operand(a1)) {
                fprintf(f, "    movl    $%s, %%eax\n", a1);
            } else {
                fprintf(f, "    movl    %s(%%rip), %%eax\n", a1);
            }
            fprintf(f, "    cltd\n");         /* 符号扩展 %eax → %edx:%eax */
            if (is_const_operand(a2)) {
                /* idiv 不支持立即数, 需要先 mov 到寄存器 */
                fprintf(f, "    movl    $%s, %%ecx\n", a2);
                fprintf(f, "    idivl   %%ecx\n");
            } else {
                fprintf(f, "    idivl   %s(%%rip)\n", a2);
            }
            fprintf(f, "    movl    %%eax, %s(%%rip)\n", r);
            break;

        case OP_AND_OP:
            if (is_const_operand(a1)) {
                fprintf(f, "    movl    $%s, %%eax\n", a1);
            } else {
                fprintf(f, "    movl    %s(%%rip), %%eax\n", a1);
            }
            if (is_const_operand(a2)) {
                fprintf(f, "    andl    $%s, %%eax\n", a2);
            } else {
                fprintf(f, "    andl    %s(%%rip), %%eax\n", a2);
            }
            fprintf(f, "    movl    %%eax, %s(%%rip)\n", r);
            break;

        case OP_OR_OP:
            if (is_const_operand(a1)) {
                fprintf(f, "    movl    $%s, %%eax\n", a1);
            } else {
                fprintf(f, "    movl    %s(%%rip), %%eax\n", a1);
            }
            if (is_const_operand(a2)) {
                fprintf(f, "    orl     $%s, %%eax\n", a2);
            } else {
                fprintf(f, "    orl     %s(%%rip), %%eax\n", a2);
            }
            fprintf(f, "    movl    %%eax, %s(%%rip)\n", r);
            break;

        case OP_NOT_OP:
            if (is_const_operand(a1)) {
                fprintf(f, "    movl    $%s, %%eax\n", a1);
            } else {
                fprintf(f, "    movl    %s(%%rip), %%eax\n", a1);
            }
            fprintf(f, "    testl   %%eax, %%eax\n");
            fprintf(f, "    sete    %%al\n");
            fprintf(f, "    movzbl  %%al, %%eax\n");
            fprintf(f, "    movl    %%eax, %s(%%rip)\n", r);
            break;

        case OP_JE:
            write_cmp_set(f, "sete", a1, a2, r);
            break;
        case OP_JNE:
            write_cmp_set(f, "setne", a1, a2, r);
            break;
        case OP_JL:
            write_cmp_set(f, "setl", a1, a2, r);
            break;
        case OP_JG:
            write_cmp_set(f, "setg", a1, a2, r);
            break;
        case OP_JLE:
            write_cmp_set(f, "setle", a1, a2, r);
            break;
        case OP_JGE:
            write_cmp_set(f, "setge", a1, a2, r);
            break;

        case OP_JMP:
            /* (jmp, _, _, label) */
            fprintf(f, "    jmp     %s\n", r);
            break;

        case OP_JNZ:
            /* (jnz, cond, _, label)
             * 如果 cond 是标签引用 (L*), 直接作为跳转目标 */
            if (is_const_operand(a1)) {
                fprintf(f, "    cmpl    $0, $%s\n", a1);
            } else {
                fprintf(f, "    cmpl    $0, %s(%%rip)\n", a1);
            }
            fprintf(f, "    jne     %s\n", r);
            break;

        case OP_WRITE:
            /* (write, id, _, _)
             * printf("%d\n", id) */
            fprintf(f, "    movl    %s(%%rip), %%esi\n", a1);
            fprintf(f, "    leaq    .fmt(%%rip), %%rdi\n");
            fprintf(f, "    xorl    %%eax, %%eax\n");
            fprintf(f, "    call    printf@PLT\n");
            break;

        case OP_END:
            /* 程序出口 */
            fprintf(f, "    xorl    %%eax, %%eax\n");
            fprintf(f, "    leave\n");
            fprintf(f, "    ret\n");
            break;

        case OP_LABEL:
            /* (label, Lx, _, _) → 汇编标签 */
            fprintf(f, "%s:\n", a1);
            break;

        default:
            fprintf(f, "    # 未支持的操作码 %d\n", q->op);
            break;
        }
        fprintf(f, "\n");
    }

    fprintf(f, "    .size   main, .-main\n");
    fclose(f);
    printf("目标代码已生成: %s\n", outfile);
}
