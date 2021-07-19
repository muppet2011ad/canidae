OPTS := -Wall -pedantic -std=c11

MAIN_DEPS := bin/memory.o bin/segment.o bin/main.o bin/debug.o bin/value.o

all: main

bin/%.o: src/%.c
	gcc $(OPTS) -c $< -g -fpic -o $@

main: $(MAIN_DEPS)
	gcc $(OPTS) -g $(MAIN_DEPS) -o bin/main

clean:
	rm bin/*