CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
TARGET = vclang

all: $(TARGET)

$(TARGET): src/vclang.c src/vclang.h
	$(CC) $(CFLAGS) -o $(TARGET) src/vclang.c

clean:
	rm -f $(TARGET)

test: $(TARGET)
	./tests/run_tests.sh
