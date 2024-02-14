CFLAGS=-Wall -Wpedantic -g

.PHONY: clean

main: main.o

clean:
	rm -f main $(wildcard *.o)
