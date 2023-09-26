# ze is c

* [Background](#background)
* [Synopsis](#synopsis)

## Background

This library was initially a rework/refactor and merge of [libco](https://github.com/higan-emu/libco) with [minicoro](https://github.com/edubart/minicoro). These two differ among many [coru](https://github.com/geky/coru), [libdill](https://github.com/sustrik/libdill), [libmill](https://github.com/sustrik/libmill), [libwire](https://github.com/baruch/libwire), [libcoro](https://github.com/semistrict/libcoro), [libcsp](https://github.com/shiyanhui/libcsp), [dyco-coroutine](https://github.com/piaodazhu/dyco-coroutine), in that Windows is supported, and not using **ucontext**. That was until I came across [libtask](https://swtch.com/libtask), where the design is the underpinning of GoLang, and made it Windows compatible in an fork [symplely/libtask](https://github.com/symplely/libtask). **Libtask** has it's channel design origins from [Richard Beton's libcsp](https://libcsp.sourceforge.net/)

You can read [Fibers, Oh My!](https://graphitemaster.github.io/fibers/) for a breakdown on how the actual context switch here is achieved by assembly. This library incorporates [libuv](http://docs.libuv.org) in a way that make providing callbacks unnecessary, same as in [Using C++ Resumable Functions with Libuv](https://devblogs.microsoft.com/cppblog/using-ibuv-with-c-resumable-functions/). **Libuv** is handling any hardware or multi-threading CPU access. This not necessary for library usage, the setup can be replaced with some other Event Loop library, or just disabled. There is a unmaintained [libasync](https://github.com/btrask/libasync) package tried combining **libco**, with **libuv** too, Linux only.

## Synopsis

```c
/* Write this function instead of main, this library provides its own main, the scheduler,
which call this function as an coroutine! */
int co_main(int, char **);

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

/* Creates/initialize the next series/collection of coroutine's created to be part of wait group,
same behavior of Go's waitGroups, but without passing struct or indicating when done.

All coroutines here behaves like regular functions, meaning they return values, and indicate
a terminated/finish status.

The initialization ends when `co_wait()` is called, as such current coroutine will pause, and
execution will begin for the group of coroutines, and wait for all to finished. */
C_API wait_group_t *co_wait_group(void);

/* Pauses current coroutine, and begin execution for given coroutine wait group object, will
wait for all to finished. Returns hast table of results, accessible by coroutine id. */
C_API wait_result_t co_wait(wait_group_t *);

/* Returns results of the given completed coroutine id, value in union value_t storage format. */
C_API value_t co_group_get_result(wait_result_t *, int);

/* Creates an unbuffered channel, similar to golang channels. */
C_API channel_t *channel(void);

/* Creates an buffered channel of given element count,
similar to golang channels. */
C_API channel_t *channel_buf(int);

/* Send data to the channel. */
C_API int co_send(channel_t *, void_t);

/* Receive data from the channel. */
C_API value_t *co_recv(channel_t *);

/* The `for_select {` macro sets up a coroutine to wait on multiple channel operations.
Must be closed out with `} select_end;`, and if no `select_case(channel)`, `select_case_if(channel)`,
`select_break` provided, an infinite loop is created.

This behaves same as GoLang `select {}` statement.
*/
for_select {
    select_case(channel) {
        co_send(channel, void_t data);
        // Or
        value_t *r = co_recv(channel);
    // Or
    } select_case_if(channel) {
        // co_send(channel); || co_recv(channel);

    /* The `select_default` is run if no other case is ready.
    Must also closed out with `select_break;`. */
    } select_default {
        // ...
    } select_break;
} select_end;

/* Creates an coroutine of given function with argument,
and add to schedular, same behavior as Go in golang. */
C_API int co_go(callable_t, void_t);

/* Creates an coroutine of given function with argument, and immediately execute. */
C_API void co_execute(co_call_t, void_t);

/* Explicitly give up the CPU for at least ms milliseconds.
Other tasks continue to run during this time. */
C_API unsigned int co_sleep(unsigned int ms);

/* Call `CO_MALLOC` to allocate memory of given size in current coroutine,
will auto free `LIFO` on function exit/return, do not free! */
C_API void_t co_new(size_t);

/* Call `CO_CALLOC` to allocate memory array of given count and size in current coroutine,
will auto free `LIFO` on function exit/return, do not free! */
C_API void_t co_new_by(int count, size_t size);

/* Defer execution `LIFO` of given function with argument,
to when current coroutine exits/returns. */
C_API void co_defer(func_t, void_t);

/* An macro that stops the ordinary flow of control and begins panicking,
throws an exception of given message. */
co_panic(message);

/* Same as `defer` but allows recover from an Error condition throw/panic,
you must call `co_recover` to retrieve error message and mark Error condition handled. */
C_API void co_defer_recover(recover_func, void_t);

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

typedef struct co_value
{
    value_t value;
    value_types type;
} co_value_t;

/* Return an value in union type storage. */
C_API value_t co_value(void_t);
```

{% include list.liquid all=true %}
