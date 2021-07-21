#ifndef canidae_hashmap_h

#define canidae_hashmap_h

#include "common.h"
#include "value.h"

typedef struct {
    object_string *k;
    value v;
} kv_pair;

typedef struct {
    uint32_t count;
    uint32_t capacity;
    kv_pair *entries;
} hashmap;

void init_hashmap(hashmap *h);
void destroy_hashmap(hashmap *h);
uint8_t hashmap_set(hashmap *h, object_string *key, value val);
uint8_t hashmap_get(hashmap *h, object_string *key, value *val);
uint8_t hashmap_delete(hashmap *h, object_string *key);
void hashmap_copy_all(hashmap *from, hashmap *to);
object_string *hashmap_find_string(hashmap *h, const char *chars, uint32_t length, uint32_t hash);

#endif