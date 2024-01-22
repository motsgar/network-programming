CC := gcc
SRC := $(shell find . -name '*.c')
TARGETS := $(patsubst %.c,%,$(SRC))
CFLAGS := -g -Wall -Wextra -std=gnu17

all: $(TARGETS)

%: %.c Makefile
	$(CC) $(CFLAGS) $(shell sed -n '1{/\/\/\(.*\)/s//\1/p}' $<) $< -o $@

clean:
	rm -f $(TARGETS)

.PHONY: all clean
