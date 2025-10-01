# Installer Makefile

CC := gcc
CFLAGS := -Wall -Wextra -O2 -I.
LDFLAGS :=

SOURCES := main.c
OBJECTS := $(SOURCES:.c=.o)
TARGET := limitless-installer

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
