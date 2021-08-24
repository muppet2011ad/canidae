#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "object.h"
#include "hashmap.h"
#include "value.h"

#define MAX_HASHMAP_LOAD_FACTOR 0.75

void init_hashmap(hashmap *h) {
    h->count = 0;
    h->capacity = 0;
    h->entries = NULL;
}

void destroy_hashmap(hashmap *h, VM *vm) {
    FREE_ARRAY(vm, kv_pair, h->entries, h->capacity);
    init_hashmap(h);
}

static kv_pair *find_entry(kv_pair *entries, uint32_t capacity, object_string *key) {
    uint32_t index = key->hash % capacity;
    kv_pair *tombstone = NULL;
    for (;;) {
        kv_pair *entry = &entries[index];
        if (entry->k == NULL) {
            if (IS_NULL(entry->v)) {
                return tombstone != NULL ? tombstone : entry;
            }
            else {
                if (tombstone == NULL) tombstone = entry;
            }
        }
        else if (entry->k == key) {
            return entry;
        }
        index = (index + 1) % capacity;
    }
}

static void adjust_capacity(hashmap *h, VM *vm, uint32_t capacity) {
    kv_pair *entries = ALLOCATE(vm, kv_pair, capacity);
    for (uint32_t i = 0; i < capacity; i++) {
        entries[i].k = NULL;
        entries[i].v = NULL_VAL;
    }

    h->count = 0;
    for (uint32_t i = 0; i < h->capacity; i++) {
        kv_pair *entry = &h->entries[i];
        if (entry->k == NULL) continue;
        kv_pair *dest = find_entry(entries, capacity, entry->k);
        dest->k = entry->k;
        dest->v = entry->v;
        h->count++;
    }
    FREE_ARRAY(vm, kv_pair, h->entries, h->capacity);
    h->entries = entries;
    h->capacity = capacity;
}

uint8_t hashmap_set(hashmap *h, VM *vm, object_string *key, value val) {
    if (h->count + 1 > h->capacity * MAX_HASHMAP_LOAD_FACTOR) {
        uint32_t capacity = GROW_CAPACITY(h->capacity);
        adjust_capacity(h, vm, capacity);
    } 
    kv_pair *entry = find_entry(h->entries, h->capacity, key);
    uint8_t is_new_key = entry->k == NULL;
    if (is_new_key && IS_NULL(entry->v)) h->count++;
    entry->k = key;
    entry->v = val;
    return is_new_key;
}

uint8_t hashmap_get(hashmap *h, object_string* key, value *val) {
    if (h->count == 0) return 0;
    kv_pair *entry = find_entry(h->entries, h->capacity, key);
    if (entry->k == NULL) return 0;

    *val = entry->v;
    return 1;
}

uint8_t hashmap_delete(hashmap *h, object_string *key) {
    if (h->count == 0) return 0;
    kv_pair *entry = find_entry(h->entries, h->capacity, key);
    if (entry->k == NULL) return 0;

    entry->k = NULL;
    entry->v = BOOL_VAL(1);
    return 1;
}

void hashmap_copy_all(VM *vm, hashmap *from, hashmap *to) {
    for (uint32_t i = 0; i < from->capacity; i++) {
        kv_pair *entry = &from->entries[i];
        if (entry->k != NULL) {
            hashmap_set(to, vm, entry->k, entry->v);
        }
    }
}

object_string *hashmap_find_string(hashmap *h, const char *chars, uint32_t length, uint32_t hash) {
    if (h->count == 0) return NULL;

    uint32_t index = hash % h->capacity;
    for (;;) {
        kv_pair *entry = &h->entries[index];
        if (entry->k == NULL) {
            if (IS_NULL(entry->v)) return NULL;
        }
        else if (entry->k->length == length && entry->k->hash == hash && memcmp(entry->k->chars, chars, length) == 0) {
            return entry->k;
        }

        index = (index + 1) % h->capacity;
    }
}