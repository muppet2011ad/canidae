#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "segment.h"
#include "debug.h"
#include "vm.h"

static void repl(VM *vm) {
    char line[4096];
    for (;;) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }
        interpret(vm, line);
    }
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }
    fseek(f, 0L, SEEK_END);
    size_t file_size = ftell(f);
    rewind(f);

    char *buffer = malloc(file_size+1);
    if (buffer == NULL) {
        fprintf(stderr, "Out of memory.\n");
        exit(74);
    }
    size_t bytes_read_actual = fread(buffer, sizeof(char), file_size, f);
    if (bytes_read_actual < file_size) {
        fprintf(stderr, "Error reading source file.\n");
    }
    buffer[bytes_read_actual] = '\0';
    fclose(f);
    return buffer;
}

static void run_file(VM *vm, const char *path) {
    char *source = read_file(path);
    interpret_result result = interpret(vm, source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, const char *argv[]) {
    VM vm;
    init_VM(&vm);
    
    if (argc == 1) {
        repl(&vm);
    } else if (argc == 2) {
        vm.source_path = argv[1];
        run_file(&vm, argv[1]);
    } else {
        fprintf(stderr, "Usage: canidae [file]\n");
        exit(64);
    }

    destroy_VM(&vm);
    return 0;
}