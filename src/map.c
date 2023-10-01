#include "../include/ze.h"

static void slice_free(array_t *array) {
    array_item_t *next;
    slice_t *each;

    if (!array)
        return;

    for (int i = 0; i <= array->no_slices; i++) {
        each = array->slice[ array->no_slices - i ];
        if (each) {
            while (each->head) {
                ZE_FREE((void_t )each->head->key);
                next = each->head->next;
                ZE_FREE(each->head);
                each->head = next;
            }
            ZE_FREE(each);
        }
    }

    ZE_FREE(array->slice);
}

static void slice_set(slice_t *array, string_t key, map_value_t *value, int64_t index) {
    array_item_t *item = (array_item_t *)try_calloc(1, sizeof(array_item_t));
    size_t copy_size = strlen(key) + 1;
    char *result = (char *)try_calloc(1, copy_size + 1);

    co_strcpy(result, key, copy_size);

    item->indic = index;
    item->key = result;
    item->value = value;
    item->prev = array->tail;
    item->next = NULL;

    array->tail = item;
    array->length++;

    if (!array->head)
        array->head = item;
    else
        item->prev->next = item;
}

slice_t *slice(array_t *array, int64_t start, int64_t end) {
    if (array->as != MAP_ARRAY)
        co_panic("slice() only accept `array` type!");

    if (array->no_slices % 64 == 0) {
        array->slice = ZE_REALLOC(array->slice, (array->no_slices + 64) * sizeof(array->slice[ 0 ]));
        if (array->slice == NULL)
            co_panic("realloc() failed");
    }

    slice_t *slice = (slice_t *)try_calloc(1, sizeof(slice_t));
    int64_t index = 0;
    for (int64_t i = start; i < end; i++) {
        string_t key = co_itoa(i);
        slice_set(slice, key, map_get(array, key), index);
        index++;
    }

    slice->sliced = true;
    slice->type = array->type;
    slice->dict = array->dict;
    slice->item_type = array->item_type;
    array->slice[ array->no_slices++ ] = slice;
    array->slice[ array->no_slices ] = NULL;

    return slice;
}

string_t slice_find(map_t *array, int64_t index) {
    array_item_t *item;

    if (!array)
        return NULL;

    if (!array->sliced)
        return co_itoa(index);

    for (item = array->head; item != NULL; item = item->next) {
        if (item->indic == index) {
            return item->key;
        }
    }

    return NULL;
}

map_t *map_new(map_value_dtor dtor) {
    map_t *array = (map_t *)try_calloc(1, sizeof(map_t));
    array->started = false;
    array->dtor = dtor;
    array->type = ZE_MAP_STRUCT;
    array->dict = ht_map_init();
    return array;
}

map_t *map_long_init() {
    map_t *array = (map_t *)try_calloc(1, sizeof(map_t));
    array->started = false;
    array->dtor = NULL;
    array->type = ZE_MAP_STRUCT;
    array->dict = ht_map_long_init();
    return array;
}

map_t *map_string_init() {
    map_t *array = (map_t *)try_calloc(1, sizeof(map_t));
    array->started = false;
    array->dtor = NULL;
    array->type = ZE_MAP_STRUCT;
    array->dict = ht_map_string_init();
    return array;
}

array_t *array(map_value_dtor dtor, int n_args, ...) {
    array_t *array = map_new(dtor);
    va_list argp;
    void_t p;

    array->no_slices = 0;
    va_start(argp, n_args);
    for (int i = 0; i < n_args; i++) {
        p = va_arg(argp, void_t );
        map_push(array, p);
    }
    va_end(argp);

    array->as = MAP_ARRAY;
    return array;
}

array_t *range(int start, int stop) {
    array_t *array = map_long_init();

    array->no_slices = 0;
    array->item_type = ZE_LLONG;
    for (int i = start; i < stop; i++) {
        map_push(array, &i);
    }

    array->as = MAP_ARRAY;
    return array;
}

array_t *array_long(int n_args, ...) {
    array_t *array = map_long_init();
    va_list argp;
    int64_t p;

    array->no_slices = 0;
    array->item_type = ZE_LLONG;
    va_start(argp, n_args);
    for (int i = 0; i < n_args; i++) {
        p = va_arg(argp, int64_t);
        map_push(array, &p);
    }
    va_end(argp);

    array->as = MAP_ARRAY;
    return array;
}

array_t *array_str(int n_args, ...) {
    array_t *array = map_string_init();
    va_list argp;
    char *s;

    array->no_slices = 0;
    array->item_type = ZE_STRING;
    va_start(argp, n_args);
    for (int i = 0; i < n_args; i++) {
        s = va_arg(argp, char *);
        map_push(array, s);
    }
    va_end(argp);

    array->as = MAP_ARRAY;
    return array;
}

map_t *map_long(int n_of_pairs, ...) {
    map_t *array = map_long_init();
    va_list argp;
    string_t k;
    int64_t p;

    array->item_type = ZE_LLONG;
    va_start(argp, n_of_pairs);
    for (int i = 0; i < (n_of_pairs * 2); i = i + 2) {
        k = va_arg(argp, char *);
        p = va_arg(argp, int64_t);
        map_put(array, k, &p);
    }
    va_end(argp);

    array->as = MAP_HASH;
    return array;
}

map_t *map_str(int n_of_pairs, ...) {
    map_t *array = map_string_init();
    va_list argp;
    string_t k;
    char *s;

    array->item_type = ZE_STRING;
    va_start(argp, n_of_pairs);
    for (int i = 0; i < (n_of_pairs * 2); i = i + 2) {
        k = va_arg(argp, char *);
        s = va_arg(argp, char *);
        map_put(array, k, s);
    }
    va_end(argp);

    array->as = MAP_HASH;
    return array;
}

map_t *map_for(map_value_dtor dtor, char *desc, ...) {
    map_t *array = map_new(dtor);
    va_list argp;
    string_t k;
    int64_t i;
    char c, *s;
    void_t p;

    array->item_type = ZE_NULL;
    va_start(argp, desc);
    while (*desc) {
        k = va_arg(argp, char *);
        switch (*desc++) {
            case 'i':
                // integer argument
                i = va_arg(argp, size_t);
                array_put_long(array, k, i);
                break;
            case 'c':
                // character argument
                c = (char)va_arg(argp, int);
                array_put_long(array, k, c);
                break;
            case 's':
                // string argument
                s = va_arg(argp, char *);
                map_put(array, k, s);
                break;
            case 'p':
                // void pointer (any arbitrary pointer) argument
                p = va_arg(argp, void_t );
                map_put(array, k, p);
                break;
            default:
                break;
        }
    }
    va_end(argp);

    array->as = MAP_HASH;
    return array;
}

map_t *map(map_value_dtor dtor, int n_of_pairs, ...) {
    map_t *array = map_new(dtor);
    va_list argp;
    void_t p;
    string_t k;

    array->item_type = ZE_OBJ;
    va_start(argp, n_of_pairs);
    for (int i = 0; i < (n_of_pairs * 2); i = i + 2) {
        k = va_arg(argp, char *);
        p = va_arg(argp, void_t );
        map_put(array, k, p);
    }
    va_end(argp);

    array->as = MAP_HASH;
    return array;
}

void map_free(map_t *array) {
    array_item_t *next;

    if (!array)
        return;

    if (is_type(array, ZE_MAP_STRUCT)) {
        while (array->head) {
            if (array->dtor)
                array->dtor(array->head->value);

            next = array->head->next;
            ZE_FREE(array->head);
            array->head = next;
        }

        hash_free(array->dict);
        if (array->slice != NULL)
            slice_free(array);

        ZE_FREE(array);
    }
}

int map_push(map_t *array, void_t value) {
    array_item_t *item;
    oa_pair *kv;

    if (!array->started) {
        array->started = true;
        array->indices = 0;
    } else {
        array->indices++;
    }

    item = (array_item_t *)try_calloc(1, sizeof(array_item_t));
    item->indic = array->indices;
    kv = (oa_pair *)hash_put(array->dict, co_itoa(item->indic), value);
    item->type = ZE_MAP_VALUE;
    item->key = kv->key;
    item->value = kv->value;
    item->prev = array->tail;
    item->next = NULL;

    array->tail = item;
    array->length++;

    if (!array->head)
        array->head = item;
    else
        item->prev->next = item;

    return (int)item->indic;
}

map_value_t *map_pop(map_t *array) {
    map_value_t *value;
    array_item_t *item;

    if (!array || !array->tail)
        return NULL;

    item = array->tail;
    array->tail = array->tail->prev;
    array->length--;

    if (array->length == 0)
        array->head = NULL;

    value = item->value;
    ZE_FREE(item);

    return value;
}

void map_shift(map_t *array, void_t value) {
    array_item_t *item;
    oa_pair *kv;

    if (!array)
        return;

    if (!array->started) {
        array->started = true;
        array->indices = 0;
    } else {
        array->indices++;
    }

    item = (array_item_t *)try_calloc(1, sizeof(array_item_t));
    item->type = ZE_MAP_VALUE;
    item->prev = NULL;
    item->next = array->head;
    if (array->head == NULL)
        item->indic = 0;
    else
        item->indic = --item->next->indic;

    array->head = item;
    array->length++;

    if (!array->tail)
        array->tail = item;
    else
        item->next->prev = item;

    kv = (oa_pair *)hash_put(array->dict, co_itoa(item->indic), value);
    item->key = kv->key;
    item->value = kv->value;
}

map_value_t *map_unshift(map_t *array) {
    map_value_t *value;
    array_item_t *item;

    if (!array || !array->head)
        return NULL;

    item = array->head;
    array->head = array->head->next;
    array->length--;

    if (array->length == 0)
        array->tail = NULL;

    value = item->value;
    ZE_FREE(item);

    return value;
}

size_t map_count(map_t *array) {
    if (array)
        return array->length;

    return 0;
}

void_t map_remove(map_t *array, void_t value) {
    array_item_t *item;

    if (!array)
        return NULL;

    for (item = array->head; item != NULL; item = item->next) {
        if (memcmp(item->value, value, sizeof(value)) == 0) {
            hash_delete(array->dict, item->key);
            if (item->prev)
                item->prev->next = item->next;
            else
                array->head = item->next;

            if (item->next)
                item->next->prev = item->prev;
            else
                array->tail = item->prev;

            ZE_FREE(item);
            array->length--;

            return value;
        }
    }

    return NULL;
}

void_t map_del(map_t *array, string_t key) {
    if (!array)
        return NULL;

    return map_remove(array, hash_get(array->dict, key));
}

map_value_t *map_get(map_t *array, string_t key) {
    return (map_value_t *)hash_get(array->dict, key);
}

void array_put_long(map_t *array, string_t key, int64_t value) {
    map_put(array, key, &value);
}

void array_put_str(map_t *array, string_t key, string_t value) {
    map_put(array, key, (char *)value);
}

void map_put(map_t *array, string_t key, void_t value) {
    array_item_t *item;
    oa_pair *kv;
    void_t has = hash_get(array->dict, key);
    if (has == NULL) {
        if (!array->started) {
            array->started = true;
            array->indices = 0;
        } else {
            array->indices++;
        }

        item = (array_item_t *)try_calloc(1, sizeof(array_item_t));
        item->type = ZE_MAP_VALUE;
        item->indic = array->indices;
        kv = (oa_pair *)hash_put(array->dict, key, value);
        item->key = kv->key;
        item->value = kv->value;
        item->prev = array->tail;
        item->next = NULL;

        array->tail = item;
        array->length++;

        if (!array->head)
            array->head = item;
        else
            item->prev->next = item;
    } else {
        for (item = array->head; item; item = item->next) {
            if (memcmp(item->value, has, sizeof(has)) == 0) {
                hash_replace(array->dict, key, value);
                break;
            }
        }
    }
}

map_value_t *map_macro_type(void_t i) {
    if (i)
        return (is_type((i), ZE_MAP_ITER) ? iter_value((i)) : ((map_value_t *)(i)));

    return ((map_value_t *)NULL);
}

map_iter_t *iter_new(map_t *array, bool forward) {
    if (array && array->head) {
        map_iter_t *iterator;

        iterator = (map_iter_t *)try_calloc(1, sizeof(map_iter_t));
        iterator->type = ZE_MAP_ITER;
        iterator->array = array;
        iterator->item = forward ? array->head : array->tail;
        iterator->forward = forward;

        return iterator;
    }

    return NULL;
}

map_iter_t *iter_next(map_iter_t *iterator) {
    if (iterator) {
        array_item_t *item;

        item = iterator->forward ? iterator->item->next : iterator->item->prev;
        if (item) {
            iterator->item = item;
            return iterator;
        } else {
            ZE_FREE(iterator);
            return NULL;
        }
    }

    return NULL;
}

map_value_t *iter_value(map_iter_t *iterator) {
    if (iterator)
        return iterator->item->value;

    return ((map_value_t *)NULL);
}

string_t iter_key(map_iter_t *iterator) {
    if (iterator)
        return iterator->item->key;

    return NULL;
}

map_iter_t *iter_remove(map_iter_t *iterator) {
    array_item_t *item;

    if (!iterator)
        return NULL;

    item = iterator->forward ? iterator->array->head : iterator->array->tail;
    while (item) {
        if (iterator->item == item) {
            if (iterator->array->head == item)
                iterator->array->head = item->next;
            else
                item->prev->next = item->next;

            if (iterator->array->tail == item)
                iterator->array->tail = item->prev;
            else
                item->next->prev = item->prev;

            iterator->array->length--;

            iterator->item = iterator->forward ? item->next : item->prev;
            ZE_FREE(item);
            if (iterator->item) {
                return iterator;
            } else {
                ZE_FREE(iterator);
                return NULL;
            }
        }

        item = iterator->forward ? item->next : item->prev;
    }

    return iterator;
}

void iter_free(map_iter_t *iterator) {
    if (iterator)
        ZE_FREE(iterator);
}
