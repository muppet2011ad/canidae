OPTS := -Wall -pedantic -std=c11

MAIN_DEPS := bin/memory.o bin/segment.o bin/main.o bin/debug.o bin/value.o bin/vm.o bin/compiler.o bin/scanner.o bin/object.o bin/hashmap.o

all: canidae

bin/%.o: src/%.c src/common.h src/segment.h
	gcc $(OPTS) -c $< -g -fpic -o $@

canidae: $(MAIN_DEPS)
	gcc $(OPTS) -g  -lm $(MAIN_DEPS) -o bin/canidae

clean:
	rm bin/*