OPTS := -Wall -pedantic -std=c11

bin/%.o: src/%.c
	gcc $(OPTS) -c $< -g -fpic -o $@

main: bin/memory.o bin/segment.o bin/main.o bin/debug.o
	gcc $(OPTS) -g bin/memory.o bin/segment.o bin/debug.o bin/main.o -o bin/main