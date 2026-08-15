CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -Iinclude
TARGET = vclang

all: $(TARGET)

$(TARGET): src/main.c
	$(CC) $(CFLAGS) -o $(TARGET) src/main.c

clean:
	rm -f $(TARGET)

test: $(TARGET)
	./tests/run_tests.sh
