#include "../include/ze.h"

static thread_local gc_channel_t *channel_list = NULL;
static thread_local gc_coroutine_t *coroutine_list = NULL;

void gc_coroutine(routine_t *co) {
    if (!coroutine_list)
        coroutine_list = (gc_coroutine_t *)ht_group_init();
    hash_put(coroutine_list, co_itoa(co->cid), co);
}

void gc_channel(channel_t *ch) {
    if (!channel_list)
        channel_list = ht_channel_init();
    hash_put(channel_list, co_itoa(ch->id), ch);
}

ZE_FORCE_INLINE gc_channel_t *gc_channel_list(void) {
    return channel_list;
}

ZE_FORCE_INLINE gc_coroutine_t *gc_coroutine_list(void) {
    return coroutine_list;
}

void gc_channel_free(void) {
    if (channel_list)
        hash_free(channel_list);
}

void gc_coroutine_free(void) {
    if (coroutine_list)
        hash_free(coroutine_list);
}

void_t try_calloc(int count, size_t size) {
    void_t ptr = ZE_CALLOC(count, size);
    if (ptr == NULL)
        co_panic("Memory allocation failed!");

    return ptr;
}

void_t try_malloc(size_t size) {
    void_t ptr = ZE_MALLOC(size);
    if (ptr == NULL)
        co_panic("Memory allocation failed!");

    return ptr;
}

void_t co_malloc_full(routine_t *coro, size_t size, func_t func) {
    void_t ptr = try_malloc(size);

    if (coro->err_allocated == NULL)
        coro->err_allocated = try_malloc(sizeof(ex_ptr_t));

    ex_protect_ptr(coro->err_allocated, ptr, func);
    coro->err_protected = true;
    co_deferred(coro, func, ptr);

    return ptr;
}

void_t co_calloc_full(routine_t *coro, int count, size_t size, func_t func) {
    void_t ptr = try_calloc(count, size);

    if (coro->err_allocated == NULL)
        coro->err_allocated = try_calloc(1, sizeof(ex_ptr_t));

    ex_protect_ptr(coro->err_allocated, ptr, func);
    coro->err_protected = true;
    co_deferred(coro, func, ptr);

    return ptr;
}

ZE_FORCE_INLINE void_t co_new_by(int count, size_t size) {
    return co_calloc_full(co_active(), count, size, ZE_FREE);
}

ZE_FORCE_INLINE void_t co_new(size_t size) {
    return co_malloc_full(co_active(), size, ZE_FREE);
}

void_t co_malloc(routine_t *coro, size_t size) {
    return co_malloc_full(coro, size, ZE_FREE);
}

void_t co_memdup(routine_t *coro, const_t src, size_t len) {
    void_t ptr = co_new_by(1, len + 1);

    return LIKELY(ptr) ? memcpy(ptr, src, len) : NULL;
}
