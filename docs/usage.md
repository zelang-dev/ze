# Usage by Debug

```c
#include "ze.h"

void_t greetings(void_t arg)
{
    const char *name = c_const_char(arg);
    for (int i = 0; i < 3; i++)
    {
        printf("%d ==> %s\n", i, name);
        co_sleep(1);
    }
    return 0;
}

int co_main(int argc, char **argv)
{
    puts("Start of main Goroutine");
    co_go(greetings, "John");
    co_go(greetings, "Mary");
    co_sleep(1000);
    puts("End of main Goroutine");
    return 0;
}
```

<details>
<summary>DEBUG run output</summary>

<pre>
Thread #7f11090f11c0 running coroutine id: 1 () status: 3
Start of main Goroutine
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 2 () status: 3
0 ==> John
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 3 () status: 3
0 ==> Mary
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 () status: 3
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 (coroutine_wait) status: 2
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 2 () status: 1
1 ==> John
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 3 () status: 1
1 ==> Mary
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 (coroutine_wait) status: 2
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 2 () status: 1
2 ==> John
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 3 () status: 1
2 ==> Mary
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 (coroutine_wait) status: 2
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 2 () status: 1
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 3 () status: 1
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 (coroutine_wait) status: 1
Back at coroutine scheduling
...
...
...
...
Thread #7f6ac8aa11c0 running coroutine id: 4 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f6ac8aa11c0 running coroutine id: 4 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f6ac8aa11c0 running coroutine id: 1 (co_main) status: 1
End of main Goroutine
Back at coroutine scheduling
Coroutine scheduler exited
</pre>
</details>

```c
#include "ze.h"

void_t sendData(void_t arg)
{
    channel_t *ch = (channel_t *)arg;

    // data sent to the channel
    co_send(ch, "Received. Send Operation Successful");
    puts("No receiver! Send Operation Blocked");

    return 0;
}

int co_main(int argc, char **argv)
{
    // create channel
    channel_t *ch = channel();

    // function call with goroutine
    co_go(sendData, ch);
    // receive channel data
    printf("%s\n", co_recv(ch)->value.str);

    return 0;
}
```

<details>
<summary>DEBUG run output</summary>

<pre>
Thread #7f87171711c0 running coroutine id: 1 () status: 3
processed
 r:0x7fffb878dca0
Back at coroutine scheduling
Thread #7f87171711c0 running coroutine id: 2 () status: 3
processed
 s:0x7fffb878dca0*
 => s:0x7fffb878dca0
No receiver! Send Operation Blocked
Back at coroutine scheduling
Thread #7f87171711c0 running coroutine id: 1 (co_main) status: 1
Received. Send Operation Successful
Back at coroutine scheduling
Coroutine scheduler exited
</pre>
</details>

```c
#include "ze.h"

int fibonacci(channel_t *c, channel_t *quit)
{
    int x = 0;
    int y = 1;
    for_select {
        select_case(c) {
            co_send(c, &x);
            unsigned long tmp = x + y;
            x = y;
            y = tmp;
        } select_case_if(quit) {
            co_recv(quit);
            puts("quit");
            return 0;
        } select_break;
    } select_end;
}

void_t func(void_t args)
{
    channel_t *c = ((channel_t **)args)[0];
    channel_t *quit = ((channel_t **)args)[1];

    for (int i = 0; i < 10; i++)
    {
        printf("%d\n", co_recv(c).integer);
    }
    co_send(quit, 0);

    return 0;
}

int co_main(int argc, char **argv)
{
    channel_t *args[2];
    channel_t *c = channel();
    channel_t *quit = channel();

    args[0] = c;
    args[1] = quit;
    co_go(func, args);
    return fibonacci(c, quit);
}
```

<details>
<summary>DEBUG run output</summary>

<pre>
Thread #7f2dadbb11c0 running coroutine id: 1 () status: 3
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 3
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
0
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
1
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
1
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
2
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
3
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
5
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
8
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
13
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
21
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
34
processed
 s:0x7fffc1f35f10
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 r:0x7fffc1f35f10*
 => r:0x7fffc1f35f10
quit
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
Back at coroutine scheduling
Coroutine scheduler exited

</pre>
</details>

```c
#include "ze.h"

int div_err(int x, int y)
{
    return x / y;
}

int mul(int x, int y)
{
    return x * y;
}

void func(void_t arg)
{
    const char *err = co_recover();
    if (NULL != err)
        printf("panic occurred: %s\n", err);
}

void divByZero(void_t arg)
{
    co_defer_recover(func, arg);
    printf("%d", div_err(1, 0));
}

int co_main(int argc, char **argv)
{
    co_execute(divByZero, NULL);
    printf("Although panicked. We recovered. We call mul() func\n");
    printf("mul func result: %d\n", mul(5, 10));
    return 0;
}
```

<details>
<summary>DEBUG run output</summary>

<pre>
Thread #7fd29c4011c0 running coroutine id: 1 () status: 3
Back at coroutine scheduling
Thread #7fd29c4011c0 running coroutine id: 2 () status: 3
panic occurred: sig_ill
Back at coroutine scheduling
Thread #7fd29c4011c0 running coroutine id: 1 (co_main) status: 1
Although panicked. We recovered. We call mul() func
mul func result: 50
Back at coroutine scheduling
Coroutine scheduler exited
</pre>
</details>

```c
#include "ze.h"

void_t worker(void_t arg)
{
    // int id = c_int(arg);
    int id = co_id();
    printf("Worker %d starting\n", id);

    co_sleep(1000);
    printf("Worker %d done\n", id);
    if (id == 4)
        return (void_t)32;
    else if (id == 3)
        return (void_t)"hello world\0";

    return 0;
}

int co_main(int argc, char **argv)
{
    int cid[5];
    wait_group_t *wg = co_wait_group();
    for (int i = 1; i <= 5; i++)
    {
       cid[i-1] = co_go(worker, &i);
    }
    wait_result_t *wgr = co_wait(wg);

    printf("\nWorker # %d returned: %d\n", cid[2], co_group_get_result(wgr, cid[2]).integer);
    printf("\nWorker # %d returned: %s\n", cid[1], co_group_get_result(wgr, cid[1]).string);
    return 0;
}
```

<details>
<summary>DEBUG run output</summary>

<pre>
Thread #7f48f6c211c0 running coroutine id: 1 () status: 3
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 2 () status: 3
Worker 2 starting
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 3 () status: 3
Worker 3 starting
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 4 () status: 3
Worker 4 starting
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 5 () status: 3
Worker 5 starting
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 6 () status: 3
Worker 6 starting
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 7 () status: 3
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 6 () status: 1
Worker 6 done
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 7 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 5 () status: 1
Worker 5 done
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 7 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 4 () status: 1
Worker 4 done
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 7 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 3 () status: 1
Worker 3 done
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 7 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 2 () status: 1
Worker 2 done
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 1 (co_main) status: 1

Worker # 4 returned: 32

Worker # 3 returned: hello world
Back at coroutine scheduling
Coroutine scheduler exited
</pre>
</details>

```c
#include "ze.h"

// a non-optimized way of checking for prime numbers:
void_t is_prime(void_t arg)
{
    int x = c_int(arg);
    for (int i = 2; i < x; ++i)
        if (x % i == 0) return (void_t)false;
    return (void_t)true;
}

int co_main(int argc, char **argv)
{
    int prime = 194232491;
    // call function asynchronously:
    future *f = co_async(is_prime, &prime);

    printf("checking...\n");
    // Pause and run other coroutines
    // until thread state changes.
    co_async_wait(f);

    printf("\n194232491 ");
    // guaranteed to be ready (and not block)
    // after wait returns
    if (co_async_get(f).boolean)
        printf("is prime!\n");
    else
        printf("is not prime.\n");

    return 0;
}
```

<details>
<summary>DEBUG run output</summary>

<pre>
Thread #7fc61b3d11c0 running coroutine id: 1 () status: 3
promise id(706099430) created in thread #7fc61b3d11c0
thread #7fc61b3d11c0 created thread #7fc61b2b0700 with status(0) future id(706099430)
checking...
Back at coroutine scheduling
Thread #7fc61b3d11c0 running coroutine id: 1 (co_main) status: 2
Back at coroutine scheduling
Thread #7fc61b3d11c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Thread #7fc61b3d11c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
...
...
...
...
Thread #7fc61b3d11c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Thread #7fc61b3d11c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Thread #7fc61b3d11c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
promise id(706099430) set LOCK in thread #7fc61b2b0700
promise id(706099430) set UNLOCK in thread #7fc61b2b0700
Thread #7fc61b3d11c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Thread #7fc61b3d11c0 running coroutine id: 1 (co_main) status: 1

194232491 promise id(706099430) get LOCK in thread #7fc61b3d11c0
promise id(706099430) get UNLOCK in thread #7fc61b3d11c0
is prime!
Back at coroutine scheduling
Coroutine scheduler exited
</pre>
</details>
