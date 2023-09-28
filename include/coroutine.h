#ifndef C_COROUTINE_H
#define C_COROUTINE_H

#if defined(__GNUC__) && (!defined(_WIN32) || !defined(_WIN64))
    #define _GNU_SOURCE
#endif

#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <inttypes.h>
#if !defined(_WIN32)
    #include <sys/time.h>
    #include <sys/resource.h> /* setrlimit() */
#endif

#include "uv_routine.h"
#if defined(_WIN32) || defined(_WIN64)
    #include "compat/pthread.h"
    #include "compat/unistd.h"
    #include <excpt.h>
#else
    #include <pthread.h>
    #include <unistd.h>
#endif

#include <time.h>

#if defined(_MSC_VER)
    #define ZE_MPROTECT 1
    #define S_IRUSR S_IREAD  /* read, user */
    #define S_IWUSR S_IWRITE /* write, user */
    #define S_IXUSR 0 /* execute, user */
    #define S_IRGRP 0 /* read, group */
    #define S_IWGRP 0 /* write, group */
    #define S_IXGRP 0 /* execute, group */
    #define S_IROTH 0 /* read, others */
    #define S_IWOTH 0 /* write, others */
    #define S_IXOTH 0 /* execute, others */
    #define S_IRWXU 0
    #define S_IRWXG 0
    #define S_IRWXO 0
    #if !defined(__cplusplus)
        #define __STDC__ 1
    #endif
#endif

#ifdef ZE_DEBUG
    #define ZE_LOG(s) puts(s)
    #define ZE_INFO(s, ...) printf(s, __VA_ARGS__ )
    #define ZE_HERE() fprintf(stderr, "Here %s:%d\n", __FILE__, __LINE__)
#else
    #define ZE_LOG(s)
    #define ZE_INFO(s, ...)
    #define ZE_HERE()
#endif

/* Stack size when creating a coroutine. */
#ifndef ZE_STACK_SIZE
    #define ZE_STACK_SIZE (10 * 1024)
#endif

#ifndef ZE_MAIN_STACK
    #define ZE_MAIN_STACK (64 * 1024)
#endif

#ifndef ZE_SCRAPE_SIZE
    #define ZE_SCRAPE_SIZE (2 * 64)
#endif

#if defined(UV_H)
    #define ZE_EVENT_LOOP(h, m) uv_run(h, m)
#elif !defined(ZE_EVENT_LOOP)
    #define ZE_EVENT_LOOP(h, m)
#endif

/* Public API qualifier. */
#ifndef C_API
    #define C_API extern
#endif

#ifndef ZE_ASSERT
  #if defined(ZE_DEBUG)
    #include <assert.h>
    #define ZE_ASSERT(c) assert(c)
  #else
    #define ZE_ASSERT(c)
  #endif
#endif

/*[amd64, arm, ppc, x86]:
   by default, co_swap_function is marked as a text (code) section
   if not supported, uncomment the below line to use mprotect instead */
/* #define ZE_MPROTECT */

/*[amd64]:
   Win64 only: provides a substantial speed-up, but will thrash XMM regs
   do not use this unless you are certain your application won't use SSE */
/* #define ZE_NO_SSE */

/*[amd64, aarch64]:
   Win64 only: provides a small speed-up, but will break stack unwinding
   do not use this if your application uses exceptions or setjmp/longjmp */
/* #define ZE_NO_TIB */

#if !defined(thread_local) /* User can override thread_local for obscure compilers */
     /* Running in multi-threaded environment */
    #if defined(__STDC__) /* Compiling as C Language */
      #if defined(_MSC_VER) /* Don't rely on MSVC's C11 support */
        #define thread_local __declspec(thread)
      #elif __STDC_VERSION__ < 201112L /* If we are on C90/99 */
        #if defined(__clang__) || defined(__GNUC__) /* Clang and GCC */
          #define thread_local __thread
        #else /* Otherwise, we ignore the directive (unless user provides their own) */
          #define thread_local
        #endif
      #elif __APPLE__ && __MACH__
        #define thread_local __thread
      #else /* C11 and newer define thread_local in threads.h */
        #include <threads.h>
      #endif
    #elif defined(__cplusplus) /* Compiling as C++ Language */
      #if __cplusplus < 201103L /* thread_local is a C++11 feature */
        #if defined(_MSC_VER)
          #define thread_local __declspec(thread)
        #elif defined(__clang__) || defined(__GNUC__)
          #define thread_local __thread
        #else /* Otherwise, we ignore the directive (unless user provides their own) */
          #define thread_local
        #endif
      #else /* In C++ >= 11, thread_local in a builtin keyword */
        /* Don't do anything */
      #endif
    #endif
#endif

#ifndef ZE_FORCE_INLINE
  #ifdef _MSC_VER
    #define ZE_FORCE_INLINE __forceinline
  #elif defined(__GNUC__)
    #if defined(__STRICT_ANSI__)
      #define ZE_FORCE_INLINE __inline__ __attribute__((always_inline))
    #else
      #define ZE_FORCE_INLINE inline __attribute__((always_inline))
    #endif
  #elif defined(__BORLANDC__) || defined(__DMC__) || defined(__SC__) || defined(__WATCOMC__) || defined(__LCC__) ||  defined(__DECC)
    #define ZE_FORCE_INLINE __inline
  #else /* No inline support. */
    #define ZE_FORCE_INLINE
  #endif
#endif

#ifndef ZE_NO_INLINE
  #ifdef __GNUC__
    #define ZE_NO_INLINE __attribute__((noinline))
  #elif defined(_MSC_VER)
    #define ZE_NO_INLINE __declspec(noinline)
  #else
    #define ZE_NO_INLINE
  #endif
#endif

/* In alignas(a), 'a' should be a power of two that is at least the type's
   alignment and at most the implementation's alignment limit.  This limit is
   2**13 on MSVC. To be portable to MSVC through at least version 10.0,
   'a' should be an integer constant, as MSVC does not support expressions
   such as 1 << 3.

   The following C11 requirements are NOT supported on MSVC:

   - If 'a' is zero, alignas has no effect.
   - alignas can be used multiple times; the strictest one wins.
   - alignas (TYPE) is equivalent to alignas (alignof (TYPE)).
*/
#if !defined(alignas)
  #if defined(__STDC__) /* C Language */
    #if defined(_MSC_VER) /* Don't rely on MSVC's C11 support */
      #define alignas(bytes) __declspec(align(bytes))
    #elif __STDC_VERSION__ >= 201112L /* C11 and above */
      #include <stdalign.h>
    #elif defined(__clang__) || defined(__GNUC__) /* C90/99 on Clang/GCC */
      #define alignas(bytes) __attribute__ ((aligned (bytes)))
    #else /* Otherwise, we ignore the directive (user should provide their own) */
      #define alignas(bytes)
    #endif
  #elif defined(__cplusplus) /* C++ Language */
    #if __cplusplus < 201103L
      #if defined(_MSC_VER)
        #define alignas(bytes) __declspec(align(bytes))
      #elif defined(__clang__) || defined(__GNUC__) /* C++98/03 on Clang/GCC */
        #define alignas(bytes) __attribute__ ((aligned (bytes)))
      #else /* Otherwise, we ignore the directive (unless user provides their own) */
        #define alignas(bytes)
      #endif
    #else /* C++ >= 11 has alignas keyword */
      /* Do nothing */
    #endif
  #endif /* = !defined(__STDC_VERSION__) && !defined(__cplusplus) */
#endif

#if !defined(ZE_MALLOC) || !defined(ZE_FREE) || !defined(ZE_REALLOC)|| !defined(ZE_CALLOC)
  #include <stdlib.h>
  #define ZE_MALLOC malloc
  #define ZE_FREE free
  #define ZE_REALLOC realloc
  #define ZE_CALLOC calloc
#endif

#if defined(__clang__)
  #pragma clang diagnostic ignored "-Wparentheses"

  /* placing code in section(text) does not mark it executable with Clang. */
  #undef  ZE_MPROTECT
  #define ZE_MPROTECT
#endif

#if (defined(__clang__) || defined(__GNUC__)) && defined(__i386__)
  #define fastcall __attribute__((fastcall))
#elif defined(_MSC_VER) && defined(_M_IX86)
  #define fastcall __fastcall
#else
  #define fastcall
#endif

#ifdef ZE_USE_VALGRIND
#include <valgrind/valgrind.h>
#endif

#ifndef __builtin_expect
#define __builtin_expect(x, y) (x)
#endif

#define LIKELY_IS(x, y) __builtin_expect((x), (y))
#define LIKELY(x) LIKELY_IS(!!(x), 1)
#define UNLIKELY(x) LIKELY_IS((x), 0)
#define INCREMENT 16

/* Function casting for single argument, no return. */
#define FUNC_VOID(fn)		((void (*)(void_t))(fn))

#if defined(_MSC_VER)
  #define section(name) __declspec(allocate("." #name))
#elif defined(__APPLE__)
  #define section(name) __attribute__((section("__TEXT,__" #name)))
#else
  #define section(name) __attribute__((section("." #name "#")))
#endif

/* Number used only to assist checking for stack overflows. */
#define ZE_MAGIC_NUMBER 0x7E3CB1A9

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    ZE_NULL = -1,
    ZE_INT,
    ZE_ENUM,
    ZE_INTEGER,
    ZE_UINT,
    ZE_SLONG,
    ZE_ULONG,
    ZE_LLONG,
    ZE_MAXSIZE,
    ZE_FLOAT,
    ZE_DOUBLE,
    ZE_BOOL,
    ZE_SHORT,
    ZE_USHORT,
    ZE_CHAR,
    ZE_UCHAR,
    ZE_UCHAR_P,
    ZE_CHAR_P,
    ZE_CONST_CHAR,
    ZE_STRING,
    ZE_ARRAY,
    ZE_HASH,
    ZE_OBJ,
    ZE_PTR,
    ZE_FUNC,
    ZE_NONE,
    ZE_DEF_ARR,
    ZE_DEF_FUNC,
    ZE_REFLECT_TYPE,
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
    ZE_FUTURE_ARG,
    ZE_EVENT_ARG,
    ZE_SCHED,
    ZE_CHANNEL,
    ZE_STRUCT,
    ZE_UNION,
    ZE_VALUE,
    ZE_NO_INSTANCE
} value_types;

typedef void *void_t;
typedef const void *const_t;
typedef char *string;
typedef const char *string_t;
typedef struct {
    value_types type;
    void_t value;
} var_t;

typedef void_t(*callable_t)(void_t);
typedef void (*func_t)(void_t);
typedef struct {
    value_types type;
    void_t value;
    func_t dtor;
} object_t;

typedef struct {
    value_types type;
    void_t base;
    size_t elements;
} co_array_t;

typedef struct {
    value_types type;
    co_array_t base;
} defer_t;

typedef struct {
    value_types type;
    func_t func;
    void_t data;
    void_t check;
} defer_func_t;

/* Coroutine states. */
typedef enum co_state
{
    ZE_EVENT_DEAD = -1, /* The coroutine has ended it's Event Loop routine, is uninitialized or deleted. */
    ZE_DEAD, /* The coroutine is uninitialized or deleted. */
    ZE_NORMAL,   /* The coroutine is active but not running (that is, it has switch to another coroutine, suspended). */
    ZE_RUNNING,  /* The coroutine is active and running. */
    ZE_SUSPENDED, /* The coroutine is suspended (in a startup, or it has not started running yet). */
    ZE_EVENT /* The coroutine is in an Event Loop callback. */
} co_state;

typedef struct routine_s routine_t;
typedef struct oa_hash_s hash_t;
typedef struct ex_ptr_s ex_ptr_t;
typedef struct ex_context_s ex_context_t;
typedef hash_t wait_group_t;
typedef hash_t wait_result_t;
typedef hash_t ht_map_t;
typedef hash_t gc_channel_t;
typedef hash_t gc_coroutine_t;

#if ((defined(__clang__) || defined(__GNUC__)) && defined(__i386__)) || (defined(_MSC_VER) && defined(_M_IX86))
    #define USE_NATIVE 1
#elif ((defined(__clang__) || defined(__GNUC__)) && defined(__amd64__)) || (defined(_MSC_VER) && defined(_M_AMD64))
    #define USE_NATIVE 1
#elif defined(__clang__) || defined(__GNUC__)
  #if defined(__arm__)
    #define USE_NATIVE 1
  #elif defined(__aarch64__)
    #define USE_NATIVE 1
  #elif defined(__powerpc64__) && defined(_CALL_ELF) && _CALL_ELF == 2
    #define USE_NATIVE 1
  #else
    #define USE_UCONTEXT 1
  #endif
#else
    #define USE_UCONTEXT 1
#endif

#if defined(USE_UCONTEXT)
    #define _BSD_SOURCE
    #if __APPLE__ && __MACH__
        #include <sys/ucontext.h>
    #elif defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
        #include <windows.h>
        #if defined(_X86_)
            #define DUMMYARGS
        #else
            #define DUMMYARGS long dummy0, long dummy1, long dummy2, long dummy3,
        #endif

        typedef struct __stack {
            void_t ss_sp;
            size_t ss_size;
            int ss_flags;
        } stack_t;

        typedef CONTEXT mcontext_t;
        typedef unsigned long __sigset_t;

        typedef struct __ucontext {
            unsigned long int	uc_flags;
            struct __ucontext	*uc_link;
            stack_t				uc_stack;
            mcontext_t			uc_mcontext;
            __sigset_t			uc_sigmask;
        } ucontext_t;

        C_API int getcontext(ucontext_t *ucp);
        C_API int setcontext(const ucontext_t *ucp);
        C_API int makecontext(ucontext_t *, void (*)(), int, ...);
        C_API int swapcontext(routine_t *, const routine_t *);
    #else
        #include <ucontext.h>
    #endif
#endif

/* Coroutine context structure. */
struct routine_s {
#if defined(_WIN32) && defined(_M_IX86)
    void_t rip, *rsp, *rbp, *rbx, *r12, *r13, *r14, *r15;
    void_t xmm[ 20 ]; /* xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15 */
    void_t fiber_storage;
    void_t dealloc_stack;
#elif defined(__x86_64__) || defined(_M_X64)
#ifdef _WIN32
    void_t rip, *rsp, *rbp, *rbx, *r12, *r13, *r14, *r15, *rdi, *rsi;
    void_t xmm[ 20 ]; /* xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15 */
    void_t fiber_storage;
    void_t dealloc_stack;
#else
    void_t rip, *rsp, *rbp, *rbx, *r12, *r13, *r14, *r15;
#endif
#elif defined(__i386) || defined(__i386__)
    void_t eip, *esp, *ebp, *ebx, *esi, *edi;
#elif defined(__ARM_EABI__)
#ifndef __SOFTFP__
    void_t f[ 16 ];
#endif
    void_t d[ 4 ]; /* d8-d15 */
    void_t r[ 4 ]; /* r4-r11 */
    void_t lr;
    void_t sp;
#elif defined(__aarch64__)
    void_t x[ 12 ]; /* x19-x30 */
    void_t sp;
    void_t lr;
    void_t d[ 8 ]; /* d8-d15 */
#elif defined(__powerpc64__) && defined(_CALL_ELF) && _CALL_ELF == 2
    uint64_t gprs[ 32 ];
    uint64_t lr;
    uint64_t ccr;

    /* FPRs */
    uint64_t fprs[ 32 ];

#ifdef __ALTIVEC__
    /* Altivec (VMX) */
    uint64_t vmx[ 12 * 2 ];
    uint32_t vrsave;
#endif
#else
    unsigned long int uc_flags;
    struct ucontext *uc_link;
    stack_t uc_stack;
    mcontext_t uc_mcontext;
    __sigset_t uc_sigmask;
#endif
    /* Stack base address, can be used to scan memory in a garbage collector. */
    void_t stack_base;
#if defined(_WIN32) && (defined(_M_X64) || defined(_M_IX86))
    void_t stack_limit;
#endif
    /* Coroutine stack size. */
    size_t stack_size;
    co_state status;
    callable_t func;
    defer_t defer;
    char name[ 64 ];
    char state[ 64 ];
    char scrape[ ZE_SCRAPE_SIZE ];
    /* unique coroutine id */
    int cid;
    size_t alarm_time;
    routine_t *next;
    routine_t *prev;
    bool channeled;
    bool ready;
    bool system;
    bool exiting;
    bool halt;
    bool synced;
    bool wait_active;
    int wait_counter;
    hash_t *wait_group;
    int all_coroutine_slot;
    routine_t *context;
    bool loop_active;
    void_t user_data;
#if defined(ZE_USE_VALGRIND)
    unsigned int vg_stack_id;
#endif
    void_t args;
    /* Coroutine result of function return/exit. */
    void_t results;
    void_t volatile err;
    const char *volatile panic;
    bool err_recovered;
    bool err_protected;
    ex_ptr_t *err_allocated;
    /* Used to check stack overflow. */
    size_t magic_number;
};

/* scheduler queue struct */
typedef struct co_scheduler_s
{
    value_types type;
    routine_t *head;
    routine_t *tail;
} co_scheduler_t;

/* Generic simple union storage types. */
typedef union
{
    int integer;
    unsigned int u_int;
    signed long s_long;
    unsigned long u_long;
    long long long_long;
    size_t max_size;
    float point;
    double precision;
    bool boolean;
    signed short s_short;
    unsigned short u_short;
    signed char schar;
    unsigned char uchar;
    unsigned char *uchar_ptr;
    char *char_ptr;
    char **array;
    void_t object;
    callable_t func;
    const char str[512];
} value_t;

typedef struct values_s
{
    value_t value;
    value_types type;
} values_t;

typedef enum {
    ZE_OK = ZE_NONE,
    ZE_ERR = ZE_NULL,
} result_type;

typedef struct {
    result_type type;
    value_t *value;
} result_t;

typedef struct uv_args_s
{
    value_types type;
    /* allocated array of arguments */
    values_t *args;
    routine_t *context;

    bool is_path;
    uv_fs_type fs_type;
    uv_req_type req_type;
    uv_handle_type handle_type;
    uv_dirent_type_t dirent_type;
    uv_tty_mode_t tty_mode;
    uv_stdio_flags stdio_flag;
    uv_errno_t errno_code;

    /* total number of args in set */
    size_t n_args;
} uv_args_t;

/*
 * channel communication
 */
struct channel_co_s;
typedef struct channel_co_s channel_co_t;
typedef struct msg_queue_s
{
    channel_co_t **a;
    unsigned int n;
    unsigned int m;
} msg_queue_t;

typedef struct channel_s
{
    value_types type;
    unsigned int bufsize;
    unsigned int elem_size;
    unsigned char *buf;
    unsigned int nbuf;
    unsigned int off;
    unsigned int id;
    values_t *tmp;
    msg_queue_t a_send;
    msg_queue_t a_recv;
    char *name;
    bool select_ready;
} channel_t;

enum
{
    CHANNEL_END,
    CHANNEL_SEND,
    CHANNEL_RECV,
    CHANNEL_OP,
    CHANNEL_BLK,
};

struct channel_co_s
{
    channel_t *c;
    void_t v;
    unsigned int op;
    routine_t *co;
    channel_co_t *x_msg;
};

uv_loop_t *co_loop(void);

/* Return handle to current coroutine. */
C_API routine_t *co_active(void);

/* Delete specified coroutine. */
C_API void co_delete(routine_t *);

/* Switch to specified coroutine. */
C_API void co_switch(routine_t *);

/* Check for coroutine completetion and return. */
C_API bool co_terminated(routine_t *);

/* Return handle to previous coroutine. */
C_API routine_t *co_current(void);

/* Return coroutine executing for scheduler */
C_API routine_t *co_coroutine(void);

/* Return the value in union storage type. */
C_API value_t co_value(void_t);

C_API values_t *co_var(var_t *);

/* Return the value in union storage type. */
C_API value_t co_data(values_t *);

/* Suspends the execution of current coroutine. */
C_API void co_suspend(void);

C_API void co_scheduler(void);

/* Yield to specified coroutine, passing data. */
C_API void co_yielding(routine_t *);

C_API void co_resuming(routine_t *);

/* Returns the status of the coroutine. */
C_API co_state co_status(routine_t *);

/* Get coroutine user data. */
C_API void_t co_user_data(routine_t *);

C_API void co_deferred_free(routine_t *);

/* Defer execution `LIFO` of given function with argument,
to when current coroutine exits/returns. */
C_API void co_defer(func_t, void_t);
C_API void co_deferred(routine_t *, func_t, void_t);
C_API void co_deferred_run(routine_t *, size_t);
C_API size_t co_deferred_count(const routine_t *);

/* Same as `defer` but allows recover from an Error condition throw/panic,
you must call `co_recover` to retrieve error message and mark Error condition handled. */
C_API void co_defer_recover(func_t, void_t);
C_API string_t co_recover(void);

/* Call `ZE_CALLOC` to allocate memory array of given count and size in current coroutine,
will auto free `LIFO` on function exit/return, do not free! */
C_API void_t co_new_by(int, size_t);
C_API void_t co_calloc_full(routine_t *, int, size_t, func_t);

/* Call `ZE_MALLOC` to allocate memory of given size in current coroutine,
will auto free `LIFO` on function exit/return, do not free! */
C_API void_t co_new(size_t);
C_API void_t co_malloc(routine_t *, size_t);
C_API void_t co_malloc_full(routine_t *, size_t, func_t);
C_API char *co_strdup(string_t );
C_API char *co_strndup(string_t , size_t);
C_API char *co_sprintf(string_t , ...);
C_API void_t co_memdup(routine_t *, const_t , size_t);

C_API int co_array_init(co_array_t *);

/* Creates an unbuffered channel, similar to golang channels. */
C_API channel_t *channel(void);

/* Creates an buffered channel of given element count,
similar to golang channels. */
C_API channel_t *channel_buf(int);

/* Send data to the channel. */
C_API int co_send(channel_t *, void_t);

/* Receive data from the channel. */
C_API value_t co_recv(channel_t *);

/* Creates an coroutine of given function with argument,
and add to schedular, same behavior as Go in golang. */
C_API int co_go(callable_t, void_t);

/* Creates an coroutine of given function with argument, and immediately execute. */
C_API void co_execute(func_t, void_t);

C_API uv_loop_t *co_loop(void);

C_API value_t co_event(callable_t, void_t arg);

C_API value_t co_await(callable_t fn, void_t arg);

/* Explicitly give up the CPU for at least ms milliseconds.
Other tasks continue to run during this time. */
C_API unsigned int co_sleep(unsigned int ms);

/* Return the unique id for the current coroutine. */
C_API unsigned int co_id(void);

/* Pause and reschedule current coroutine. */
C_API void co_pause(void);

/* The current coroutine will be scheduled again once all the
other currently-ready coroutines have a chance to run. Returns
the number of other tasks that ran while the current task was waiting. */
C_API int coroutine_yield(void);

/* Returns the current coroutine's name. */
C_API char *coroutine_get_name(void);

/* Exit the current coroutine. If this is the last non-system coroutine,
exit the entire program using the given exit status. */
C_API void coroutine_exit(int);

C_API void coroutine_schedule(routine_t *);
C_API bool coroutine_active(void);
C_API void coroutine_info(void);

C_API void channel_print(channel_t *);
C_API channel_t *channel_create(int, int);
C_API void channel_free(channel_t *);

#if defined(_WIN32) || defined(_WIN64)
C_API int vasprintf(char **, string_t, va_list);
C_API int asprintf(char **, string_t, ...);
C_API int gettimeofday(struct timeval *, struct timezone *);
#endif

#define HASH_LOAD_FACTOR (0.75)
#define HASH_GROWTH_FACTOR (1<<2)
#ifndef HASH_INIT_CAPACITY
    #define HASH_INIT_CAPACITY (1<<9)
#endif

typedef struct oa_key_ops_s {
    uint32_t (*hash)(const_t data, void_t arg);
    void* (*cp)(const_t data, void_t arg);
    void (*free)(void_t data, void_t arg);
    bool (*eq)(const_t data1, const_t data2, void_t arg);
    void_t arg;
} oa_key_ops;

typedef struct oa_val_ops_s {
    void* (*cp)(const_t data, void_t arg);
    void (*free)(void_t data);
    bool (*eq)(const_t data1, const_t data2, void_t arg);
    void_t arg;
} oa_val_ops;

typedef struct oa_pair_s {
    uint32_t hash;
    void_t key;
    void_t value;
} oa_pair;

typedef struct oa_hash_s oa_hash;
struct oa_hash_s {
    int capacity;
    size_t size;
    oa_pair **buckets;
    void (*probing_fct)(struct oa_hash_s *htable, size_t *from_idx);
    oa_key_ops key_ops;
    oa_val_ops val_ops;
};

C_API void hash_free(hash_t *);
C_API void_t hash_put(hash_t *, const_t, const_t);
C_API void_t hash_replace(hash_t *, const_t, const_t);
C_API void_t hash_get(hash_t *, const_t);
C_API void hash_delete(hash_t *, const_t);
C_API void hash_remove(hash_t *, const_t);
C_API void hash_print(hash_t *, void (*print_key)(const_t k), void (*print_val)(const_t v));

/* Creates a new wait group coroutine hash table. */
C_API wait_group_t *ht_group_init(void);

/* Creates a new wait group results hash table. */
C_API wait_result_t *ht_result_init(void);

C_API gc_channel_t *ht_channel_init(void);

C_API ht_map_t *ht_map_init(void);

C_API ht_map_t *ht_map_long_init(void);

C_API ht_map_t *ht_map_string_init(void);

/* Creates/initialize the next series/collection of coroutine's created to be part of wait group, same behavior of Go's waitGroups, but without passing struct or indicating when done.

All coroutines here behaves like regular functions, meaning they return values, and indicate a terminated/finish status.

The initialization ends when `co_wait()` is called, as such current coroutine will pause, and execution will begin for the group of coroutines, and wait for all to finished. */
C_API wait_group_t *co_wait_group(void);

/* Pauses current coroutine, and begin execution for given coroutine wait group object, will wait for all to finish.
Returns hast table of results, accessible by coroutine id. */
C_API wait_result_t *co_wait(wait_group_t *);

/* Returns results of the given completed coroutine id, value in union value_t storage format. */
C_API value_t co_group_get_result(wait_result_t *, int);

C_API void co_result_set(routine_t *, void_t);

typedef struct map_value_s map_value_t;
typedef func_t map_value_dtor;
typedef struct map_iterator_s map_iter_t;
typedef struct array_item_s array_item_t;
typedef union {
    int integer;
    unsigned int u_int;
    signed long s_long;
    unsigned long u_long;
    long long long_long;
    size_t max_size;
    float point;
    double precision;
    bool boolean;
    signed short s_short;
    unsigned short u_short;
    signed char schar;
    unsigned char uchar;
    unsigned char *uchar_ptr;
    char *char_ptr;
    char **array;
    void_t object;
    callable_t func;
    const char str[64];
} map_value;

typedef enum {
    MAP_ARRAY = ZE_ARRAY,
    MAP_HASH
} map_data_type;

struct map_value_s {
    map_value value;
    value_types type;
};

struct array_item_s {
    value_types type;
    map_value_t *value;
    array_item_t *prev;
    array_item_t *next;
    int64_t indic;
    string_t key;
};

typedef struct map_s map_t;
typedef map_t slice_t;
struct map_s
{
    value_types type;
    array_item_t *head;
    array_item_t *tail;
    ht_map_t *dict;
    map_value_dtor dtor;
    int64_t indices;
    int64_t length;
    int no_slices;
    slice_t **slice;
    value_types item_type;
    map_data_type as;
    bool started;
    bool sliced;
};

typedef map_t array_t;
struct map_iterator_s
{
    value_types type;
    map_t *array;
    array_item_t *item;
    bool forward;
};

C_API map_t *map_new(map_value_dtor);
C_API map_t *map_long_init(void);
C_API map_t *map_string_init(void);
C_API map_t *map(map_value_dtor, int, ...);
C_API map_t *map_long(int, ...);
C_API map_t *map_str(int, ...);
C_API map_t *map_for(map_value_dtor dtor, char *desc, ...);
C_API void map_free(map_t *);
C_API int map_push(map_t *, void_t);
C_API map_value_t *map_pop(map_t *);
C_API void map_shift(map_t *, void_t);
C_API map_value_t *map_unshift(map_t *);
C_API size_t map_count(map_t *);
C_API void_t map_remove(map_t *, void_t);
C_API void map_put(map_t *, string_t, void_t);
C_API map_value_t *map_get(map_t *, string_t);
C_API void_t map_del(map_t *, string_t);
C_API array_t *range(int start, int stop);
C_API array_t *array(map_value_dtor, int n_args, ...);
C_API array_t *array_long(int n_args, ...);
C_API array_t *array_str(int n_args, ...);
C_API void array_put_long(map_t *, string_t, int64_t value);
C_API void array_put_str(map_t *, string_t, string_t);
C_API slice_t *slice(array_t *, int64_t start, int64_t end);
C_API string_t slice_find(map_t *array, int64_t index);
C_API map_iter_t *iter_new(map_t *, bool);
C_API map_iter_t *iter_next(map_iter_t *);
C_API map_value_t *iter_value(map_iter_t *);
C_API string_t iter_key(map_iter_t *);
C_API map_iter_t *iter_remove(map_iter_t *);
C_API void iter_free(map_iter_t *);
C_API void println(int n_of_args, ...);
C_API void delete(void_t ptr);

C_API map_value_t *map_macro_type(void_t);

#define EX_CAT(a, b) a ## b

#define EX_STR_(a) #a
#define EX_STR(a) EX_STR_(a)

#define EX_NAME(e) EX_CAT(ex_err_, e)
#define EX_PNAME(p) EX_CAT(ex_protected_, p)

#define EX_MAKE()                                              \
    string_t const err = ((void)err, ex_err.ex);             \
    string_t const err_file = ((void)err_file, ex_err.file); \
    const int err_line = ((void)err_line, ex_err.line)

#ifdef sigsetjmp
    #define ex_jmp_buf         sigjmp_buf
    #define ex_setjmp(buf)    sigsetjmp(buf,1)
    #define ex_longjmp(buf,st) siglongjmp(buf,st)
#else
    #define ex_jmp_buf         jmp_buf
    #define ex_setjmp(buf)    setjmp(buf)
    #define ex_longjmp(buf,st) longjmp(buf,st)
#endif

#define EX_EXCEPTION(E) \
    const char EX_NAME(E)[] = EX_STR(E)

#define rethrow() \
    ex_throw(ex_err.ex, ex_err.file, ex_err.line, ex_err.function, NULL)

#ifdef _WIN32
#define ex_try                                   \
    {                                         \
        /* local context */                   \
        ex_context_t ex_err;                  \
        if (!ex_context)                      \
            ex_init();                        \
        ex_err.next = ex_context;             \
        ex_err.stack = 0;                     \
        ex_err.ex = 0;                        \
        ex_err.unstack = 0;                   \
        /* global context updated */          \
        ex_context = &ex_err;                 \
        /* save jump location */              \
        ex_err.state = ex_setjmp(ex_err.buf); \
    __try                                     \
    {                                         \
        if (ex_err.state == ex_try_st)        \
        {                                     \
            {

#define ex_catch_any                    \
    }                                \
    }                                \
    }                                \
    __except(catch_filter_seh(GetExceptionCode(), GetExceptionInformation())) {\
    if (ex_err.state == ex_throw_st) \
    {                                \
        {                            \
            EX_MAKE();               \
            ex_err.state = ex_catch_st;

#define ex_end_try                            \
    }                                      \
    }                                      \
    if (ex_context == &ex_err)             \
        /* global context updated */       \
        ex_context = ex_err.next;          \
    if ((ex_err.state & ex_throw_st) != 0) \
        rethrow();                         \
    }                                      \
    }

#else
#define ex_try                               \
    {                                         \
        /* local context */                   \
        ex_context_t ex_err;                  \
        if (!ex_context)                      \
            ex_init();                        \
        ex_err.next = ex_context;             \
        ex_err.stack = 0;                     \
        ex_err.ex = 0;                        \
        ex_err.unstack = 0;                   \
        /* global context updated */          \
        ex_context = &ex_err;                 \
        /* save jump location */              \
        ex_err.state = ex_setjmp(ex_err.buf); \
        if (ex_err.state == ex_try_st)        \
        {                                     \
            {

#define ex_catch_any                    \
    }                                \
    }                                \
    if (ex_err.state == ex_throw_st) \
    {                                \
        {                            \
            EX_MAKE();               \
            ex_err.state = ex_catch_st;

#define ex_end_try                            \
    }                                      \
    }                                      \
    if (ex_context == &ex_err)             \
        /* global context updated */       \
        ex_context = ex_err.next;          \
    if ((ex_err.state & ex_throw_st) != 0) \
        rethrow();                         \
    }
#endif

#define ex_finally                          \
    }                                    \
    }                                    \
    {                                    \
        {                                \
            EX_MAKE();                   \
            /* global context updated */ \
            ex_context = ex_err.next;

#define ex_catch(E)                        \
    }                                   \
    }                                   \
    if (ex_err.state == ex_throw_st)    \
    {                                   \
        extern const char EX_NAME(E)[]; \
        if (ex_err.ex == EX_NAME(E))    \
        {                               \
            EX_MAKE();                  \
            ex_err.state = ex_catch_st;

#define ex_throw_loc(E, F, L, C)           \
    do                                  \
    {                                   \
        extern const char EX_NAME(E)[]; \
        ex_throw(EX_NAME(E), F, L, C, NULL);     \
    } while (0)

#define throw(E) \
    ex_throw_loc(E, __FILE__, __LINE__, __FUNCTION__)

/* An macro that stops the ordinary flow of control and begins panicking,
throws an exception of given message. */
#define co_panic(message)                                                      \
    do                                                                         \
    {                                                                          \
        extern const char EX_NAME(panic)[];                                    \
        ex_throw(EX_NAME(panic), __FILE__, __LINE__, __FUNCTION__, (message)); \
    } while (0)

C_API void ex_throw(string_t, string_t, int, string_t, string_t);
C_API int ex_uncaught_exception(void);
C_API void ex_terminate(void);
C_API void ex_init(void);
C_API void (*ex_signal(int sig, string_t ex))(int);
C_API ex_ptr_t ex_protect_ptr(ex_ptr_t *const_ptr, void_t ptr, void (*func)(void_t));
#ifdef _WIN32
C_API int catch_seh(string_t exception, DWORD code, struct _EXCEPTION_POINTERS *ep);
C_API int catch_filter_seh(DWORD code, struct _EXCEPTION_POINTERS *ep);
#endif

/* Convert signals into exceptions */
C_API void ex_signal_setup(void);

enum
{
    ex_try_st,
    ex_throw_st,
    ex_catch_st
};

/* stack of protected pointer */
struct ex_ptr_s
{
    value_types type;
    ex_ptr_t *next;
    func_t func;
    void_t *ptr;
};

/* stack of exception */
struct ex_context_s
{
    value_types type;
    /* The handler in the stack (which is a FILO container). */
    ex_context_t *next;
    ex_ptr_t *stack;
    routine_t *co;

    /** The function from which the exception was thrown */
    const char *volatile function;

    /** The name of this exception */
    const char *volatile ex;

    /* The file from whence this handler was made, in order to backtrace it later (if we want to). */
    const char *volatile file;

    /* The line from whence this handler was made, in order to backtrace it later (if we want to). */
    int volatile line;

    /* Which is our active state? */
    int volatile state;

    int unstack;

    /* The program state in which the handler was created, and the one to which it shall return. */
    ex_jmp_buf buf;
};

C_API thread_local ex_context_t *ex_context;
C_API thread_local char ex_message[256];
C_API thread_local int coroutine_count;

typedef struct _promise
{
    value_types type;
    values_t *result;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool done;
    int id;
} promise;

typedef struct _future
{
    value_types type;
    pthread_t thread;
    pthread_attr_t attr;
    callable_t func;
    int id;
    promise *value;
} future;

typedef struct _future_arg
{
    value_types type;
    callable_t func;
    void_t arg;
    promise *value;
} future_arg;

C_API future *future_create(callable_t);
C_API void future_start(future *, void_t);
C_API void future_stop(future *);
C_API void future_close(future *);

C_API promise *promise_create();
C_API value_t promise_get(promise *);
C_API void promise_set(promise *, void_t);
C_API bool promise_done(promise *);
C_API void promise_close(promise *);

/* Calls fn (with args as arguments) in separated thread, returning without waiting
for the execution of fn to complete. The value returned by fn can be accessed through
 the future object returned (by calling `co_async_get()`). */
C_API future *co_async(callable_t, void_t);

/* Returns the value of a promise, a future thread's shared object, If not ready this
function blocks the calling thread and waits until it is ready. */
C_API value_t co_async_get(future *);

/* Waits for the future thread's state to change. this function pauses current coroutine
and execute others until future is ready, thread execution has ended. */
C_API void co_async_wait(future *);

C_API unsigned long co_async_self(void);

/* Check for at least `n` bytes left on the stack. If not present, panic/abort. */
C_API void co_stack_check(int);

C_API string_t co_itoa(int64_t number);
C_API void co_strcpy(char *dest, string_t src, size_t len);

C_API void gc_coroutine(routine_t *);
C_API void gc_channel(channel_t *);
C_API gc_channel_t *gc_channel_list(void);
C_API gc_coroutine_t *gc_coroutine_list(void);
C_API void gc_coroutine_free(void);
C_API void gc_channel_free(void);

/* Write this function instead of `main`, this library provides its own main, the scheduler,
which call this function as an coroutine! */
int co_main(int, char **);

#ifdef __cplusplus
}
#endif

#endif
