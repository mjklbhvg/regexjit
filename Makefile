CFLAGS=-Wall -Wpedantic -g -O3

.PHONY: clean

main: main.o compile.o parse.o asm_snippets.o

clean:
	rm -f main $(wildcard *.o)
