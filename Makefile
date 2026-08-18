CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -g -Iinclude
TARGET  = vclang

# Source files for separate compilation (X-1 fix)
SRCS    = src/memory.c src/opcodes.c src/ast.c src/lexer.c src/parser.c src/interpreter.c src/main.c
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Pattern rule for .c -> .o
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Header dependencies (incremental builds rebuild only what changed)
src/memory.o:      src/memory.c include/memory.h include/interpreter.h
src/opcodes.o:     src/opcodes.c include/opcodes.h
src/ast.o:         src/ast.c include/ast.h include/memory.h include/opcodes.h
src/lexer.o:       src/lexer.c include/lexer.h include/memory.h
src/parser.o:      src/parser.c include/parser.h include/ast.h include/lexer.h include/opcodes.h
src/interpreter.o: src/interpreter.c include/interpreter.h include/ast.h include/parser.h include/opcodes.h
src/main.o:        src/main.c include/interpreter.h include/parser.h include/opcodes.h

clean:
	rm -f $(TARGET) src/*.o

test: $(TARGET)
	./tests/run_tests.sh

.PHONY: all clean test
