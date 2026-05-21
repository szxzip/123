## 模块: main.c — 主程序入口 (CLI + GTK3 GUI)

## 简明解释

- CLI 模式（`read_file`、`read_stdin`、`main`）：无参数 → 启动 `run_gtk` 图形界面。带文件参数 → `read_file` → `parser_parse` → 输出词法单元/符号/常数/四元式表 → `optimize_run` → `codegen_generate` → 打印 gcc 命令。
- GTK3 图形界面模式（`#ifdef USE_GTK`）：5 标签页笔记本（词法单元/符号表/四元式/汇编代码/运行输出），文件打开按钮，编译按钮，gcc 编译运行按钮。`on_compile_clicked` 执行完整流水线并填充所有标签页。`on_gcc_clicked` 通过 popen 调用 gcc 汇编链接并运行生成的可执行文件。
- 常量说明：输出缓冲区为栈上 16KB，GTK 使用 `/tmp/opencode/` 存放临时文件。

---

# main.c 逐行详解

> 编译器前端主程序，包含 GTK3 图形界面和 CLI 命令行两种运行模式。

---

## 1. 头部注释与头文件引用 (第 1–29 行)

| 行号 | 代码 | 讲解 |
|------|------|------|
| 1 | `// * main.c — 编译器前端主程序` | 文件标题注释，标明本文件是编译器前端的主入口程序。 |
| 2 | `// *` | 注释分隔符，仅起排版作用。 |
| 3 | `// * 对应课程要求: 编译器前端设计与实现` | 说明本项目对应的课程设计题目为"编译器前端设计与实现"。 |
| 4 | `// *` | 注释分隔符。 |
| 5 | `// * 模块对应:` | 标明下面将列出各功能模块与源文件的对应关系。 |
| 6 | `// *   [词法分析] -> scanner.c (常数自动机 + 关键字/界符表)` | 词法分析模块由 `scanner.c` 实现，采用常数自动机（DFA）进行标识符识别，并维护关键字表和界符表。 |
| 7 | `// *   [符号表]   -> symbol.c  (标识符/常数查填, 临时变量分配)` | 符号表模块由 `symbol.c` 实现，负责标识符与常数的查找/填入（enter/lookup），以及临时变量和标签的分配。 |
| 8 | `// *   [四元式]   -> quadruple.c (中间代码生成)` | 四元式（中间代码）模块由 `quadruple.c` 实现，负责生成四地址格式的中间表示。 |
| 9 | `// *   [语法分析] -> parser.c (递归下降子程序 + 表达式分析)` | 语法分析模块由 `parser.c` 实现，采用递归下降子程序法进行语法分析，并处理表达式解析。 |
| 10 | `// *   [语义分析] -> parser.c 内置语义动作 (a1~a6 翻译文法)` | 语义分析内嵌在 `parser.c` 中，通过翻译文法（语义动作 a1~a6）在语法分析的同时完成语义处理。 |
| 11 | `// *` | 注释分隔符。 |
| 12 | `// * 运行方式:` | 标明下面列出程序的运行方式。 |
| 13 | `// *   编译: make` | 编译命令：在 `compiler/` 目录下执行 `make`。 |
| 14 | `// *   运行: ./compiler <source_file>         (CLI 模式)` | CLI 模式运行方式：通过命令行参数传入源文件名进行编译。 |
| 15 | `// *         ./compiler                        (CLI 交互模式)` | 不带参数运行时默认进入 GTK 图形界面模式（由 `main()` 中的 `argc < 2` 逻辑决定，非 CLI 交互，但注释保留原始设计意图）。 |
| 16 | `// *` | 注释分隔符。 |
| 17 | `// * 如有 GTK3: make GUI=1 编译图形界面版本` | 若系统安装了 GTK3 开发库，可通过 `make GUI=1` 编译图形界面版本。 |
| 18 | （空行） | 空行，用于分隔注释块和头文件引用区域。 |
| 19 | `#include "grammar.h"` | 引入语法头文件，其中定义了所有共享类型、枚举（Token 码、四元式操作码）、编译器上下文结构体 `Compiler` 等。 |
| 20 | `#include "scanner.h"` | 引入词法分析器头文件，声明词法扫描相关函数（如 `scanner_scan_all`、`scanner_dump_tokens` 等）。 |
| 21 | `#include "symbol.h"` | 引入符号表头文件，声明符号表与常数表相关函数（`sym_init`、`sym_dump`、`const_dump` 等）。 |
| 22 | `#include "quadruple.h"` | 引入四元式头文件，声明中间代码相关函数（`quad_init`、`quad_dump` 等）。 |
| 23 | `#include "parser.h"` | 引入语法分析器头文件，声明 `parser_parse` 等函数。 |
| 24 | `#include "optimize.h"` | 引入优化器头文件，声明 `optimize_run` 函数用于四元式优化。 |
| 25 | `#include "codegen.h"` | 引入目标代码生成器头文件，声明 `codegen_generate` 函数用于生成 x86-64 汇编代码。 |
| 26 | （空行） | 空行，分隔项目内头文件与外部系统头文件。 |
| 27 | `#include <gtk/gtk.h>` | 引入 GTK3 图形库头文件，提供窗口、按钮、文本视图等 GUI 组件。 |
| 28 | （空行） | 空行，用于分隔头文件引用区域与函数前置声明区域。 |
| 29 | `static char *read_file(const char *filename);` | 前置声明 `read_file` 静态函数，该函数通过文件名读取整个文件内容到堆内存并返回。由于函数定义在 `main()` 之后，需要提前声明。 |

---

## 2. GTK3 全局变量 (第 35–38 行)

| 行号 | 代码 | 讲解 |
|------|------|------|
| 31 | `// ================================================================` | 顶部注释分隔线，用于在代码中划出清晰的节边界。 |
| 32 | `// * GTK3 图形界面` | 节标题注释，标记以下代码段属于 GTK3 图形界面部分。 |
| 33 | `// * ================================================================` | 底部注释分隔线，与顶部对称。 |
| 34 | （空行） | 空行。 |
| 35 | `static Compiler g_compiler;` | 声明一个全局的 `Compiler` 结构体变量 `g_compiler`，用于存储整个编译过程中所有状态信息（源文本、词法单元列表、符号表、四元式列表等）。`static` 限定其作用域为本文件。`g_` 前缀表示 global。 |
| 36 | `static GtkWidget *src_view, *token_view, *sym_view, *quad_view, *asm_view, *run_view;` | 声明六个全局 `GtkWidget*` 指针变量，分别指向 GUI 中六个 `GtkTextView` 控件：`src_view`（源代码输入区）、`token_view`（Token 序列输出标签页）、`sym_view`（符号表/常数表输出标签页）、`quad_view`（四元式输出标签页）、`asm_view`（汇编代码输出标签页）、`run_view`（运行输出标签页）。`static` 限定作用域。 |
| 37 | `static GtkWidget *parent_window;` | 声明全局窗口指针 `parent_window`，保存主窗口的引用，供文件选择对话框（`GtkFileChooserDialog`）等子窗口设置父窗口使用。 |
| 38 | `static int g_compiled_ok;` | 声明全局整型标志 `g_compiled_ok`，记录最近一次编译是否成功：编译成功时置 `1`，失败时置 `0`。用于 `on_gcc_clicked` 中判断是否可以执行 GCC 编译运行。 |

---

## 3. `on_open_clicked()` — 文件选择对话框 (第 40–60 行)

| 行号 | 代码 | 讲解 |
|------|------|------|
| 40 | `static void on_open_clicked(GtkWidget *widget, gpointer data) {` | 定义"读取文件"按钮的回调函数。参数 `widget` 是被点击的按钮控件，`data` 是连接信号时传入的用户数据（此处为 NULL）。 |
| 41 | `    (void)widget;` | 将 `widget` 强制转换为 `void`，消除编译器"未使用参数"的警告。代码中未用到该参数。 |
| 42 | `    (void)data;` | 同样消除 `data` 参数的未使用警告。 |
| 43 | `    GtkWidget *dialog = gtk_file_chooser_dialog_new(` | 调用 GTK 函数创建一个文件选择对话框（`GtkFileChooserDialog`），返回 `GtkWidget*`。该对话框继承自 `GtkDialog`，内嵌一个 `GtkFileChooser` 组件。 |
| 44 | `        "打开源文件", GTK_WINDOW(parent_window),` | 第一个参数设置对话框标题为"打开源文件"；第二个参数 `GTK_WINDOW(parent_window)` 将全局主窗口设为对话框的父窗口（模态对话框将阻塞主窗口）。 |
| 45 | `        GTK_FILE_CHOOSER_ACTION_OPEN,` | 设置文件选择器的动作为 `GTK_FILE_CHOOSER_ACTION_OPEN`，即"打开现有文件"模式（选择已有的文件，不可输入不存在的文件名）。 |
| 46 | `        "取消", GTK_RESPONSE_CANCEL,` | 添加对话框按钮：按钮文字为"取消"，按下后返回 `GTK_RESPONSE_CANCEL` 响应码。 |
| 47 | `        "打开", GTK_RESPONSE_ACCEPT,` | 添加对话框按钮：按钮文字为"打开"，按下后返回 `GTK_RESPONSE_ACCEPT` 响应码。 |
| 48 | `        NULL);` | 参数列表以 `NULL` 结束（可变长参数终止标记）。 |
| 49 | `    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {` | 以模态方式运行对话框，阻塞等待用户操作。如果用户点击了"打开"按钮（返回 `GTK_RESPONSE_ACCEPT`），则进入 if 块处理文件读取。 |
| 50 | `        char *fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));` | 从文件选择器中获取用户选中的文件路径字符串。返回的字符串由 GLib 分配内存，使用后需调用 `g_free` 释放。 |
| 51 | `        char *src = read_file(fname);` | 调用 `read_file` 函数，读取该文件的全部内容。返回值是 `malloc` 分配的 C 字符串，失败时返回 NULL。 |
| 52 | `        if (src) {` | 如果文件读取成功（`src` 非空），则将内容显示到源代码文本视图中。 |
| 53 | `            GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(src_view));` | 从 `src_view`（源代码输入区 `GtkTextView`）获取其底层的 `GtkTextBuffer`。GTK 中文本内容存储在 buffer 中，view 只负责显示。 |
| 54 | `            gtk_text_buffer_set_text(buf, src, -1);` | 将读取到的文件内容 `src` 设置到 buffer 中。第三个参数 `-1` 表示自动计算字符串长度（遇 `\0` 为止）。 |
| 55 | `            free(src);` | 释放 `read_file` 分配的堆内存，防止内存泄漏。 |
| 56 | `        }` | 结束 `if (src)` 块。 |
| 57 | `        g_free(fname);` | 释放 GLib 为文件路径字符串分配的内存。这是 GLib 专用的释放函数，对应 `gtk_file_chooser_get_filename` 的返回值。 |
| 58 | `    }` | 结束 `if (gtk_dialog_run(...) == GTK_RESPONSE_ACCEPT)` 块。即使用户点了"取消"，也会跳过文件读取直接销毁对话框。 |
| 59 | `    gtk_widget_destroy(dialog);` | 销毁文件选择对话框控件，释放其占用的所有 GTK 资源。无论用户选择打开还是取消，都会执行此行。 |
| 60 | `}` | 结束 `on_open_clicked` 函数定义。 |

---

## 4. `on_compile_clicked()` — GUI 完整编译流水线 (第 62–131 行)

### 4.1 重置编译器状态

| 行号 | 代码 | 讲解 |
|------|------|------|
| 62 | `static void on_compile_clicked(GtkWidget *widget, gpointer data) {` | 定义"编译"按钮的回调函数。这是整个编译器前端流水线在 GUI 模式下的完整入口。 |
| 63 | `    (void)widget;` | 消除 `widget` 参数的未使用警告。 |
| 64 | `    (void)data;` | 消除 `data` 参数的未使用警告。 |
| 65 | `    GtkTextBuffer *buf;` | 声明一个 `GtkTextBuffer*` 局部变量 `buf`，稍后用于临时持有各个文本视图的 buffer。 |
| 66 | `    char *src_text;` | 声明一个 `char*` 局部变量 `src_text`，用于接收从源代码输入视图中取出的文本内容。 |
| 67 | `    char out_buf[16384];` | 声明一个 16 KB 的栈缓冲区 `out_buf`，用于拼接符号表 + 常数表的组合输出。 |
| 68 | （空行） | 空行。 |
| 69 | `    memset(&g_compiler, 0, sizeof(g_compiler));` | 将全局编译器结构体 `g_compiler` 的所有字节清零，重置上一次编译可能遗留的状态（词法单元、符号表、四元式、错误信息等）。 |
| 70 | `    sym_init(&g_compiler);` | 调用 `sym_init` 初始化符号表子系统，通常包括将 `sym_count` 和 `const_count` 置零、初始化临时变量计数器和标签计数器等。 |
| 71 | `    quad_init(&g_compiler);` | 调用 `quad_init` 初始化四元式子系统，通常包括将 `quad_count` 置零，准备接收新的中间代码。 |
| 72 | （空行） | 空行。 |

### 4.2 从文本视图读取源代码

| 行号 | 代码 | 讲解 |
|------|------|------|
| 73 | `    // 读取源代码` | 注释：标记此段代码的功能为读取用户在 GUI 编辑区中输入（或通过"读取文件"按钮加载）的源代码文本。 |
| 74 | `    buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(src_view));` | 从 `src_view`（源代码输入区）获取底层的 `GtkTextBuffer` 对象。 |
| 75 | `    GtkTextIter start, end;` | 声明两个 `GtkTextIter`（GTK 文本迭代器）变量 `start` 和 `end`，用于定位 buffer 中文本的起始和结束位置。 |
| 76 | `    gtk_text_buffer_get_bounds(buf, &start, &end);` | 获取 buffer 的边界迭代器：`start` 指向文本的第一个字符之前，`end` 指向最后一个字符之后。 |
| 77 | `    src_text = gtk_text_buffer_get_text(buf, &start, &end, FALSE);` | 从 buffer 中提取从 `start` 到 `end` 的全部文本内容。最后一个参数 `FALSE` 表示不包含隐藏字符。返回的字符串由 GTK 分配内存，需要后续调用 `g_free` 释放。 |
| 78 | （空行） | 空行。 |
| 79 | `    g_compiler.source = src_text;` | 将提取的源代码字符串指针赋给编译器结构体的 `source` 字段。编译器各阶段（词法、语法）将通过此字段访问源代码文本。 |
| 80 | `    g_compiler.len = strlen(src_text);` | 计算源代码字符串的长度（字节数），存入编译器结构体的 `len` 字段。词法分析器用此字段判断是否扫描结束（`pos >= len`）。 |
| 81 | `    g_compiler.pos = 0;` | 将当前扫描位置 `pos` 重置为 0，从源代码的第一个字符开始扫描。 |

### 4.3 调用语法分析器

| 行号 | 代码 | 讲解 |
|------|------|------|
| 82 | （空行） | 空行。 |
| 83 | `    // 编译` | 注释：标记下面将开始真正的编译处理。 |
| 84 | `    int ok = parser_parse(&g_compiler);` | 调用语法分析器主函数 `parser_parse`，对 `g_compiler` 中的源代码进行完整的"词法分析 → 符号表填充 → 语法/语义分析 → 四元式生成"流程。返回值 `ok` 为非零表示编译成功，0 表示失败。 |
| 85 | （空行） | 空行。 |
| 86 | `    if (ok) {` | 如果编译成功（`ok` 为非零），进入成功分支，在各输出标签页中展示编译结果。 |

### 4.4 填充输出 Tab 1 — Token 序列

| 行号 | 代码 | 讲解 |
|------|------|------|
| 87 | `        // 输出 Token 序列` | 注释：标记此段代码用于在第一个标签页输出词法分析得到的 Token（词法单元）序列。 |
| 88 | `        char tbuf[8192];` | 声明一个 8 KB 的栈缓冲区 `tbuf`，用于存储 Token 序列的格式化文本。 |
| 89 | `        scanner_dump_tokens(&g_compiler, tbuf, sizeof(tbuf));` | 调用 `scanner_dump_tokens` 将编译器扫描到的所有词法单元以可读文本格式写入 `tbuf`。通常每行包含 Token 类型码、词素文本及其他元信息。 |
| 90 | `        buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(token_view));` | 获取第一个标签页（Token 序列）对应的文本视图 buffer。 |
| 91 | `        gtk_text_buffer_set_text(buf, tbuf, -1);` | 将 Token 序列文本设置到该标签页的 buffer 中显示。 |
| 92 | （空行） | 空行。 |

### 4.5 填充输出 Tab 2 — 符号表与常数表

| 行号 | 代码 | 讲解 |
|------|------|------|
| 93 | `        // 输出符号表 + 常数表` | 注释：标记此段代码的功能是将符号表与常数表内容合并后输出到第二个标签页。 |
| 94 | `        char sbuf[8192], cbuf[4096];` | 声明两个栈缓冲区：`sbuf`（8 KB，存储符号表文本）和 `cbuf`（4 KB，存储常数表文本）。 |
| 95 | `        sym_dump(&g_compiler, sbuf, sizeof(sbuf));` | 调用 `sym_dump` 将符号表中所有已登记的标识符（变量名、程序名等）以可读格式写入 `sbuf`。输出内容通常包括序号、名字、种类、类型、偏移地址等字段。 |
| 96 | `        const_dump(&g_compiler, cbuf, sizeof(cbuf));` | 调用 `const_dump` 将常数表中所有已登记的字面常量以可读格式写入 `cbuf`。 |
| 97 | `        snprintf(out_buf, sizeof(out_buf), "%s%s", sbuf, cbuf);` | 将 `sbuf`（符号表）和 `cbuf`（常数表）拼接成一个字符串，写入 `out_buf`。`snprintf` 保证不会越界写入。 |
| 98 | `        buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(sym_view));` | 获取第二个标签页（符号表/常数表）对应的文本视图 buffer。 |
| 99 | `        gtk_text_buffer_set_text(buf, out_buf, -1);` | 将拼接后的完整符号表/常数表文本显示到该标签页。 |
| 100 | （空行） | 空行。 |

### 4.6 填充输出 Tab 3 — 四元式（中间代码）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 101 | `        // 输出四元式` | 注释：标记此段代码用于输出四元式（中间代码）列表。 |
| 102 | `        char qbuf[8192];` | 声明一个 8 KB 的栈缓冲区 `qbuf`，用于存储四元式格式化文本。 |
| 103 | `        quad_dump(&g_compiler, qbuf, sizeof(qbuf));` | 调用 `quad_dump` 将所有四元式以可读格式写入 `qbuf`。每行格式通常为 `(序号) (op, arg1, arg2, result)`，其中 `_` 表示未使用字段。 |
| 104 | `        buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(quad_view));` | 获取第三个标签页（四元式）对应的文本视图 buffer。 |
| 105 | `        gtk_text_buffer_set_text(buf, qbuf, -1);` | 将四元式文本显示到该标签页。 |
| 106 | （空行） | 空行。 |

### 4.7 优化 + 代码生成 + 填充输出 Tab 4 — 汇编代码

| 行号 | 代码 | 讲解 |
|------|------|------|
| 107 | `        // 优化 + 生成汇编` | 注释：标记下面将先对四元式进行优化，再从优化后的四元式生成 x86-64 汇编代码。 |
| 108 | `        optimize_run(&g_compiler);` | 调用优化器主函数 `optimize_run`，对 `g_compiler` 中的四元式列表进行优化（如消除冗余临时变量、合并无用赋值等）。优化直接在原四元式列表上就地修改。 |
| 109 | `        codegen_generate(&g_compiler, "/tmp/opencode/compiler_output.s");` | 调用目标代码生成函数，将优化后的四元式转换为 x86-64 AT&T 格式汇编代码，写入临时文件 `/tmp/opencode/compiler_output.s`。该文件后续供 GCC 编译使用。 |
| 110 | （空行） | 空行。 |
| 111 | `        // 显示汇编代码` | 注释：标记下面将从刚生成的 `.s` 文件中读取汇编代码并显示到 GUI。 |
| 112 | `        FILE *af = fopen("/tmp/opencode/compiler_output.s", "r");` | 以只读文本模式打开刚才生成的汇编文件。 |
| 113 | `        if (af) {` | 如果文件打开成功（`af` 非 NULL），进入读取和显示逻辑。 |
| 114 | `            char abuf[16384];` | 声明一个 16 KB 的栈缓冲区 `abuf`，用于存储从汇编文件中读取的全部内容。 |
| 115 | `            size_t alen = fread(abuf, 1, sizeof(abuf) - 1, af);` | 调用 `fread` 从文件中读取最多 `sizeof(abuf)-1` 个字节到 `abuf` 中，返回值 `alen` 是实际读取的字节数。预留的一个字节用于后续附加 `\0` 结尾符。 |
| 116 | `            abuf[alen] = '\0';` | 在读取内容的末尾追加字符串终止符 `\0`，使其成为合法的 C 字符串。 |
| 117 | `            fclose(af);` | 关闭汇编文件，释放文件句柄资源。 |
| 118 | `            buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(asm_view));` | 获取第四个标签页（汇编代码）对应的文本视图 buffer。 |
| 119 | `            gtk_text_buffer_set_text(buf, abuf, -1);` | 将汇编代码文本显示到该标签页。 |
| 120 | `        }` | 结束 `if (af)` 块。如果文件打开失败则静默跳过，不更新汇编标签页。 |
| 121 | （空行） | 空行。 |
| 122 | `        g_compiled_ok = 1;` | 将全局编译成功标志设为 1，允许用户随后点击"GCC 编译运行"按钮来执行汇编并运行。 |

### 4.8 编译失败处理

| 行号 | 代码 | 讲解 |
|------|------|------|
| 123 | `    } else {` | 如果 `parser_parse` 返回 0（编译失败），进入错误处理分支。 |
| 124 | `        snprintf(out_buf, sizeof(out_buf), "编译错误:\n%s", g_compiler.error_msg);` | 将错误信息前缀 "编译错误:\n" 与编译器结构体中的 `error_msg` 字段拼接，写入 `out_buf`。`error_msg` 由语法分析器在检测到错误时填充（如"语法错误: 缺少分号"等）。 |
| 125 | `        buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(token_view));` | 获取第一个标签页（Token 序列）的文本视图 buffer，复用该标签页来显示错误信息。 |
| 126 | `        gtk_text_buffer_set_text(buf, out_buf, -1);` | 将错误信息文本显示在 Token 序列标签页中，覆盖原有内容。 |
| 127 | `        g_compiled_ok = 0;` | 将全局编译成功标志设为 0，阻止用户执行 GCC 编译运行。 |
| 128 | `    }` | 结束 `if (ok) ... else ...` 分支。 |
| 129 | （空行） | 空行。 |
| 130 | `    g_free(src_text);` | 释放 `gtk_text_buffer_get_text` 为源代码文本分配的内存。无论编译成功与否都执行此行，防止内存泄漏。 |
| 131 | `}` | 结束 `on_compile_clicked` 函数定义。 |

---

## 5. `on_gcc_clicked()` — popen 调用 GCC 编译并执行 (第 133–176 行)

### 5.1 前置检查

| 行号 | 代码 | 讲解 |
|------|------|------|
| 133 | `static void on_gcc_clicked(GtkWidget *widget, gpointer data) {` | 定义"GCC 编译运行"按钮的回调函数。功能是调用系统 GCC 编译编译器生成的汇编文件，并执行生成的可执行文件，将 GCC 输出和程序运行输出显示在运行输出标签页。 |
| 134 | `    (void)widget;` | 消除 `widget` 参数的未使用警告。 |
| 135 | `    (void)data;` | 消除 `data` 参数的未使用警告。 |
| 136 | `    if (!g_compiled_ok) {` | 检查全局编译成功标志。如果上次编译失败（`g_compiled_ok == 0`），则拒绝执行 GCC。 |
| 137 | `        GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(run_view));` | 获取运行输出标签页的 buffer。 |
| 138 | `        gtk_text_buffer_set_text(buf, "请先点\"编译\"编译成功后再运行 GCC\n", -1);` | 在运行输出标签页显示提示信息，告知用户需要先成功编译。然后直接 `return`，不执行后续 GCC 流程。 |
| 139 | `        return;` | 提前返回，终止本函数。 |
| 140 | `    }` | 结束前置检查块。 |
| 141 | （空行） | 空行。 |

### 5.2 运行 GCC 编译汇编文件

| 行号 | 代码 | 讲解 |
|------|------|------|
| 142 | `    char result[32768];` | 声明一个 32 KB 的栈缓冲区 `result`，用于累积整个输出结果（GCC 编译信息 + 程序运行输出）。 |
| 143 | `    int pos = 0;` | 声明位置追踪变量 `pos`，表示 `result` 中当前已写入的有效字符数（下一个可写位置）。后续所有 `snprintf` 都使用 `result + pos` 追加写入。 |
| 144 | （空行） | 空行。 |
| 145 | `    // 运行 gcc 编译` | 注释：标记下面将执行 GCC 编译流程。 |
| 146 | `    pos += snprintf(result + pos, sizeof(result) - pos,` | `snprintf` 将格式化字符串写入 `result + pos`，并返回写入的字符数（不含 `\0`）。`sizeof(result) - pos` 计算剩余可用空间。返回值加到 `pos` 上实现位置推进。 |
| 147 | `        "===== GCC 编译 =====\n");` | 写入节标题分隔线 "===== GCC 编译 =====\n"，将 GCC 编译信息与程序运行输出在视觉上分开。 |
| 148 | `    FILE *gcc = popen(` | 调用 `popen` 创建一个管道，启动一个子进程执行 shell 命令，并返回一个 `FILE*` 流用于读取子进程的标准输出。 |
| 149 | `        "gcc -no-pie /tmp/opencode/compiler_output.s -o /tmp/opencode/compiler_output 2>&1",` | shell 命令：① `gcc -no-pie` 使用系统 GCC 编译器，`-no-pie` 禁用位置无关可执行文件（某些旧版本 GCC 需要）；② 编译源文件 `/tmp/opencode/compiler_output.s`（编译器生成的汇编）；③ `-o /tmp/opencode/compiler_output` 指定输出可执行文件路径；④ `2>&1` 将标准错误重定向到标准输出，使编译错误也能通过管道读取。 |
| 150 | `        "r");` | 以只读模式打开管道（"r"），用于读取 GCC 编译过程的输出信息。 |
| 151 | `    if (gcc) {` | 如果管道创建成功（`gcc` 非 NULL），进入读取逻辑。 |
| 152 | `        char line[512];` | 声明一个 512 字节的栈缓冲区 `line`，用于逐行读取子进程输出。 |
| 153 | `        while (fgets(line, sizeof(line), gcc))` | 循环调用 `fgets` 逐行读取 GCC 子进程的输出，直到 EOF（管道关闭）。 |
| 154 | `            pos += snprintf(result + pos, sizeof(result) - pos, "%s", line);` | 将每行输出追加写入 `result` 缓冲区，更新 `pos`。 |
| 155 | `        int status = pclose(gcc);` | 关闭管道，等待子进程结束。`pclose` 返回子进程的退出状态码（类似 `waitpid` 的返回值）。0 表示编译成功。 |
| 156 | `        if (status == 0) {` | 如果 GCC 编译成功（退出码为 0），进入程序运行分支。 |
| 157 | `            pos += snprintf(result + pos, sizeof(result) - pos,` | 向 `result` 追加写入下一行信息。 |
| 158 | `                "GCC 编译成功\n\n");` | 写入"GCC 编译成功"提示和两个换行，与后续程序运行输出分隔。 |
| 159 | `            // 运行可执行文件` | 注释：标记下面将执行刚才编译生成的可执行文件。 |
| 160 | `            pos += snprintf(result + pos, sizeof(result) - pos,` | 追加写入程序运行输出的节标题。 |
| 161 | `                "===== 程序运行输出 =====\n");` | 写入节标题分隔线 "===== 程序运行输出 =====\n"。 |
| 162 | `            FILE *run = popen("/tmp/opencode/compiler_output 2>&1", "r");` | 再次调用 `popen`，这次直接执行编译生成的可执行文件 `/tmp/opencode/compiler_output`，并将标准错误重定向到标准输出，以只读方式捕获其输出。 |
| 163 | `            if (run) {` | 如果程序执行管道创建成功。 |
| 164 | `                while (fgets(line, sizeof(line), run))` | 逐行读取程序的运行输出。 |
| 165 | `                    pos += snprintf(result + pos, sizeof(result) - pos, "%s", line);` | 将每行输出追加到 `result` 缓冲区。 |
| 166 | `                pclose(run);` | 关闭程序执行管道，等待程序运行结束。 |
| 167 | `            }` | 结束 `if (run)` 块。 |
| 168 | `        } else {` | 如果 GCC 编译失败（`status != 0`），进入错误信息分支。 |
| 169 | `            pos += snprintf(result + pos, sizeof(result) - pos,` | 追加写入 GCC 编译失败信息。 |
| 170 | `                "\nGCC 编译失败 (退出码 %d)\n", status);` | 写入失败提示文本，包含 GCC 的退出码以便调试。注意 `pclose` 返回的 `status` 需要通过宏（如 `WEXITSTATUS`）提取实际退出码，但这里直接使用原始值也具备基本参考意义。 |
| 171 | `        }` | 结束 `if (status == 0) ... else ...` 分支。 |
| 172 | `    }` | 结束 `if (gcc)` 块。如果 `popen` 失败则跳过所有输出，`result` 仅包含最初的节标题。 |
| 173 | （空行） | 空行。 |
| 174 | `    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(run_view));` | 获取第五个标签页（运行输出）对应的文本视图 buffer。 |
| 175 | `    gtk_text_buffer_set_text(buf, result, -1);` | 将累积的全部输出文本（GCC 编译信息 + 程序运行输出）显示在该标签页中。 |
| 176 | `}` | 结束 `on_gcc_clicked` 函数定义。 |

---

## 6. `create_output_area()` 辅助函数 (第 178–189 行)

| 行号 | 代码 | 讲解 |
|------|------|------|
| 178 | `static GtkWidget *create_output_area(const char *title, GtkWidget *notebook) {` | 定义一个工厂函数，用于快速创建 notebook（标签页容器）中的一个输出标签页。参数 `title` 是标签页标题文字，`notebook` 是目标 `GtkNotebook` 容器。返回值为内部创建的 `GtkTextView*` 控件指针。 |
| 179 | `    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);` | 创建一个带滚动条的容器 `GtkScrolledWindow`。两个 `NULL` 参数表示使用默认的水平/垂直滚动条调整策略（后续通过 `set_policy` 覆盖为 AUTOMATIC）。 |
| 180 | `    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),` | 设置滚动窗口的滚动条显示策略。 |
| 181 | `        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);` | 两个方向都设为 `GTK_POLICY_AUTOMATIC`：仅在内容超出可视区域时才自动显示滚动条，否则隐藏。 |
| 182 | `    GtkWidget *view = gtk_text_view_new();` | 创建一个 `GtkTextView`（多行文本视图）控件，用于显示文本内容。 |
| 183 | `    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);` | 将文本视图设为不可编辑（只读模式），防止用户修改输出内容。 |
| 184 | `    gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);` | 将文本视图设为等宽字体（monospace），使 Token 序列、四元式、汇编代码等对齐输出更加美观。 |
| 185 | `    gtk_container_add(GTK_CONTAINER(scrolled), view);` | 将文本视图嵌入滚动窗口容器中。这样当文本内容超出视图范围时会出现滚动条。 |
| 186 | `    GtkWidget *label = gtk_label_new(title);` | 创建一个标签控件 `GtkLabel`，显示传入的标题文本（如"Token 序列"），用作 notebook 的标签页标题。 |
| 187 | `    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scrolled, label);` | 将滚动窗口（内含文本视图）作为一个新标签页添加到 notebook 中，并以 `label` 作为该标签页的标题标签。 |
| 188 | `    return view;` | 返回内部创建的 `GtkTextView*` 指针。调用方将其保存到全局变量（如 `token_view`），以便后续通过它更新该标签页的文本内容。 |
| 189 | `}` | 结束 `create_output_area` 函数定义。 |

---

## 7. `activate()` — GTK 主窗口构建 (第 191–237 行)

### 7.1 创建顶层窗口

| 行号 | 代码 | 讲解 |
|------|------|------|
| 191 | `static void activate(GtkApplication *app, gpointer user_data) {` | 定义 GTK Application 的 `activate` 信号回调函数。当 GTK 应用启动时（`g_application_run`），会触发此信号，在此函数内构建整个 GUI 界面。 |
| 192 | `    (void)user_data;` | 消除 `user_data` 参数的未使用警告（连接信号时传入的是 NULL）。 |
| 193 | `    GtkWidget *window = gtk_application_window_new(app);` | 创建一个与 `GtkApplication` 关联的顶层窗口。这是 GTK Application 框架的标准用法，而非直接使用 `gtk_window_new`。 |
| 194 | `    parent_window = window;` | 将窗口指针保存到全局变量 `parent_window`，供文件选择对话框等子窗口设置父窗口（实现正确的窗口堆叠和模态管理）。 |
| 195 | `    gtk_window_set_title(GTK_WINDOW(window), "编译器前端 - Compiler Frontend");` | 设置窗口标题栏文字为"编译器前端 - Compiler Frontend"（中英双语）。 |
| 196 | `    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);` | 设置窗口的默认初始大小为宽 1000 像素、高 700 像素。用户可手动调整窗口大小。 |
| 197 | （空行） | 空行。 |

### 7.2 垂直布局盒子

| 行号 | 代码 | 讲解 |
|------|------|------|
| 198 | `    // 主布局: 垂直` | 注释：说明主布局容器采用垂直方向排列子控件。 |
| 199 | `    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);` | 创建一个垂直方向的盒子容器 `GtkBox`。第二个参数 `5` 是子控件之间的间距（像素）。所有子控件将从上到下依次排列。 |
| 200 | `    gtk_container_add(GTK_CONTAINER(window), vbox);` | 将垂直盒子容器添加到窗口中。GTK 顶层窗口只能包含一个直接子控件，这里 `vbox` 就是唯一的顶层子控件。 |

### 7.3 源代码输入区

| 行号 | 代码 | 讲解 |
|------|------|------|
| 201 | （空行） | 空行。 |
| 202 | `    // 上半部分: 源代码输入` | 注释：标记此段构建窗口上半部分的源代码输入区域。 |
| 203 | `    GtkWidget *src_label = gtk_label_new("源程序 (Source Code):");` | 创建一个标签控件，显示提示文字"源程序 (Source Code):"。 |
| 204 | `    gtk_box_pack_start(GTK_BOX(vbox), src_label, FALSE, FALSE, 0);` | 将标签添加到垂直盒子顶部。参数：`FALSE` 表示不扩展占用多余空间，`FALSE` 表示不填充分配的空间，`0` 表示四周无额外内边距。 |
| 205 | `    GtkWidget *src_scrolled = gtk_scrolled_window_new(NULL, NULL);` | 创建一个滚动窗口容器，用于包裹源代码文本视图，在内容超出时提供滚动条。 |
| 206 | `    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(src_scrolled),` | 设置滚动窗口的滚动条显示策略。 |
| 207 | `        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);` | 水平和垂直方向都设为自动显示滚动条（内容超出时显示，否则隐藏）。 |
| 208 | `    gtk_widget_set_size_request(src_scrolled, -1, 200);` | 设置滚动窗口的最小高度为 200 像素，宽度设为 `-1` 表示保持默认（由布局容器决定）。这为源代码编辑区预留了固定的显示高度。 |
| 209 | `    src_view = gtk_text_view_new();` | 创建一个 `GtkTextView` 控件作为源代码编辑区，并将其指针保存到全局变量 `src_view`。 |
| 210 | `    gtk_text_view_set_monospace(GTK_TEXT_VIEW(src_view), TRUE);` | 将源代码编辑区的字体设为等宽字体，便于代码对齐和阅读。区别于输出区，源代码编辑区保持可编辑（默认状态）。 |
| 211 | `    gtk_container_add(GTK_CONTAINER(src_scrolled), src_view);` | 将文本视图嵌入滚动窗口容器中。 |
| 212 | `    gtk_box_pack_start(GTK_BOX(vbox), src_scrolled, FALSE, FALSE, 0);` | 将滚动窗口（内含源代码编辑区）添加到垂直盒子的第二个位置（标签之下）。 |
| 213 | （空行） | 空行。 |

### 7.4 按钮行

| 行号 | 代码 | 讲解 |
|------|------|------|
| 214 | `    // 按钮行` | 注释：标记此段构建三个操作按钮所在的行。 |
| 215 | `    GtkWidget *hbox_btn = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);` | 创建一个水平方向的盒子容器用于放置按钮，子控件间距为 5 像素。 |
| 216 | `    GtkWidget *btn_open = gtk_button_new_with_label("读取文件");` | 创建第一个按钮，标签文字为"读取文件"。点击后将弹出文件选择对话框。 |
| 217 | `    g_signal_connect(btn_open, "clicked", G_CALLBACK(on_open_clicked), NULL);` | 将按钮的 `clicked` 信号连接到 `on_open_clicked` 回调函数。`NULL` 是传递给回调函数的用户数据（此处不需要）。`G_CALLBACK` 宏用于类型转换。 |
| 218 | `    gtk_box_pack_start(GTK_BOX(hbox_btn), btn_open, FALSE, FALSE, 0);` | 将"读取文件"按钮添加到水平盒子中，从左到右排列。 |
| 219 | `    GtkWidget *btn_compile = gtk_button_new_with_label("编译 (Compile)");` | 创建第二个按钮，标签文字为"编译 (Compile)"（中英双语）。点击后执行编译器前端完整流水线。 |
| 220 | `    g_signal_connect(btn_compile, "clicked", G_CALLBACK(on_compile_clicked), NULL);` | 将按钮的 `clicked` 信号连接到 `on_compile_clicked` 回调函数。 |
| 221 | `    gtk_box_pack_start(GTK_BOX(hbox_btn), btn_compile, FALSE, FALSE, 0);` | 将"编译"按钮添加到水平盒子中，紧接在"读取文件"按钮右侧。 |
| 222 | `    GtkWidget *btn_gcc = gtk_button_new_with_label("GCC 编译运行");` | 创建第三个按钮，标签文字为"GCC 编译运行"。点击后调用系统 GCC 编译生成的汇编并执行。 |
| 223 | `    g_signal_connect(btn_gcc, "clicked", G_CALLBACK(on_gcc_clicked), NULL);` | 将按钮的 `clicked` 信号连接到 `on_gcc_clicked` 回调函数。 |
| 224 | `    gtk_box_pack_start(GTK_BOX(hbox_btn), btn_gcc, FALSE, FALSE, 0);` | 将"GCC 编译运行"按钮添加到水平盒子最右侧。 |
| 225 | `    gtk_box_pack_start(GTK_BOX(vbox), hbox_btn, FALSE, FALSE, 5);` | 将按钮行（水平盒子）添加到主垂直布局中。最后一个参数 `5` 是此控件与上下相邻控件之间的额外间距（内边距）。 |
| 226 | （空行） | 空行。 |

### 7.5 Notebook 输出标签页

| 行号 | 代码 | 讲解 |
|------|------|------|
| 227 | `    // 下半部分: 输出 (Notebook)` | 注释：标记此段构建窗口下半部分的多标签页输出区域。 |
| 228 | `    GtkWidget *notebook = gtk_notebook_new();` | 创建一个 `GtkNotebook` 控件。Notebook 是一个容器，包含多个子页面（标签页），用户通过点击标签标题切换显示不同页面。 |
| 229 | `    token_view = create_output_area("Token 序列", notebook);` | 调用 `create_output_area` 在 notebook 中创建第一个标签页，标题为"Token 序列"，返回的 `GtkTextView*` 保存在全局变量 `token_view` 中。 |
| 230 | `    sym_view   = create_output_area("符号表/常数表", notebook);` | 创建第二个标签页，标题为"符号表/常数表"，返回的视图指针保存在 `sym_view`。 |
| 231 | `    quad_view  = create_output_area("四元式(中间代码)", notebook);` | 创建第三个标签页，标题为"四元式(中间代码)"，视图指针保存在 `quad_view`。 |
| 232 | `    asm_view   = create_output_area("汇编代码", notebook);` | 创建第四个标签页，标题为"汇编代码"，视图指针保存在 `asm_view`。 |
| 233 | `    run_view   = create_output_area("运行输出", notebook);` | 创建第五个标签页，标题为"运行输出"，视图指针保存在 `run_view`。 |
| 234 | `    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);` | 将 notebook 添加到主垂直布局的最后位置。`TRUE, TRUE` 表示此控件在水平和垂直方向都扩展以填充剩余空间（这是唯一被设为可扩展的控件，确保输出区域占满窗口下半部分）。 |
| 235 | （空行） | 空行。 |
| 236 | `    gtk_widget_show_all(window);` | 显示窗口及其所有子控件。GTK 控件默认是隐藏的，必须调用此函数使整个界面可见。 |
| 237 | `}` | 结束 `activate` 函数定义。 |

---

## 8. `run_gtk()` — GTK 应用启动入口 (第 239–245 行)

| 行号 | 代码 | 讲解 |
|------|------|------|
| 239 | `void run_gtk(int argc, char **argv) {` | 定义 `run_gtk` 函数，这是 GTK 图形界面模式的启动入口。参数 `argc` 和 `argv` 来自 `main()` 的命令行参数，透传给 GTK。该函数为非 `static`，可以被外部调用。 |
| 240 | `    GtkApplication *app = gtk_application_new("edu.compiler.frontend",` | 创建一个 `GtkApplication` 实例，第一个参数是应用的唯一标识符（遵循反向域名命名规范 `edu.compiler.frontend`），用于 D-Bus 通信和单实例管理。 |
| 241 | `        G_APPLICATION_DEFAULT_FLAGS);` | 第二个参数是应用标志，`G_APPLICATION_DEFAULT_FLAGS` 表示使用默认行为（标准应用，不特殊处理）。 |
| 242 | `    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);` | 将应用的 `activate` 信号连接到 `activate` 回调函数。当应用被激活（启动）时，`activate` 函数被调用以构建窗口和界面。 |
| 243 | `    g_application_run(G_APPLICATION(app), argc, argv);` | 启动 GTK 应用主循环。此调用会阻塞直到窗口被关闭。`argc` 和 `argv` 传入以支持 GTK 自身的命令行选项（如 `--display`）。 |
| 244 | `    g_object_unref(app);` | 减少 `GtkApplication` 对象的引用计数，释放其占用的内存。GLib/GObject 使用引用计数进行内存管理。 |
| 245 | `}` | 结束 `run_gtk` 函数定义。 |

---

## 9. CLI 模式

### 9.1 `read_file()` — 读取文件全部内容 (第 251–266 行)

| 行号 | 代码 | 讲解 |
|------|------|------|
| 247 | `// ================================================================` | 顶部注释分隔线，用于划出 CLI 模式代码段的边界。 |
| 248 | `// * CLI 模式` | 节标题注释，标记以下代码属于命令行接口（CLI）模式。 |
| 249 | `// * ================================================================` | 底部注释分隔线。 |
| 250 | （空行） | 空行。 |
| 251 | `static char *read_file(const char *filename) {` | 定义 `read_file` 静态函数（实现第 29 行的前置声明）。参数 `filename` 是要读取的源文件路径。返回值是 `malloc` 分配的、以 `\0` 结尾的文件内容字符串，失败返回 NULL。调用方负责 `free` 释放。 |
| 252 | `    FILE *f = fopen(filename, "r");` | 以只读文本模式（`"r"`）打开传入的文件路径，返回 `FILE*` 流指针。 |
| 253 | `    if (!f) {` | 如果文件打开失败（`f == NULL`，可能因为文件不存在或无权限），进入错误处理分支。 |
| 254 | `        fprintf(stderr, "无法打开文件: %s\n", filename);` | 向标准错误流输出错误提示信息，包含无法打开的文件的名称。使用 `stderr` 确保错误信息不会与正常输出混淆。 |
| 255 | `        return NULL;` | 返回 NULL 表示文件读取失败，调用方需检查返回值。 |
| 256 | `    }` | 结束错误处理块。 |
| 257 | `    fseek(f, 0, SEEK_END);` | 将文件位置指针移动到文件末尾。`SEEK_END` 表示相对于文件末尾的偏移（偏移量为 0 即文件末尾），这一步是为了确定文件大小。 |
| 258 | `    long size = ftell(f);` | 获取当前文件位置指针的偏移量。由于上一步移动到了文件末尾，`ftell` 返回的就是文件的总字节数。 |
| 259 | `    fseek(f, 0, SEEK_SET);` | 将文件位置指针重新定位到文件开头（`SEEK_SET` 表示相对于文件开头），为后续读取内容做准备。 |
| 260 | `    char *buf = malloc(size + 1);` | 在堆上分配 `size + 1` 字节的内存。多出的 1 字节用于存放字符串结尾符 `\0`。 |
| 261 | `    if (!buf) { fclose(f); return NULL; }` | 如果内存分配失败（`buf == NULL`），先关闭文件再返回 NULL，防止资源泄漏。 |
| 262 | `    size_t n = fread(buf, 1, size, f);` | 从文件中一次性读取最多 `size` 个块，每块 1 字节，共最多 `size` 字节到 `buf` 中。返回值 `n` 是实际读取的字节数（通常等于 `size`，除非读取过程中出错）。 |
| 263 | `    buf[n] = '\0';` | 在读取内容末尾追加 `\0`，形成合法的 C 字符串。 |
| 264 | `    fclose(f);` | 关闭文件，释放文件句柄资源。 |
| 265 | `    return buf;` | 返回包含文件全部内容的堆内存缓冲区。调用方负责后续 `free` 释放。 |
| 266 | `}` | 结束 `read_file` 函数定义。 |

### 9.2 `read_stdin()` — 从标准输入读取（动态缓冲区）(第 268–281 行)

| 行号 | 代码 | 讲解 |
|------|------|------|
| 268 | `static char *read_stdin() {` | 定义 `read_stdin` 静态函数，从标准输入（stdin）读取用户输入的全部内容。使用动态扩容缓冲区，适合读取任意长度的输入。返回 `malloc` 分配的字符串，调用方负责 `free`。 |
| 269 | `    size_t cap = 4096, len = 0;` | 初始化缓冲区容量 `cap` 为 4 KB，当前有效内容长度 `len` 为 0。 |
| 270 | `    char *buf = malloc(cap);` | 分配初始 4 KB 的堆内存作为输入缓冲区。 |
| 271 | `    if (!buf) return NULL;` | 如果初始内存分配失败，返回 NULL。 |
| 272 | `    printf("请输入源程序 (Ctrl+D 结束):\n");` | 向用户输出交互提示信息，说明输入结束方式（Ctrl+D 发送 EOF）。 |
| 273 | `    while (fgets(buf + len, cap - len, stdin)) {` | 循环调用 `fgets`：从 `buf + len`（缓冲区当前尾部）开始写入，最多写入 `cap - len`（剩余可用空间）字节。当用户按下 Ctrl+D 发送 EOF 时 `fgets` 返回 NULL，循环结束。 |
| 274 | `        len += strlen(buf + len);` | 将本次 `fgets` 实际读入的字符数加到总长度 `len` 上。`strlen(buf + len)` 计算从新写入位置开始的字符串长度（`fgets` 会在末尾加 `\0`）。 |
| 275 | `        if (len + 256 >= cap) {` | 检查剩余空间是否不足 256 字节。256 字节的安全裕度确保下一次 `fgets` 有足够空间读入至少一行完整的输入。 |
| 276 | `            cap *= 2;` | 容量翻倍（4K → 8K → 16K → ...），指数级增长以摊销扩容开销。 |
| 277 | `            buf = realloc(buf, cap);` | 调用 `realloc` 将缓冲区扩容到新容量。`realloc` 会尽量在原地扩展，必要时分配新内存并复制旧数据。 |
| 278 | `        }` | 结束容量检查块。 |
| 279 | `    }` | 结束 while 循环。 |
| 280 | `    return buf;` | 返回包含全部标准输入内容的缓冲区。 |
| 281 | `}` | 结束 `read_stdin` 函数定义。 |

### 9.3 `main()` — 主函数，GUI/CLI 模式路由 (第 283–355 行)

#### 9.3.1 模式判断

| 行号 | 代码 | 讲解 |
|------|------|------|
| 283 | `int main(int argc, char **argv) {` | 程序主入口函数。`argc` 是命令行参数个数（含程序名本身），`argv` 是命令行参数字符串数组。返回值 0 表示正常结束，非 0 表示出错。 |
| 284 | `    if (argc < 2) {` | 如果命令行参数少于 2 个（即仅含程序名，未指定源文件），进入 GUI 模式。 |
| 285 | `        run_gtk(argc, argv);` | 调用 `run_gtk` 启动 GTK3 图形界面。此函数将阻塞直到用户关闭窗口。 |
| 286 | `        return 0;` | GUI 模式结束后正常退出，返回 0。 |
| 287 | `    }` | 结束 GUI 模式分支。若 `argc >= 2`，程序继续执行 CLI 模式。 |
| 288 | （空行） | 空行。 |

#### 9.3.2 CLI 模式：初始化编译器状态

| 行号 | 代码 | 讲解 |
|------|------|------|
| 289 | `    Compiler c;` | 在栈上声明一个 `Compiler` 结构体局部变量 `c`（区别于 GUI 模式的全局 `g_compiler`）。CLI 模式下编译器状态完全保存在栈上。 |
| 290 | `    memset(&c, 0, sizeof(c));` | 将结构体所有字节清零，确保没有被栈上的遗留数据污染（所有字段从已知的 0 状态开始）。 |
| 291 | `    sym_init(&c);` | 调用 `sym_init` 初始化符号表子系统（重置计数器、初始化内部状态等）。 |
| 292 | `    quad_init(&c);` | 调用 `quad_init` 初始化四元式子系统。 |

#### 9.3.3 CLI 模式：读取源代码

| 行号 | 代码 | 讲解 |
|------|------|------|
| 293 | （空行） | 空行。 |
| 294 | `    // 读取源程序` | 注释：标记此段代码的功能是读取待编译的源代码。 |
| 295 | `    char *src;` | 声明一个 `char*` 变量 `src`，用于接收读取到的源代码字符串。 |
| 296 | `    if (argc >= 2) {` | 判断是否有命令行参数（源文件名）。由于第 284 行已过滤 `argc < 2`，此处必定为真。此处的判断是防御性编程，也为可能的未来扩展保留逻辑分支。 |
| 297 | `        src = read_file(argv[1]);` | 调用 `read_file` 读取命令行第一个参数（`argv[1]`）指定的文件内容。 |
| 298 | `        if (!src) return 1;` | 如果文件读取失败（`src == NULL`，如文件不存在），直接返回错误码 1 并终止程序。 |
| 299 | `    } else {` | （当前永远不执行）分支：无命令行参数时的后备处理。 |
| 300 | `        src = read_stdin();` | 从标准输入读取源代码（交互式输入模式）。 |
| 301 | `        if (!src) return 1;` | 如果标准输入读取失败，返回错误码 1。 |
| 302 | `    }` | 结束读取分支。 |
| 303 | （空行） | 空行。 |
| 304 | `    c.source = src;` | 将读取到的源代码字符串指针赋给编译器结构体的 `source` 字段。 |
| 305 | `    c.len = strlen(src);` | 计算源代码长度，存入 `len` 字段。 |
| 306 | `    c.pos = 0;` | 将扫描起始位置重置为 0。 |
| 307 | （空行） | 空行。 |

#### 9.3.4 CLI 模式：输出源程序回显

| 行号 | 代码 | 讲解 |
|------|------|------|
| 308 | `    printf("========== 源程序 ==========\n%s\n", src);` | 在标准输出打印一条分隔线和源代码原文，让用户确认编译器读取到的内容正确无误。这是 CLI 输出格式的惯例，方便调试。 |
| 309 | （空行） | 空行。 |

#### 9.3.5 CLI 模式：编译与输出

| 行号 | 代码 | 讲解 |
|------|------|------|
| 310 | `    // 编译` | 注释：标记下面开始执行编译流程。 |
| 311 | `    int ok = parser_parse(&c);` | 调用语法分析器主函数，执行完整的"词法分析 → 符号表填充 → 语法/语义分析 → 四元式生成"流水线。返回非零表示成功，0 表示失败。 |
| 312 | （空行） | 空行。 |
| 313 | `    if (ok) {` | 如果编译成功，进入成功输出分支。 |

#### 9.3.6 CLI 模式：输出 Token 序列

| 行号 | 代码 | 讲解 |
|------|------|------|
| 314 | `        // 输出 Token 序列` | 注释：标记下面输出词法分析结果。 |
| 315 | `        char buf[16384];` | 声明一个 16 KB 的栈缓冲区 `buf`，复用为各阶段输出缓冲。CLI 模式下使用同一个缓冲区依次打印各阶段结果，内存效率更高。 |
| 316 | `        scanner_dump_tokens(&c, buf, sizeof(buf));` | 将 Token 序列的格式化文本写入 `buf`。 |
| 317 | `        printf("%s", buf);` | 将 Token 序列文本输出到标准输出。 |

#### 9.3.7 CLI 模式：输出符号表

| 行号 | 代码 | 讲解 |
|------|------|------|
| 318 | （空行） | 空行。 |
| 319 | `        // 输出符号表` | 注释：标记下面输出符号表内容。 |
| 320 | `        sym_dump(&c, buf, sizeof(buf));` | 将符号表的格式化文本写入 `buf`（覆盖之前的 Token 内容）。 |
| 321 | `        printf("%s", buf);` | 将符号表文本输出到标准输出。 |

#### 9.3.8 CLI 模式：输出常数表

| 行号 | 代码 | 讲解 |
|------|------|------|
| 322 | （空行） | 空行。 |
| 323 | `        // 输出常数表` | 注释：标记下面输出常数表内容。 |
| 324 | `        const_dump(&c, buf, sizeof(buf));` | 将常数表的格式化文本写入 `buf`。 |
| 325 | `        printf("%s", buf);` | 将常数表文本输出到标准输出。 |

#### 9.3.9 CLI 模式：输出四元式（优化前）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 326 | （空行） | 空行。 |
| 327 | `        // 输出四元式 (优化前)` | 注释：标记下面输出优化前的四元式列表。CLI 模式会分别输出优化前和优化后的四元式，便于观察优化效果。 |
| 328 | `        quad_dump(&c, buf, sizeof(buf));` | 将四元式列表格式化写入 `buf`。 |
| 329 | `        printf("%s", buf);` | 将优化前四元式输出到标准输出。 |

#### 9.3.10 CLI 模式：优化 + 输出优化后四元式

| 行号 | 代码 | 讲解 |
|------|------|------|
| 330 | （空行） | 空行。 |
| 331 | `        // 优化` | 注释：标记下面执行四元式优化。 |
| 332 | `        optimize_run(&c);` | 调用优化器，就地修改 `c` 中的四元式列表（如同步冗余赋值、合并临时变量等）。 |
| 333 | `        printf("\n===== 优化后四元式 =====\n");` | 打印优化后四元式输出的节标题分隔线。 |
| 334 | `        quad_dump(&c, buf, sizeof(buf));` | 再次调用 `quad_dump`，此时输出的四元式已经是优化后的版本。 |
| 335 | `        printf("%s", buf);` | 将优化后四元式输出到标准输出。用户可以对比优化前后的差异。 |

#### 9.3.11 CLI 模式：生成目标代码

| 行号 | 代码 | 讲解 |
|------|------|------|
| 336 | （空行） | 空行。 |
| 337 | `        // 生成 x86-64 目标代码` | 注释：标记下面将中间代码转换为 x86-64 汇编代码。 |
| 338 | `        char asmfile[512];` | 声明一个 512 字节的栈缓冲区 `asmfile`，用于存储输出汇编文件的路径名。 |
| 339 | `        if (argc >= 2) {` | 如果有命令行参数（源文件名），以源文件名为基础生成汇编文件名。 |
| 340 | `            snprintf(asmfile, sizeof(asmfile), "%s.s", argv[1]);` | 将汇编文件命名规则设定为 `源文件名.s`。例如 `test.pas` → `test.pas.s`。 |
| 341 | `        } else {` | 无源文件名的后备处理（当前逻辑下不可达，保留作为防御性代码）。 |
| 342 | `            snprintf(asmfile, sizeof(asmfile), "output.s");` | 默认输出文件名为 `output.s`。 |
| 343 | `        }` | 结束汇编文件命名分支。 |
| 344 | `        codegen_generate(&c, asmfile);` | 调用目标代码生成器，将优化后的四元式转换为 x86-64 汇编代码并写入 `asmfile` 文件。 |
| 345 | `        printf("\n汇编与运行:\n  gcc -no-pie %s -o %s.out && ./%s.out\n",` | 打印便捷的运行提示信息，告知用户如何用 GCC 汇编并运行生成的可执行文件。格式为 `gcc -no-pie 源.s -o 源.s.out && ./源.s.out`。 |
| 346 | `               asmfile, asmfile, asmfile);` | 续行：三个格式化参数均使用 `asmfile`（源文件名.s）。 |
| 347 | （空行） | 空行。 |
| 348 | `        printf("\n========== 编译成功 ==========\n");` | 打印编译成功结束信息的分隔线，表示从词法分析到代码生成全部完成。 |

#### 9.3.12 CLI 模式：编译失败处理

| 行号 | 代码 | 讲解 |
|------|------|------|
| 349 | `    } else {` | 如果 `parser_parse` 返回 0（编译失败），进入错误处理分支。 |
| 350 | `        fprintf(stderr, "编译错误: %s\n", c.error_msg);` | 向标准错误流输出编译错误信息，包含具体的错误原因（由语法分析器在检测到错误时填充到 `c.error_msg` 中）。使用 `stderr` 而非 `stdout` 便于脚本通过退出码和重定向区分正常输出和错误信息。 |
| 351 | `    }` | 结束 `if (ok) ... else ...` 分支。 |
| 352 | （空行） | 空行。 |

#### 9.3.13 清理与退出

| 行号 | 代码 | 讲解 |
|------|------|------|
| 353 | `    free(src);` | 释放 `read_file`（或 `read_stdin`）为源代码文本分配的堆内存。无论编译成功或失败都执行，防止内存泄漏。 |
| 354 | `    return ok ? 0 : 1;` | 根据编译结果返回退出码：成功返回 0（操作系统标准成功退出码），失败返回 1（标准错误退出码）。这使得该程序可以在 shell 脚本中通过 `$?` 或 `&&`/`||` 判断编译结果。 |
| 355 | `}` | 结束 `main` 函数定义。整个 `main.c` 文件结束。 |

---

## 附录：总体程序流程总结

```
main(argc, argv)
  │
  ├─ argc < 2 ─────────────────────────────────────────────────────┐
  │   └─ run_gtk(argc, argv)                                        │
  │       └─ g_application_run() → activate() 构建窗口               │
  │           ├─ [读取文件] → on_open_clicked                        │
  │           │   └─ read_file(fname) → 显示到 src_view              │
  │           ├─ [编译]    → on_compile_clicked                      │
  │           │   ├─ memset/sym_init/quad_init 重置状态               │
  │           │   ├─ 从 src_view 读取源文本                           │
  │           │   ├─ parser_parse() 编译                             │
  │           │   ├─ scanner_dump_tokens  → token_view                │
  │           │   ├─ sym_dump + const_dump → sym_view                 │
  │           │   ├─ quad_dump             → quad_view                │
  │           │   ├─ optimize_run + codegen_generate                  │
  │           │   └─ 读取 .s 文件          → asm_view                 │
  │           └─ [GCC 编译运行] → on_gcc_clicked                     │
  │               ├─ popen("gcc ...") 编译汇编                        │
  │               └─ popen("./a.out") 执行程序 → run_view             │
  │                                                                  │
  └─ argc >= 2 ─────────────────────────────────────────────────────┘
      ├─ Compiler c 初始化 (栈变量)                                  │
      ├─ read_file(argv[1]) 读取源文件                               │
      ├─ parser_parse(&c) 编译                                       │
      ├─ 依次输出: Token序列 → 符号表 → 常数表 → 四元式(优化前)      │
      ├─ optimize_run(&c) 优化                                       │
      ├─ 输出: 四元式(优化后)                                        │
      ├─ codegen_generate(&c, argv[1].s) 生成汇编                    │
      └─ free(src); return ok ? 0 : 1;
```
