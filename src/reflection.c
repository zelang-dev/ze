#include "../include/ze.h"

ZE_FORCE_INLINE value_types type_of(void_t self) {
    return ((var_t *)self)->type;
}

ZE_FORCE_INLINE bool is_type(void_t self, value_types check) {
    return type_of(self) == check;
}

ZE_FORCE_INLINE bool is_instance_of(void_t self, void_t check) {
    return type_of(self) == type_of(check);
}

ZE_FORCE_INLINE bool is_value(void_t self) {
    return (type_of(self) > ZE_NULL) && (type_of(self) < ZE_NONE);
}

ZE_FORCE_INLINE bool is_instance(void_t self) {
    return (type_of(self) > ZE_NONE) && (type_of(self) < ZE_NO_INSTANCE);
}

ZE_FORCE_INLINE bool is_valid(void_t self) {
    return is_value(self) || is_instance(self);
}

ZE_FORCE_INLINE bool is_reflection(void_t self) {
    return ((reflect_kind_t *)self)->fields
        && ((reflect_kind_t *)self)->name
        && ((reflect_kind_t *)self)->num_fields
        && ((reflect_kind_t *)self)->size
        && ((reflect_kind_t *)self)->packed_size;
}

ZE_FORCE_INLINE bool is_status_invalid(routine_t *t) {
    return (t->status > ZE_EVENT || t->status < ZE_DEAD);
}

/*
TODO:
    ZE_NULL = -1,
    ZE_SLONG,
    ZE_ULONG,
    ZE_SHORT,
    ZE_USHORT,
    ZE_CHAR,
    ZE_UCHAR,
    ZE_ARRAY,
    ZE_HASH,
    ZE_NONE,
    ZE_DEF_ARR,
    ZE_DEF_FUNC,
    ZE_REFLECT_INFO,
    ZE_REFLECT_VALUE,
    ZE_MAP_VALUE,
    ZE_MAP_STRUCT,
    ZE_MAP_ITER,
    ZE_MAP_ARR,
    ZE_ERR_PTR,
    ZE_ERR_CONTEXT,
    ZE_PROMISE,
    ZE_FUTURE,
    ZE_FUTUZE_ARG,
    ZE_EVENT_ARG,
    ZE_SCHED,
    ZE_CHANNEL,
    ZE_VALUE,
    ZE_NO_INSTANCE
*/


string_t reflect_kind(void_t value) {
    reflect_types res = (reflect_types)type_of(value);
    if (res == ZE_STRUCT) {
        if (strcmp(reflect_type_of((reflect_type_t *)value), "var_t") == 0) {
            char out[ZE_SCRAPE_SIZE];
            reflect_get_field((reflect_type_t *)value, 0, out);
            res = c_int(out);
        }
    }
    switch (res) {
        case ZE_STRUCT:
            return "struct";
        case ZE_UNION:
            return "union";
        case ZE_CONST_CHAR:
            return "const char *";
        case ZE_STRING:
            // return "string";
        case ZE_CHAR_P:
            return "char *";
        case ZE_UCHAR_P:
            return "unsigned char *";
        case ZE_INTEGER:
        case ZE_INT:
            return "int";
        case ZE_UINT:
            return "unsigned int";
        case ZE_MAXSIZE:
            return "unsigned long long";
        case ZE_LLONG:
            return "long long";
        case ZE_ENUM:
            return "enum";
        case ZE_BOOL:
            return "unsigned char";
        case ZE_FLOAT:
            return "float";
        case ZE_DOUBLE:
            return "double";
        case ZE_OBJ:
            return "* object(struct)";
        case ZE_PTR:
            return "* ptr";
        case ZE_FUNC:
            return "*(*)(*) callable";
        case ZE_REFLECT_TYPE:
            return "<> reflect";
    }

    return "Unknown error";
}

ZE_FORCE_INLINE void reflect_with(reflect_type_t *type, void_t value) {
    type->instance = value;
}

ZE_FORCE_INLINE reflect_field_t *reflect_value_of(reflect_type_t *type) {
    return type->fields;
}

ZE_FORCE_INLINE size_t reflect_num_fields(reflect_type_t *type) {
    return type->fields_count;
}

ZE_FORCE_INLINE string_t reflect_type_of(reflect_type_t *type) {
    return type->name;
}

ZE_FORCE_INLINE reflect_types reflect_type_enum(reflect_type_t *type) {
    return type->data_type;
}

ZE_FORCE_INLINE size_t reflect_type_size(reflect_type_t *type) {
    return type->size;
}

ZE_FORCE_INLINE size_t reflect_packed_size(reflect_type_t *type) {
    return type->packed_size;
}

ZE_FORCE_INLINE string_t reflect_field_type(reflect_type_t *type, int slot) {
    return (type->fields + slot)->field_type;
}

ZE_FORCE_INLINE string_t reflect_field_name(reflect_type_t *type, int slot) {
    return (type->fields + slot)->field_name;
}

ZE_FORCE_INLINE size_t reflect_field_size(reflect_type_t *type, int slot) {
    return (type->fields + slot)->size;
}

ZE_FORCE_INLINE size_t reflect_field_offset(reflect_type_t *type, int slot) {
    return (type->fields + slot)->offset;
}

ZE_FORCE_INLINE bool reflect_field_is_signed(reflect_type_t *type, int slot) {
    return (type->fields + slot)->is_signed;
}

ZE_FORCE_INLINE int reflect_field_array_size(reflect_type_t *type, int slot) {
    return (type->fields + slot)->array_size;
}

ZE_FORCE_INLINE reflect_types reflect_field_enum(reflect_type_t *type, int slot) {
    return (type->fields + slot)->data_type;
}

ZE_FORCE_INLINE void reflect_set_field(reflect_type_t *variable, int slot, void_t value) {
    memcpy((string)variable->instance + (variable->fields + slot)->offset, value, (variable->fields + slot)->size);
}

ZE_FORCE_INLINE void reflect_get_field(reflect_type_t *variable, int slot, void_t out) {
    memcpy(out, (string)variable->instance + (variable->fields + slot)->offset, (variable->fields + slot)->size);
}


ze_func(var_t,
        (PTR, void_t, value)
)
ze_func(co_array_t,
        (PTR, void_t, base), (MAXSIZE, size_t, elements)
)
ze_func(defer_t,
        (STRUCT, co_array_t, base)
)
ze_func(defer_func_t,
        (FUNC, func_t, func), (PTR, void_t, data), (PTR, void_t, check)
)
ze_func(promise,
        (STRUCT, values_t *, result), (STRUCT, pthread_mutex_t, mutex),
        (STRUCT, pthread_cond_t, cond),
        (BOOL, bool, done),
        (INTEGER, int, id)
)
ze_func(future,
        (STRUCT, pthread_t, thread), (STRUCT, pthread_attr_t, attr),
        (FUNC, callable_t, func),
        (INTEGER, int, id),
        (STRUCT, promise *, value)
)
ze_func(future_arg,
        (FUNC, callable_t, func), (PTR, void_t, arg), (STRUCT, promise *, value)
)
ze_func(co_scheduler_t,
        (STRUCT, routine_t *, head), (STRUCT, routine_t *, tail)
)
ze_func(uv_args_t,
        (STRUCT, values_t *, args),
        (STRUCT, routine_t *, context),
        (STRING, string, buffer),
        (STRUCT, uv_buf_t, bufs),
        (STRUCT, uv_stat_t, stat, 1),
        (STRUCT, uv_statfs_t, statfs, 1),
        (BOOL, bool, is_path),
        (BOOL, bool, is_request),
        (ENUM, uv_fs_type, fs_type),
        (ENUM, uv_req_type, req_type),
        (ENUM, uv_handle_type, handle_type),
        (ENUM, uv_dirent_type_t, dirent_type),
        (ENUM, uv_tty_mode_t, tty_mode),
        (ENUM, uv_stdio_flags, stdio_flag),
        (ENUM, uv_errno_t, errno_code),
        (MAXSIZE, size_t, n_args)
)
ze_func(channel_t,
        (UINT, unsigned int, bufsize),
        (UINT, unsigned int, elem_size),
        (UCHAR_P, unsigned char *, buf),
        (UINT, unsigned int, nbuf),
        (UINT, unsigned int, off),
        (UINT, unsigned int, id),
        (STRUCT, values_t *, tmp),
        (STRUCT, msg_queue_t, a_send),
        (STRUCT, msg_queue_t, a_recv),
        (STRING, string, name),
        (BOOL, bool, select_ready)
)
ze_func(array_item_t,
        (STRUCT, map_value_t *, value),
        (STRUCT, array_item_t *, prev),
        (STRUCT, array_item_t *, next),
        (LLONG, int64_t, indic),
        (CONST_CHAR, string_t, key)
)
ze_func(map_t,
        (STRUCT, array_item_t *, head),
        (STRUCT, array_item_t *, tail),
        (STRUCT, ht_map_t *, dict),
        (FUNC, map_value_dtor, dtor),
        (LLONG, int64_t, indices),
        (LLONG, int64_t, length),
        (INTEGER, int, no_slices),
        (STRUCT, slice_t **, slice),
        (ENUM, value_types, item_type),
        (ENUM, map_data_type, as),
        (BOOL, bool, started),
        (BOOL, bool, sliced)
)
ze_func(map_iter_t,
        (STRUCT, map_t *, array),
        (STRUCT, array_item_t *, item),
        (BOOL, bool, forward)
)
ze_func(ex_ptr_t,
        (STRUCT, ex_ptr_t *, next),
        (FUNC, func_t, func),
        (OBJ, void_t *, ptr)
)
ze_func(ex_context_t,
        (STRUCT, ex_context_t *, next),
        (STRUCT, ex_ptr_t *, stack),
        (STRUCT, routine_t *, co),
        (CONST_CHAR, volatile const char *, function),
        (CONST_CHAR, volatile const char *, ex),
        (CONST_CHAR, volatile const char *, file),
        (INTEGER, int volatile, line),
        (INTEGER, int volatile, state),
        (INTEGER, int, unstack)
)
ze_func(object_t,
        (PTR, void_t, value),
        (FUNC, func_t, dtor)
)
ze_func(result_t,
        (UNION, value_t, value)
)
