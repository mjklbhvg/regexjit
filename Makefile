CFLAGS=-Wall -Wpedantic -g

.PHONY: clean

main: main.o compile.o

clean:
	rm -f main $(wildcard *.o)
