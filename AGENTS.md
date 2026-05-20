# AGENTS.md — Compiler Frontend Project

## Build & Test

```bash
make -C compiler          # build
make -C compiler test     # run all 3 test samples
./compiler/compiler <file>  # compile a single source file
```

No external dependencies beyond `gcc`, `make`, and `libm`. GTK3 is optional
(`make GUI=1`) but not installed on this system.

## Architecture

```
compiler/src/
  grammar.h     — all shared types, enums (Token codes, Quad ops), Compiler context struct
  scanner.c/h   — lexical analyzer: DFA for identifiers, Pascal constant automaton
  symbol.c/h    — symbol table & constant table (enter/lookup), temp/label allocation
  quadruple.c/h — four-address code list (emit, dump, backpatch)
  parser.c/h    — recursive descent parser + semantic actions (translation grammar)
  main.c        — CLI entrypoint (conditional GTK3 via #ifdef USE_GTK)
```

## Key Conventions

- **Token codes** follow the course document: `program=3, var=4, ..., :=13, ...`
  Identifiers are `TOK_ID=1`, constants are `TOK_CONST=2`. Extended tokens
  (`if/while/and/or/not/comparison`) are 21–34. See `grammar.h` lines 30-64.
- **Quadruple format**: `(op, arg1, arg2, result)`. Underscore `_` means unused.
- **Symbol table**: `sym_table[]` is a flat array. `sym_count` tracks entries.
  Kinds: `KIND_PROGRAM=0`, `KIND_VARIABLE=1`, `KIND_TEMP=2`.
- **Translation grammar**: Variable declarations use semantic actions a1–a6
  (PPT slides 19-20). `a2` pushes identifiers onto `sem_stack[]`, `a6` processes
  them in FIFO order (declaration order) to assign offsets.
- **Two-pass scan**: `parser_parse()` first calls `scanner_scan_all()` to populate
  `token_list[]` and symbol/constant tables, then resets and does recursive descent.
  `token_count` must be saved before reset (scanner_init zeroes it).

## Compiler Stages (run order)

1. **Lexical analysis** → Token sequence (stored in `c->token_list[]`)
2. **Symbol/constant table** populated during scan
3. **Syntax + semantic analysis** → recursive descent + quadruple generation
4. Output: Token dump → Symbol table → Constant table → Quadruple list

## Expression Parsing

Recursive descent with operator precedence (NOT the document's operator-precedence
parser — that table remains in grammar.h but is unused). Levels:
- L1: `or` → L2: `and` → L3: `not` → L4: comparisons (=,<,>,<=,>=,<>) → L5: +,-
  → L6: *,/ → L7: id, const, (E), unary -

Comparison operators generate `OP_JE`/`OP_JL`/etc. with result in a temp.
IF/WHILE then use `OP_JNZ` on that temp for branching.

## IF/WHILE Code Generation

```
IF:   jg/comparison → t
      jnz t _ L_then
      jmp _ _ L_false   (or L_end if no else)
L_then: <then body>
      [jmp _ _ L_end]   (if else present)
L_false: <else body>
L_end:

WHILE:
L_loop: jl/comparison → t
      jnz t _ L_body
      jmp _ _ L_exit
L_body: <loop body>
      jmp _ _ L_loop
L_exit:
```

## Grammar (Extended)

```
PROGRAM    → program id SUB_PROGRAM .
SUB_PROGRAM → VARIABLE COM_SENTENCE
VARIABLE   → var ID_SEQ : TYPE ; | ε
ID_SEQ     → id { , id }
TYPE       → integer | real | char
COM_SENTENCE → begin STATEMENT { ; STATEMENT } end
STATEMENT  → EVA | IF | WHILE | COM_SENTENCE
EVA        → id := EXPRESSION
IF         → if EXPRESSION then STATEMENT [ else STATEMENT ]
WHILE      → while EXPRESSION do STATEMENT
EXPRESSION → or_expr
```
