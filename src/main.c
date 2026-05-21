// * main.c — 编译器前端主程序
// *
// * 对应课程要求: 编译器前端设计与实现
// *
// * 模块对应:
// *   [词法分析] -> scanner.c (常数自动机 + 关键字/界符表)
// *   [符号表]   -> symbol.c  (标识符/常数查填, 临时变量分配)
// *   [四元式]   -> quadruple.c (中间代码生成)
// *   [语法分析] -> parser.c (递归下降子程序 + 表达式分析)
// *   [语义分析] -> parser.c 内置语义动作 (a1~a6 翻译文法)
// *
// * 运行方式:
// *   编译: make
// *   运行: ./compiler <source_file>         (CLI 模式)
// *         ./compiler                        (CLI 交互模式)
// *
// * 如有 GTK3: make GUI=1 编译图形界面版本

#include "grammar.h"
#include "scanner.h"
#include "symbol.h"
#include "quadruple.h"
#include "parser.h"
#include "optimize.h"
#include "codegen.h"

#include <gtk/gtk.h>

static char *read_file(const char *filename);

// ================================================================
// * GTK3 图形界面
// * ================================================================

static Compiler g_compiler;
static GtkWidget *src_view, *token_view, *sym_view, *quad_view, *asm_view, *run_view;
static GtkWidget *parent_window;
static int g_compiled_ok;

static void on_open_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "打开源文件", GTK_WINDOW(parent_window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "取消", GTK_RESPONSE_CANCEL,
        "打开", GTK_RESPONSE_ACCEPT,
        NULL);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        char *src = read_file(fname);
        if (src) {
            GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(src_view));
            gtk_text_buffer_set_text(buf, src, -1);
            free(src);
        }
        g_free(fname);
    }
    gtk_widget_destroy(dialog);
}

static void on_compile_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    GtkTextBuffer *buf;
    char *src_text;
    char out_buf[16384];

    memset(&g_compiler, 0, sizeof(g_compiler));
    sym_init(&g_compiler);
    quad_init(&g_compiler);

    // 读取源代码
    buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(src_view));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);
    src_text = gtk_text_buffer_get_text(buf, &start, &end, FALSE);

    g_compiler.source = src_text;
    g_compiler.len = strlen(src_text);
    g_compiler.pos = 0;

    // 编译
    int ok = parser_parse(&g_compiler);

    if (ok) {
        // 输出 Token 序列
        char tbuf[8192];
        scanner_dump_tokens(&g_compiler, tbuf, sizeof(tbuf));
        buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(token_view));
        gtk_text_buffer_set_text(buf, tbuf, -1);

        // 输出符号表 + 常数表
        char sbuf[8192], cbuf[4096];
        sym_dump(&g_compiler, sbuf, sizeof(sbuf));
        const_dump(&g_compiler, cbuf, sizeof(cbuf));
        snprintf(out_buf, sizeof(out_buf), "%s%s", sbuf, cbuf);
        buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(sym_view));
        gtk_text_buffer_set_text(buf, out_buf, -1);

        // 输出四元式
        char qbuf[8192];
        quad_dump(&g_compiler, qbuf, sizeof(qbuf));
        buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(quad_view));
        gtk_text_buffer_set_text(buf, qbuf, -1);

        // 优化 + 生成汇编
        optimize_run(&g_compiler);
        codegen_generate(&g_compiler, "/tmp/opencode/compiler_output.s");

        // 显示汇编代码
        FILE *af = fopen("/tmp/opencode/compiler_output.s", "r");
        if (af) {
            char abuf[16384];
            size_t alen = fread(abuf, 1, sizeof(abuf) - 1, af);
            abuf[alen] = '\0';
            fclose(af);
            buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(asm_view));
            gtk_text_buffer_set_text(buf, abuf, -1);
        }

        g_compiled_ok = 1;
    } else {
        snprintf(out_buf, sizeof(out_buf), "编译错误:\n%s", g_compiler.error_msg);
        buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(token_view));
        gtk_text_buffer_set_text(buf, out_buf, -1);
        g_compiled_ok = 0;
    }

    g_free(src_text);
}

static void on_gcc_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    if (!g_compiled_ok) {
        GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(run_view));
        gtk_text_buffer_set_text(buf, "请先点\"编译\"编译成功后再运行 GCC\n", -1);
        return;
    }

    char result[32768];
    int pos = 0;

    // 运行 gcc 编译
    pos += snprintf(result + pos, sizeof(result) - pos,
        "===== GCC 编译 =====\n");
    FILE *gcc = popen(
        "gcc -no-pie /tmp/opencode/compiler_output.s -o /tmp/opencode/compiler_output 2>&1",
        "r");
    if (gcc) {
        char line[512];
        while (fgets(line, sizeof(line), gcc))
            pos += snprintf(result + pos, sizeof(result) - pos, "%s", line);
        int status = pclose(gcc);
        if (status == 0) {
            pos += snprintf(result + pos, sizeof(result) - pos,
                "GCC 编译成功\n\n");
            // 运行可执行文件
            pos += snprintf(result + pos, sizeof(result) - pos,
                "===== 程序运行输出 =====\n");
            FILE *run = popen("/tmp/opencode/compiler_output 2>&1", "r");
            if (run) {
                while (fgets(line, sizeof(line), run))
                    pos += snprintf(result + pos, sizeof(result) - pos, "%s", line);
                pclose(run);
            }
        } else {
            pos += snprintf(result + pos, sizeof(result) - pos,
                "\nGCC 编译失败 (退出码 %d)\n", status);
        }
    }

    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(run_view));
    gtk_text_buffer_set_text(buf, result, -1);
}

static GtkWidget *create_output_area(const char *title, GtkWidget *notebook) {
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    GtkWidget *view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);
    gtk_container_add(GTK_CONTAINER(scrolled), view);
    GtkWidget *label = gtk_label_new(title);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scrolled, label);
    return view;
}

static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    GtkWidget *window = gtk_application_window_new(app);
    parent_window = window;
    gtk_window_set_title(GTK_WINDOW(window), "编译器前端 - Compiler Frontend");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);

    // 主布局: 垂直
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // 上半部分: 源代码输入
    GtkWidget *src_label = gtk_label_new("源程序 (Source Code):");
    gtk_box_pack_start(GTK_BOX(vbox), src_label, FALSE, FALSE, 0);
    GtkWidget *src_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(src_scrolled),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(src_scrolled, -1, 200);
    src_view = gtk_text_view_new();
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(src_view), TRUE);
    gtk_container_add(GTK_CONTAINER(src_scrolled), src_view);
    gtk_box_pack_start(GTK_BOX(vbox), src_scrolled, FALSE, FALSE, 0);

    // 按钮行
    GtkWidget *hbox_btn = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *btn_open = gtk_button_new_with_label("读取文件");
    g_signal_connect(btn_open, "clicked", G_CALLBACK(on_open_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_btn), btn_open, FALSE, FALSE, 0);
    GtkWidget *btn_compile = gtk_button_new_with_label("编译 (Compile)");
    g_signal_connect(btn_compile, "clicked", G_CALLBACK(on_compile_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_btn), btn_compile, FALSE, FALSE, 0);
    GtkWidget *btn_gcc = gtk_button_new_with_label("GCC 编译运行");
    g_signal_connect(btn_gcc, "clicked", G_CALLBACK(on_gcc_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_btn), btn_gcc, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_btn, FALSE, FALSE, 5);

    // 下半部分: 输出 (Notebook)
    GtkWidget *notebook = gtk_notebook_new();
    token_view = create_output_area("Token 序列", notebook);
    sym_view   = create_output_area("符号表/常数表", notebook);
    quad_view  = create_output_area("四元式(中间代码)", notebook);
    asm_view   = create_output_area("汇编代码", notebook);
    run_view   = create_output_area("运行输出", notebook);
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

    gtk_widget_show_all(window);
}

void run_gtk(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("edu.compiler.frontend",
        G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
}

// ================================================================
// * CLI 模式
// * ================================================================

static char *read_file(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "无法打开文件: %s\n", filename);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, size, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static char *read_stdin() {
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    printf("请输入源程序 (Ctrl+D 结束):\n");
    while (fgets(buf + len, cap - len, stdin)) {
        len += strlen(buf + len);
        if (len + 256 >= cap) {
            cap *= 2;
            buf = realloc(buf, cap);
        }
    }
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        run_gtk(argc, argv);
        return 0;
    }

    Compiler c;
    memset(&c, 0, sizeof(c));
    sym_init(&c);
    quad_init(&c);

    // 读取源程序
    char *src;
    if (argc >= 2) {
        src = read_file(argv[1]);
        if (!src) return 1;
    } else {
        src = read_stdin();
        if (!src) return 1;
    }

    c.source = src;
    c.len = strlen(src);
    c.pos = 0;

    printf("========== 源程序 ==========\n%s\n", src);

    // 编译
    int ok = parser_parse(&c);

    if (ok) {
        // 输出 Token 序列
        char buf[16384];
        scanner_dump_tokens(&c, buf, sizeof(buf));
        printf("%s", buf);

        // 输出符号表
        sym_dump(&c, buf, sizeof(buf));
        printf("%s", buf);

        // 输出常数表
        const_dump(&c, buf, sizeof(buf));
        printf("%s", buf);

        // 输出四元式 (优化前)
        quad_dump(&c, buf, sizeof(buf));
        printf("%s", buf);

        // 优化
        optimize_run(&c);
        printf("\n===== 优化后四元式 =====\n");
        quad_dump(&c, buf, sizeof(buf));
        printf("%s", buf);

        // 生成 x86-64 目标代码
        char asmfile[512];
        if (argc >= 2) {
            snprintf(asmfile, sizeof(asmfile), "%s.s", argv[1]);
        } else {
            snprintf(asmfile, sizeof(asmfile), "output.s");
        }
        codegen_generate(&c, asmfile);
        printf("\n汇编与运行:\n  gcc -no-pie %s -o %s.out && ./%s.out\n",
               asmfile, asmfile, asmfile);

        printf("\n========== 编译成功 ==========\n");
    } else {
        fprintf(stderr, "编译错误: %s\n", c.error_msg);
    }

    free(src);
    return ok ? 0 : 1;
}
