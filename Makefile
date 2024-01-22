CC := gcc
SRC := $(shell find . -name '*.c')
TARGETS := $(patsubst %.c,%,$(SRC))
CFLAGS := -g -Wall -Wextra -std=gnu17
WEEKS := $(filter-out $(wildcard week*.tar.gz), $(wildcard week*))

all: $(TARGETS)

%: %.c Makefile
	$(CC) $(CFLAGS) $(shell sed -n '1{/\/\/\(.*\)/s//\1/p}' $<) $< -o $@

clean:
	rm -f $(TARGETS)

pack: $(addsuffix .tar.gz,$(WEEKS))

%.tar.gz: %/*.c
	tar -czvf $@ $^

.PHONY: all clean pack