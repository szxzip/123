# AGENTS.md

## Build

- **No Makefile or build system.** Default build includes GTK3 GUI (requires `gtk+-3.0` dev headers):
  ```bash
  gcc -DUSE_GTK $(pkg-config --cflags gtk+-3.0) src/*.c -o compiler.out $(pkg-config --libs gtk+-3.0) -lm
  ```
  When compiled with GTK3, running without arguments launches the GUI; with a file argument it stays in CLI mode.
- **Pure CLI** (no GUI, no GTK dependency):
  ```bash
  gcc src/*.c -o compiler.out -lm
  ```

## Run / Test

- **No automated test runner.** Verification is manual against sample programs in `test/`.
- **CLI workflow:**
  ```bash
  ./compiler.out test/sample1.txt      # generates test/sample1.txt.s
  gcc -no-pie test/sample1.txt.s -o test/sample1.out && ./test/sample1.out
  ```
- **Use `-no-pie`** when linking the generated `.s` file; the emitted assembly uses RIP-relative data references that may break under PIE.

## Architecture

- Single-pass C codebase in `src/`. No external dependencies except GTK3 (optional) and libc/math.
- **Pipeline:** scanner → parser → semantic → quadruple → optimize → codegen.
- **Parser is two-pass:**
  1. `scanner_scan_all()` tokenizes the whole source and populates the symbol/constant tables.
  2. State is reset (token stream kept, tables preserved), then recursive descent (`PROGRAM()`) runs over the token list again, emitting quadruples.
- All global/shared definitions (token codes, opcodes, `Compiler` struct) live in `grammar.h`. Every other module includes it.
- `main.c` is the only entrypoint. Do not add alternative `main` functions in other `.c` files.

## Source Language

- Pascal-like procedural language. See `README.md` for the full BNF.
- Quick reference:
  - Program header: `program id ... .`
  - Declarations: `var a,b:integer;`
  - Statements: `begin ... end` (semicolon-separated)
  - Assignment: `:=`
  - I/O: `write id`
  - Control: `if E then S [else S]`, `while E do S`
  - Expressions support `and`, `or`, `not`, comparisons, `+ - * /`, unary `-`, parentheses.

## Conventions & Quirks

- `.gitignore` ignores `*.s`, `*.out`, `*.exe`, `*.o`. Generated assembly and binaries are not tracked.
- The compiler outputs **x86-64 AT&T syntax** GAS assembly, not Intel syntax.
- `docs/` contains per-file line-by-line Chinese explanations. Treat them as reference, not executable specs; if they conflict with the code, trust the code.
- No formatter/linter/type-checker config exists. Follow existing C style in `src/`.
