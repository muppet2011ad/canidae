LIBS := -lm

OPTS := -Wall -pedantic -std=c11

BUILD_FOLDER := bin

DEBUG_OPTS := -DDEBUG_PRINT_CODE -DDEBUG_TRACE_EXECUTION -DDEBUG_LOG_GC

MAIN_DEPS := $(BUILD_FOLDER)/memory.o $(BUILD_FOLDER)/segment.o $(BUILD_FOLDER)/main.o $(BUILD_FOLDER)/debug.o $(BUILD_FOLDER)/value.o $(BUILD_FOLDER)/vm.o $(BUILD_FOLDER)/compiler.o $(BUILD_FOLDER)/scanner.o $(BUILD_FOLDER)/object.o $(BUILD_FOLDER)/hashmap.o $(BUILD_FOLDER)/stdlib_canidae.o

DEBUG_DEPS := $(BUILD_FOLDER)/memory_debug.o $(BUILD_FOLDER)/segment_debug.o $(BUILD_FOLDER)/main_debug.o $(BUILD_FOLDER)/debug_debug.o $(BUILD_FOLDER)/value_debug.o $(BUILD_FOLDER)/vm_debug.o $(BUILD_FOLDER)/compiler_debug.o $(BUILD_FOLDER)/scanner_debug.o $(BUILD_FOLDER)/object_debug.o $(BUILD_FOLDER)/hashmap_debug.o $(BUILD_FOLDER)/stdlib_canidae_debug.o

all: $(BUILD_FOLDER)/canidae $(BUILD_FOLDER)/canidae_debug

$(BUILD_FOLDER)/.sentinel:
	mkdir $(BUILD_FOLDER)
	touch $(BUILD_FOLDER).sentinel

canidae: $(BUILD_FOLDER)/canidae

canidae_debug: $(BUILD_FOLDER)/canidae_debug

$(BUILD_FOLDER)/main_debug.o: src/main.c $(BUILD_FOLDER)/.sentinel
	gcc $(OPTS) $(DEBUG_OPTS) $(LIBS) -c src/main.c -g -fpic -o $(BUILD_FOLDER)/main_debug.o

$(BUILD_FOLDER)/%_debug.o: src/%.c src/common.h src/segment.h src/%.h $(BUILD_FOLDER)/.sentinel
	gcc $(OPTS) $(DEBUG_OPTS) $(LIBS) -c $< -g -fpic -o $@

$(BUILD_FOLDER)/main.o: src/main.c $(BUILD_FOLDER)/.sentinel
	gcc $(OPTS) $(LIBS) -c src/main.c -O3 -fpic -o $(BUILD_FOLDER)/main.o

$(BUILD_FOLDER)/%.o: src/%.c src/common.h src/segment.h src/%.h $(BUILD_FOLDER)/.sentinel
	gcc $(OPTS) $(LIBS) -c $< -O3 -fpic -o $@

$(BUILD_FOLDER)/canidae_debug: $(DEBUG_DEPS) $(BUILD_FOLDER)/.sentinel
	gcc $(OPTS) $(DEBUG_OPTS) $(LIBS) -g  -lm $(DEBUG_DEPS) -o $(BUILD_FOLDER)/canidae_debug

$(BUILD_FOLDER)/canidae: $(MAIN_DEPS) $(BUILD_FOLDER)/.sentinel
	gcc $(OPTS) -O3  $(MAIN_DEPS) $(LIBS) -o $(BUILD_FOLDER)/canidae

test_report: $(BUILD_FOLDER)/canidae test/*
	-python -m pytest > test_report
	cat test_report

clean:
	rm $(BUILD_FOLDER)/*
