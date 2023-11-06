#include "../include/ze.h"

void delete(void_t ptr) {
    match(ptr) {
        and (ZE_MAP_STRUCT)
            map_free(ptr);
        or (ZE_CHANNEL)
            channel_free(ptr);
        or (ZE_OBJ)
            ((object_t *)ptr)->dtor(ptr);
        or (ZE_OA_HASH)
            hash_free(ptr);
        otherwise{
            if (is_valid(ptr))
                ZE_FREE(ptr);
            else
                ZE_LOG("Pointer not valid! Not freed!\nPossible double free attempted...\n");
        }
    }
}

void println(int n_of_args, ...) {
    va_list argp;
    void_t list;
    int type;

    va_start(argp, n_of_args);
    for (int i = 0; i < n_of_args; i++) {
        list = va_arg(argp, void_t);
        if (is_type(((map_t *)list), ZE_MAP_STRUCT)) {
            type = ((map_t *)list)->item_type;
            foreach(item in list) {
                if (type == ZE_LLONG)
#ifdef _WIN32
                    printf("%ld ", (long)has(item).long_long);
#else
                    printf("%lld ", has(item).long_long);
#endif
                else if (type == ZE_STRING)
                    printf("%s ", has(item).str);
                else if (type == ZE_OBJ)
                    printf("%p ", has(item).object);
                else if (type == ZE_BOOL)
                    printf(has(item).boolean ? "true " : "false ");
            }
        } else if (is_reflection(list)) {
            reflect_type_t *kind = (reflect_type_t *)list;
            printf("[ %d, %s, %zu, %zu, %zu ]\n",
                   reflect_type_enum(kind),
                   reflect_type_of(kind),
                   reflect_num_fields(kind),
                   reflect_type_size(kind),
                   reflect_packed_size(kind)
            );
            for (int i = 0; i < reflect_num_fields(kind); i++) {
                printf("  -  %d, %s, %s, %zu, %zu, %d, %d\n",
                       reflect_field_enum(kind, i),
                       reflect_field_type(kind, i),
                       reflect_field_name(kind, i),
                       reflect_field_size(kind, i),
                       reflect_field_offset(kind, i),
                       reflect_field_is_signed(kind, i),
                       reflect_field_array_size(kind, i)
                );
            }
        } else {
            printf("%s ", co_value(list).str);
        }
    }
    va_end(argp);
    puts("");
}

void co_delete(routine_t *co) {
    if (!co) {
        ZE_LOG("attempt to delete an invalid coroutine");
    } else if (!(co->status == ZE_NORMAL
                 || co->status == ZE_DEAD
                 || co->status == ZE_ERRED
                 || co->status == ZE_EVENT_DEAD)
               && !co->exiting
               ) {
        ZE_INFO("attempt to delete a coroutine named: %s,\nthat is not dead or suspended, status is: %d\n", co->name, co->status);
    } else {
#ifdef ZE_USE_VALGRIND
        if (co->vg_stack_id != 0) {
            VALGRIND_STACK_DEREGISTER(co->vg_stack_id);
            co->vg_stack_id = 0;
        }
#endif
        if (co->loop_active) {
            co->status = ZE_EVENT_DEAD;
            co->loop_active = false;
            co->synced = false;
        } else {
            if (!is_empty(co->err_allocated))
                ZE_FREE(co->err_allocated);

            if (co->results && !co->event_active)
                ZE_FREE(co->results);

            ZE_FREE(co);
        }
    }
}

void_t co_user_data(routine_t *co) {
    return !is_empty(co) ? co->user_data : NULL;
}

co_state co_status(routine_t *co) {
    return !is_empty(co) ? co->status : ZE_DEAD;
}

values_t *co_var(var_t *data) {
    if (data)
        return ((values_t *)data->value);

    ZE_LOG("attempt to get value on null");
    return ((values_t *)0);
}

value_t co_value(void_t data) {
    if (data)
        return ((values_t *)data)->value;

    ZE_LOG("attempt to get value on null");
    return ((values_t *)0)->value;
}

value_t co_data(values_t *data) {
    if (data)
        return data->value;

    return ((values_t *)0)->value;
}

ZE_FORCE_INLINE void co_suspend() {
    co_yielding(co_current());
}

void co_yielding(routine_t *co) {
    co_stack_check(0);
    co_switch(co);
}

ZE_FORCE_INLINE void co_resuming(routine_t *co) {
    if (!co_terminated(co))
        co_switch(co);
}

ZE_FORCE_INLINE bool co_terminated(routine_t *co) {
    return co->halt;
}

value_t co_await(callable_t fn, void_t arg) {
    wait_group_t *wg = co_wait_group();
    int cid = co_go(fn, arg);
    wait_result_t *wgr = co_wait(wg);
    //if (is_empty(wgr))
    //    return;

    return co_group_get_result(wgr, cid);
}

value_t co_event(callable_t fn, void_t arg) {
    routine_t *co = co_active();
    co->loop_active = true;
    return co_await(fn, arg);
}

wait_group_t *co_wait_group(void) {
    routine_t *c = co_active();
    wait_group_t *wg = ht_group_init();
    c->wait_active = true;
    c->wait_group = wg;

    return wg;
}

void co_handler(func_t fn, void_t handle, func_t dtor) {
    routine_t *co = co_active();
    wait_group_t *eg = ht_group_init();

    co->event_group = eg;
    co->event_active = true;

    int cid = co_go((callable_t)fn, handle);
    string_t key = co_itoa(cid);
    routine_t *c = (routine_t *)hash_get(eg, key);

    co_deferred(c, dtor, handle);
    int r = snprintf(c->name, sizeof(c->name), "handler #%s", key);
    if (r < 0)
        ZE_LOG("Invalid handler");

    co->event_group = NULL;
    hash_remove(eg, key);
    hash_free(eg);
}

wait_result_t *co_wait(wait_group_t *wg) {
    routine_t *c = co_active();
    wait_result_t *wgr = NULL;
    routine_t *co;
    if (c->wait_active && (memcmp(c->wait_group, wg, sizeof(wg)) == 0)) {
        co_pause();
        wgr = ht_result_init();
        co_deferred(c, FUNC_VOID(hash_free), wgr);
        oa_pair *pair;
        while (wg->size != 0) {
            for (int i = 0; i < wg->capacity; i++) {
                pair = wg->buckets[i];
                if (NULL != pair) {
                    if (!is_empty(pair->value)) {
                        co = (routine_t *)pair->value;
                        if (!co_terminated(co)) {
                            if (!co->loop_active && co->status == ZE_NORMAL)
                                coroutine_schedule(co);

                            coroutine_yield();
                        } else {
                            if (!is_empty(co->results) && !co->loop_erred) {
                                hash_put(wgr, co_itoa(co->cid), co->results);
                                if (!co->event_active)
                                    ZE_FREE(co->results);

                                co->results = NULL;
                            }

                            if (co->loop_erred) {
                                //return NULL;
                                //hash_remove(wg, pair->key);
                                //--c->wait_counter;
                                //break;
                                //continue;
                                hash_free(wg);
                                return wgr;
                            }

                            if (co->loop_active)
                                co_deferred_free(co);

                            hash_delete(wg, pair->key);
                            --c->wait_counter;
                        }
                    }
                }
            }
        }
        c->wait_active = false;
        c->wait_group = NULL;
        --coroutine_count;
    }

    hash_free(wg);
    return wgr;
}

value_t co_group_get_result(wait_result_t *wgr, int cid) {
    //if (is_empty(wgr))
    //    return;

    return ((values_t *)hash_get(wgr, co_itoa(cid)))->value;
}

void co_result_set(routine_t *co, void_t data) {
    if (data && data != ZE_ERROR) {
        if (!is_empty(co->results) && !co->event_active)
            ZE_FREE(co->results);

        if (co->event_active) {
            co->results = data;
        } else {
            co->results = try_calloc(1, sizeof(values_t) + sizeof(data));
            memcpy(co->results, &data, sizeof(data));
        }
    }
}

#if defined(_WIN32) || defined(_WIN64)
struct tm *gmtime_r(const time_t *timer, struct tm *buf) {
    int r = gmtime_s(buf, timer);
    if (r)
        return NULL;

    return buf;
}
#endif

unsigned int co_id() {
    return co_active()->cid;
}

signed int co_err_code() {
    return co_active()->loop_code;
}

char *co_get_name() {
    return co_active()->name;
}

char *co_get_state() {
    return co_active()->state;
}
