OPTS := -Wall -pedantic -std=c11

MAIN_DEPS := bin/memory.o bin/segment.o bin/main.o bin/debug.o bin/value.o bin/vm.o bin/compiler.o bin/scanner.o

all: main

bin/%.o: src/%.c
	gcc $(OPTS) -c $< -g -fpic -o $@

main: $(MAIN_DEPS)
	gcc $(OPTS) -g  -lm $(MAIN_DEPS) -o bin/main

clean:
	rm bin/*