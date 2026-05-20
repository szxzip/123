CC       = gcc
CFLAGS   = -Wall -Wextra -std=c99 -g -O2
LDFLAGS  = -lm
TARGET   = compiler
SRCDIR   = src
OBJDIR   = build
SOURCES  = $(wildcard $(SRCDIR)/*.c)
OBJECTS  = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))

# GTK3 支持 (可选): make GUI=1
ifdef GUI
	CFLAGS  += -DUSE_GTK $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
	LDFLAGS += $(shell pkg-config --libs gtk+-3.0 2>/dev/null) -lm
endif

.PHONY: all clean test run asm

all: $(TARGET)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: $(TARGET)
	@echo "--- 测试1: 基本算术 ---"
	./$(TARGET) test/sample1.txt
	@echo ""
	@echo "--- 测试2: IF/WHILE ---"
	./$(TARGET) test/sample2.txt
	@echo ""
	@echo "--- 测试3: 复杂表达式 ---"
	./$(TARGET) test/sample3.txt

# 汇编 + 链接 + 运行
asm: $(TARGET) test/sample1.txt
	./$(TARGET) test/sample1.txt
	@echo ""
	@echo "=== 汇编 ==="
	gcc -no-pie test/sample1.txt.s -o test/sample1.out
	@echo "=== 运行 ==="
	./test/sample1.out

run: $(TARGET)
	./$(TARGET) test/sample1.txt

clean:
	rm -rf $(OBJDIR) $(TARGET) test/*.s test/*.out
