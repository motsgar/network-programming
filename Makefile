CC := gcc
SRC := $(shell find . -name '*.c')
TARGETS := $(patsubst %.c,%,$(SRC))
CFLAGS := -g -Wall -Wextra -std=c17

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $(shell sed -n '1{/\/\/\(.*\)/s//\1/p}' $<) $< -o $@

clean:
	rm -f $(TARGETS)

.PHONY: all clean
