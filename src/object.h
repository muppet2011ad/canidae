#ifndef canidae_object_h

#define canidae_object_h

#include "common.h"
#include "value.h"

#define GET_OBJ_TYPE(v) (AS_OBJ(v)->type)
#define IS_STRING(v) is_obj_type(v, OBJ_STRING)

#define AS_STRING(v) ((object_string*)AS_OBJ(v))
#define AS_CSTRING(v) (((object_string*)AS_OBJ(v))->chars)

typedef enum {
    OBJ_STRING,
} object_type;

struct object {
    object_type type;
    struct object *next;
};

struct object_string {
    object obj;
    uint32_t length;
    char *chars;
};

object_string *take_string(object **objects, char *chars, uint32_t length);
object_string *copy_string(object **objects, const char *chars, uint32_t length);
void print_object(value v);

static inline uint8_t is_obj_type(value v, object_type type) {
    return IS_OBJ(v) && AS_OBJ(v)->type == type;
}

#endif