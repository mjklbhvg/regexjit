CFLAGS=-Wall -Wpedantic -g

.PHONY: clean

main: main.o compile.o parse.o asm_snippets.o

clean:
	rm -f main $(wildcard *.o)
