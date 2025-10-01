# POSIX Persona Makefile

CC := gcc
CFLAGS := -Wall -Wextra -O2 -I. -I../../../kernel/include
LDFLAGS :=

SOURCES := posix.c test_posix.c
OBJECTS := $(SOURCES:.c=.o)
TARGET := test_posix

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

test: $(TARGET)
	./$(TARGET)

.PHONY: all clean test
