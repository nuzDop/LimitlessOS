# Settings Application Makefile

CC := gcc
CFLAGS := -Wall -Wextra -O2 -I. -I../../kernel/include
LDFLAGS :=

SOURCES := settings.c main.c
OBJECTS := $(SOURCES:.c=.o)
TARGET := limitless-settings

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
