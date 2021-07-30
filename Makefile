OPTS := -Wall -pedantic -std=c11

DEBUG_OPTS := -DDEBUG_PRINT_CODE

MAIN_DEPS := bin/memory.o bin/segment.o bin/main.o bin/debug.o bin/value.o bin/vm.o bin/compiler.o bin/scanner.o bin/object.o bin/hashmap.o

DEBUG_DEPS := bin/memory_debug.o bin/segment_debug.o bin/main_debug.o bin/debug_debug.o bin/value_debug.o bin/vm_debug.o bin/compiler_debug.o bin/scanner_debug.o bin/object_debug.o bin/hashmap_debug.o

all: bin/canidae bin/canidae_debug

canidae: bin/canidae

canidae_debug: bin/canidae_debug

bin/main_debug.o: src/main.c
	gcc $(OPTS) $(DEBUG_OPTS) -c src/main.c -g -fpic -o bin/main_debug.o

bin/%_debug.o: src/%.c src/common.h src/segment.h src/%.h
	gcc $(OPTS) $(DEBUG_OPTS) -c $< -g -fpic -o $@

bin/main.o: src/main.c
	gcc $(OPTS) -c src/main.c -O3 -fpic -o bin/main.o

bin/%.o: src/%.c src/common.h src/segment.h src/%.h
	gcc $(OPTS) -c $< -O3 -fpic -o $@

bin/canidae_debug: $(DEBUG_DEPS)
	gcc $(OPTS) $(DEBUG_OPTS) -g  -lm $(DEBUG_DEPS) -o bin/canidae_debug

bin/canidae: $(MAIN_DEPS)
	gcc $(OPTS) -O3  -lm $(MAIN_DEPS) -o bin/canidae

test_report: bin/canidae test
	-python -m pytest > test_report

clean:
	rm bin/*