#include "../include/ze.h"
/*
MIT License

Copyright (c) 2021 Andrei Ciobanu

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

A open addressing hash table implemented in C.

Code associated with the following article:
https://www.andreinc.net/2021/10/02/implementing-hash-tables-in-c-part-1

*/

oa_hash *oa_hash_new(oa_key_ops key_ops, oa_val_ops val_ops, void (*probing_fct)(struct oa_hash_s *htable, size_t *from_idx));
oa_hash *oa_hash_new_lp(oa_key_ops key_ops, oa_val_ops val_ops);
void oa_hash_free(oa_hash *htable);
void_t oa_hash_put(oa_hash *htable, const_t key, const_t value);
void_t oa_hash_get(oa_hash *htable, const_t key);
void oa_hash_delete(oa_hash *htable, const_t key);
void oa_hash_print(oa_hash *htable, void (*print_key)(const_t k), void (*print_val)(const_t v));

// Pair related
oa_pair *oa_pair_new(uint32_t hash, const_t key, const_t value);

// String operations
uint32_t oa_string_hash(const_t data, void_t arg);
void_t oa_string_cp(const_t data, void_t arg);
bool oa_string_eq(const_t data1, const_t data2, void_t arg);
void oa_string_free(void_t data, void_t arg);
void oa_string_print(const_t data);

/* Probing functions */
static inline void oa_hash_lp_idx(oa_hash *htable, size_t *idx);

enum oa_ret_ops {
    DEL,
    PUT,
    GET
};

static size_t oa_hash_getidx(oa_hash *htable, size_t idx, uint32_t hash_val, const_t key, enum oa_ret_ops op);
static inline void oa_hash_grow(oa_hash *htable);
static inline bool oa_hash_should_grow(oa_hash *htable);
static inline bool oa_hash_is_tombstone(oa_hash *htable, size_t idx);
static inline void oa_hash_put_tombstone(oa_hash *htable, size_t idx);

oa_hash *oa_hash_new(
    oa_key_ops key_ops,
    oa_val_ops val_ops,
    void (*probing_fct)(struct oa_hash_s *htable, size_t *from_idx)) {
    oa_hash *htable = CO_CALLOC(1, sizeof(*htable));
    if (NULL == htable)
        co_panic("calloc() failed");

    htable->size = 0;
    htable->capacity = HASH_INIT_CAPACITY;
    htable->val_ops = val_ops;
    htable->key_ops = key_ops;
    htable->probing_fct = probing_fct;

    htable->buckets = CO_CALLOC(1, sizeof(*(htable->buckets)) * htable->capacity);
    if (NULL == htable->buckets)
        co_panic("calloc() failed");

    for (int i = 0; i < htable->capacity; i++) {
        htable->buckets[ i ] = NULL;
    }

    return htable;
}

oa_hash *oa_hash_new_lp(oa_key_ops key_ops, oa_val_ops val_ops) {
    return oa_hash_new(key_ops, val_ops, oa_hash_lp_idx);
}

void oa_hash_free(oa_hash *htable) {
    for (int i = 0; i < htable->capacity; i++) {
        if (NULL != htable->buckets[i]) {
            if (htable->buckets[i]->key != NULL)
                htable->key_ops.free(htable->buckets[i]->key, htable->key_ops.arg);
            if (htable->buckets[ i ]->value != NULL)
                htable->val_ops.free(htable->buckets[ i ]->value);

            htable->buckets[ i ]->value = NULL;
            htable->buckets[ i ]->key = NULL;
        }

        CO_FREE(htable->buckets[ i ]);
        htable->buckets[ i ] = NULL;
    }

    if (htable->buckets != NULL)
        CO_FREE(htable->buckets);

    htable->buckets = NULL;
    CO_FREE(htable);
}

inline static void oa_hash_grow(oa_hash *htable) {
    uint32_t old_capacity;
    oa_pair **old_buckets;
    oa_pair *crt_pair;

    uint64_t new_capacity_64 = (uint64_t)htable->capacity * HASH_GROWTH_FACTOR;
    if (new_capacity_64 > SIZE_MAX)
        co_panic("re-size overflow");

    old_capacity = (uint32_t)htable->capacity;
    old_buckets = htable->buckets;

    htable->capacity = (uint32_t)new_capacity_64;
    htable->size = 0;
    htable->buckets = CO_CALLOC(1, htable->capacity * sizeof(*(htable->buckets)));

    if (NULL == htable->buckets)
        co_panic("calloc() failed");

    for (int i = 0; i < htable->capacity; i++) {
        htable->buckets[ i ] = NULL;
    };

    for (size_t i = 0; i < old_capacity; i++) {
        crt_pair = old_buckets[ i ];
        if (NULL != crt_pair && !oa_hash_is_tombstone(htable, i)) {
            oa_hash_put(htable, crt_pair->key, crt_pair->value);
            htable->key_ops.free(crt_pair->key, htable->key_ops.arg);
            htable->val_ops.free(crt_pair->value);
            CO_FREE(crt_pair);
        }
    }

    CO_FREE(old_buckets);
}

inline static bool oa_hash_should_grow(oa_hash *htable) {
    return (htable->size / htable->capacity) > HASH_LOAD_FACTOR;
}

void_t oa_hash_put(oa_hash *htable, const_t key, const_t value) {

    if (oa_hash_should_grow(htable)) {
        oa_hash_grow(htable);
    }

    uint32_t hash_val = htable->key_ops.hash(key, htable->key_ops.arg);
    size_t idx = hash_val % htable->capacity;

    if (NULL == htable->buckets[ idx ]) {
        // Key doesn't exist & we add it anew
        htable->buckets[ idx ] = oa_pair_new(
            hash_val,
            htable->key_ops.cp(key, htable->key_ops.arg),
            htable->val_ops.cp(value, htable->val_ops.arg));
    } else {
        // // Probing for the next good index
        idx = oa_hash_getidx(htable, idx, hash_val, key, PUT);

        if (NULL == htable->buckets[ idx ]) {
            htable->buckets[ idx ] = oa_pair_new(
                hash_val,
                htable->key_ops.cp(key, htable->key_ops.arg),
                htable->val_ops.cp(value, htable->val_ops.arg));
        } else {
            // Update the existing value
            // Free the old values
            htable->val_ops.free(htable->buckets[ idx ]->value);
            htable->key_ops.free(htable->buckets[ idx ]->key, htable->key_ops.arg);
            // Update the new values
            htable->buckets[ idx ]->value = htable->val_ops.cp(value, htable->val_ops.arg);
            htable->buckets[ idx ]->key = htable->val_ops.cp(key, htable->key_ops.arg);
            htable->buckets[ idx ]->hash = hash_val;
            --htable->size;
        }
    }

    htable->size++;
    return htable->buckets[ idx ];
}

void_t oa_hash_replace(oa_hash *htable, const_t key, const_t value) {

    if (oa_hash_should_grow(htable)) {
        oa_hash_grow(htable);
    }

    uint32_t hash_val = htable->key_ops.hash(key, htable->key_ops.arg);
    size_t idx = hash_val % htable->capacity;

    // // Probing for the next good index
    idx = oa_hash_getidx(htable, idx, hash_val, key, PUT);

    if (NULL == htable->buckets[ idx ]) {
        htable->buckets[ idx ] = oa_pair_new(
            hash_val,
            htable->key_ops.cp(key, htable->key_ops.arg),
            htable->val_ops.cp(value, htable->val_ops.arg));
    } else {
        // Update the new values
        memcpy(htable->buckets[ idx ]->value, value, sizeof(htable->buckets[ idx ]->value));
        memcpy(htable->buckets[ idx ]->key, key, sizeof(htable->buckets[ idx ]->key));
        htable->buckets[ idx ]->hash = hash_val;
        --htable->size;
    }

    htable->size++;
    return htable->buckets[ idx ];
}

inline static bool oa_hash_is_tombstone(oa_hash *htable, size_t idx) {
    if (NULL == htable->buckets[ idx ]) {
        return false;
    }
    if (NULL == htable->buckets[ idx ]->key &&
        NULL == htable->buckets[ idx ]->value &&
        0 == htable->buckets[ idx ]->key) {
        return true;
    }
    return false;
}

inline static void oa_hash_put_tombstone(oa_hash *htable, size_t idx) {
    if (NULL != htable->buckets[ idx ]) {
        htable->buckets[ idx ]->hash = 0;
        htable->buckets[ idx ]->key = NULL;
        htable->buckets[ idx ]->value = NULL;
    }
}

void_t oa_hash_get(oa_hash *htable, const_t key) {
    uint32_t hash_val = htable->key_ops.hash(key, htable->key_ops.arg);
    size_t idx = hash_val % htable->capacity;

    if (NULL == htable->buckets[ idx ]) {
        return NULL;
    }

    idx = oa_hash_getidx(htable, idx, hash_val, key, GET);

    return (NULL == htable->buckets[ idx ]) ? NULL : htable->buckets[ idx ]->value;
}

void oa_hash_delete(oa_hash *htable, const_t key) {
    uint32_t hash_val = htable->key_ops.hash(key, htable->key_ops.arg);
    size_t idx = hash_val % htable->capacity;

    if (NULL == htable->buckets[ idx ]) {
        return;
    }

    idx = oa_hash_getidx(htable, idx, hash_val, key, DEL);
    if (NULL == htable->buckets[ idx ]) {
        return;
    }

    htable->val_ops.free(htable->buckets[ idx ]->value);
    htable->key_ops.free(htable->buckets[ idx ]->key, htable->key_ops.arg);
    --htable->size;

    oa_hash_put_tombstone(htable, idx);
}

void oa_hash_remove(oa_hash *htable, const_t key) {
    uint32_t hash_val = htable->key_ops.hash(key, htable->key_ops.arg);
    size_t idx = hash_val % htable->capacity;

    if (NULL == htable->buckets[ idx ]) {
        return;
    }

    if (htable->buckets[idx]->value)
        htable->buckets[idx]->value = NULL;
    --htable->size;
}

void oa_hash_print(oa_hash *htable, void (*print_key)(const_t k), void (*print_val)(const_t v)) {

    oa_pair *pair;

    printf("Hash Capacity: %zu\n", (size_t)htable->capacity);
    printf("Hash Size: %zu\n", htable->size);

    printf("Hash Buckets:\n");
    for (int i = 0; i < htable->capacity; i++) {
        pair = htable->buckets[ i ];
        printf("\tbucket[%d]:\n", i);
        if (NULL != pair) {
            if (oa_hash_is_tombstone(htable, i)) {
                printf("\t\t TOMBSTONE");
            } else {
                printf("\t\thash=%" PRIu32 ", key=", pair->hash);
                print_key(pair->key);
                printf(", value=");
                print_val(pair->value);
            }
        }
        printf("\n");
    }
}

static size_t oa_hash_getidx(oa_hash *htable, size_t idx, uint32_t hash_val, const_t key, enum oa_ret_ops op) {
    do {
        if (op == PUT && oa_hash_is_tombstone(htable, idx)) {
            break;
        }
        if (htable->buckets[ idx ]->hash == hash_val &&
            htable->key_ops.eq(key, htable->buckets[ idx ]->key, htable->key_ops.arg)) {
            break;
        }
        htable->probing_fct(htable, &idx);
    } while (NULL != htable->buckets[ idx ]);
    return idx;
}

oa_pair *oa_pair_new(uint32_t hash, const_t key, const_t value) {
    oa_pair *p = CO_CALLOC(1, sizeof(*p) + sizeof(key) + sizeof(value) + 2);
    if (NULL == p)
        co_panic("calloc() failed");

    p->hash = hash;
    p->value = (void_t)value;
    p->key = (void_t)key;
    return p;
}

// Probing functions
static inline void oa_hash_lp_idx(oa_hash *htable, size_t *idx) {
    (*idx)++;
    if ((*idx) == htable->capacity) {
        (*idx) = 0;
    }
}

bool oa_string_eq(const_t data1, const_t data2, void_t arg) {
    string_t str1 = (string_t)data1;
    string_t str2 = (string_t)data2;
    return !(strcmp(str1, str2)) ? true : false;
}

// String operations
static uint32_t oa_hash_fmix32(uint32_t h) {
    h ^= h >> 16;
    h *= 0x3243f6a9U;
    h ^= h >> 16;
    return h;
}

uint32_t oa_string_hash(const_t data, void_t arg) {

    // djb2
    uint32_t hash = (const uint32_t)5381;
    string_t str = (string_t)data;
    char c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return oa_hash_fmix32(hash);
}

void_t oa_string_cp(const_t data, void_t arg) {
    string_t input = (string_t)data;
    size_t input_length = strlen(input) + 1;
    char *result = CO_CALLOC(1, sizeof(*result) * input_length + sizeof(co_value_t) + 1);
    if (NULL == result)
        co_panic("calloc() failed");

    co_strcpy(result, input, input_length);
    return result;
}

bool oa_coroutine_eq(const_t data1, const_t data2, void_t arg) {
    return memcmp(data1, data2, sizeof(co_active())) == 0 ? true : false;
}

void_t oa_coroutine_cp(const_t data, void_t arg) {
    return (co_routine_t *)data;
}

void_t oa_channel_cp(const_t data, void_t arg) {
    return (channel_t *)data;
}

void_t oa_value_cp(const_t data, void_t arg) {
    co_value_t *result = CO_CALLOC(1, sizeof(data) + sizeof(co_value_t) + 1);
    if (NULL == result)
        co_panic("calloc() failed");

    memcpy(result, data, sizeof(data));
    return result;
}

bool oa_value_eq(const_t data1, const_t data2, void_t arg) {
    return memcmp(data1, data2, sizeof(data2)) == 0 ? true : false;
}

void oa_string_free(void_t data, void_t arg) {
    CO_FREE(data);
}

void oa_string_print(const_t data) {
    printf("%s", (string_t)data);
}

void oa_map_free(void_t data) {}

void_t oa_map_cp(const_t data, void_t arg) {
    map_value_t *result = CO_CALLOC(1, sizeof(data) + sizeof(map_value_t) + 1);
    if (NULL == result)
        co_panic("calloc() failed");

    memcpy(result, data, sizeof(data));
    return result;
}

void_t oa_map_cp_long(const_t data, void_t arg) {
    int64_t *result = CO_CALLOC(1, sizeof(data) + sizeof(map_value_t));
    if (NULL == result)
        co_panic("calloc() failed");

    memcpy(result, data, sizeof(data));
    return result;
}

oa_key_ops oa_key_ops_string = { oa_string_hash, oa_string_cp, oa_string_free, oa_string_eq, NULL };
oa_val_ops oa_val_ops_struct = { oa_coroutine_cp, FUNC_VOID(co_delete), oa_value_eq, NULL };
oa_val_ops oa_val_ops_string = { oa_string_cp, CO_FREE, oa_string_eq, NULL };
oa_val_ops oa_val_ops_value = { oa_value_cp, CO_FREE, oa_value_eq, NULL };
oa_val_ops oa_val_ops_map_long = { oa_map_cp_long, CO_FREE, oa_value_eq, NULL };
oa_val_ops oa_val_ops_map = {oa_map_cp, CO_FREE, oa_value_eq, NULL};
oa_val_ops oa_val_ops_channel = {oa_channel_cp, FUNC_VOID(channel_free), oa_value_eq, NULL};

CO_FORCE_INLINE wait_group_t *co_ht_group_init() {
    return (wait_group_t *)oa_hash_new(oa_key_ops_string, oa_val_ops_struct, oa_hash_lp_idx);
}

CO_FORCE_INLINE wait_result_t *co_ht_result_init() {
    return (wait_result_t *)oa_hash_new(oa_key_ops_string, oa_val_ops_value, oa_hash_lp_idx);
}

CO_FORCE_INLINE gc_channel_t *co_ht_channel_init() {
    return (gc_channel_t *)oa_hash_new(oa_key_ops_string, oa_val_ops_channel, oa_hash_lp_idx);
}

CO_FORCE_INLINE ht_map_t *co_ht_map_init() {
    return (ht_map_t *)oa_hash_new(oa_key_ops_string, oa_val_ops_map, oa_hash_lp_idx);
}

CO_FORCE_INLINE ht_map_t *co_ht_map_long_init() {
    return (ht_map_t *)oa_hash_new(oa_key_ops_string, oa_val_ops_map_long, oa_hash_lp_idx);
}

CO_FORCE_INLINE ht_map_t *co_ht_map_string_init() {
    return (ht_map_t *)oa_hash_new(oa_key_ops_string, oa_val_ops_string, oa_hash_lp_idx);
}

CO_FORCE_INLINE void co_hash_free(co_hast_t *htable) {
    oa_hash_free(htable);
}

CO_FORCE_INLINE void_t co_hash_put(co_hast_t *htable, const_t key, const_t value) {
    return oa_hash_put(htable, key, value);
}

CO_FORCE_INLINE void_t co_hash_replace(co_hast_t *htable, const_t key, const_t value) {
    return oa_hash_replace(htable, key, value);
}

CO_FORCE_INLINE void_t co_hash_get(co_hast_t *htable, const_t key) {
    return oa_hash_get(htable, key);
}

CO_FORCE_INLINE void co_hash_delete(co_hast_t *htable, const_t key) {
    oa_hash_delete(htable, key);
}

CO_FORCE_INLINE void co_hash_remove(co_hast_t *htable, const_t key) {
    oa_hash_remove(htable, key);
}

CO_FORCE_INLINE void co_hash_print(co_hast_t *htable, void (*print_key)(const_t k), void (*print_val)(const_t v)) {
    oa_hash_print(htable, print_key, print_val);
}
