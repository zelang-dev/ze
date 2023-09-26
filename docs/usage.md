# --- Usage

Original **Go** example from <https://www.golinuxcloud.com/goroutines-golang/>

<table>
<tr>
<th>GoLang</th>
<th>C89</th>
</tr>
<tr>
<td>

```go
package main

import (
   "fmt"
   "time"
)

func main() {
   fmt.Println("Start of main Goroutine")
   go greetings("John")
   go greetings("Mary")
   time.Sleep(time.Second * 10)
   fmt.Println("End of main Goroutine")

}

func greetings(name string) {
   for i := 0; i < 3; i++ {
       fmt.Println(i, "==>", name)
       time.Sleep(time.Millisecond)
  }
}
```

</td>
<td>

```c
# include "../include/ze.h"

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

</td>
</tr>
</table>

Original **Go** example from <https://www.programiz.com/golang/channel>

<table>
<tr>
<th>GoLang</th>
<th>C89</th>
</tr>
<tr>
<td>

```go
package main
import "fmt"

func main() {

  // create channel
  ch := make(chan string)

  // function call with goroutine
  go sendData(ch)

  // receive channel data
  fmt.Println(<-ch)

}

func sendData(ch chan string) {

  // data sent to the channel
   ch <- "Received. Send Operation Successful"
   fmt.Println("No receiver! Send Operation Blocked")

}
```

</td>
<td>

```c
#include "../include/ze.h"

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

</td>
</tr>
</table>

Original **Go** example from <https://go.dev/tour/concurrency/5>

<table>
<tr>
<th>GoLang</th>
<th>C89</th>
</tr>
<tr>
<td>

```go
package main

import "fmt"

func fibonacci(c, quit chan int) {
    x, y := 0, 1
    for {
        select {
        case c <- x:
            x, y = y, x+y
        case <-quit:
            fmt.Println("quit")
            return
        }
    }
}

func main() {
    c := make(chan int)
    quit := make(chan int)
    go func() {
        for i := 0; i < 10; i++ {
            fmt.Println(<-c)
        }
        quit <- 0
    }()
    fibonacci(c, quit)
}
```

</td>
<td>

```c
#include "../include/ze.h"

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

</td>
</tr>
</table>

Original **Go** example from <https://www.developer.com/languages/go-error-handling-with-panic-recovery-and-defer/>

<table>
<tr>
<th>GoLang</th>
<th>C89</th>
</tr>
<tr>
<td>

```go
package main

import (
 "fmt"
 "log"
)

func main() {
 divByZero()
 fmt.Println("Although panicked. We recovered. We call mul() func")
 fmt.Println("mul func result: ", mul(5, 10))
}

func div(x, y int) int {
 return x / y
}

func mul(x, y int) int {
 return x * y
}

func divByZero() {
 defer func() {
  if err := recover(); err != nil {
   log.Println("panic occurred:", err)
  }
 }()
 fmt.Println(div(1, 0))
}
```

</td>
<td>

```c
#include "../include/ze.h"

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

</td>
</tr>
</table>

Original **Go** example from <https://gobyexample.com/waitgroups>

<table>
<tr>
<th>GoLang</th>
<th>C89</th>
</tr>
<tr>
<td>

```go
package main

import (
    "fmt"
    "sync"
    "time"
)

func worker(id int) {
    fmt.Printf("Worker %d starting\n", id)

    time.Sleep(time.Second)
    fmt.Printf("Worker %d done\n", id)
}

func main() {

    var wg sync.WaitGroup

    for i := 1; i <= 5; i++ {
        wg.Add(1)

        i := i

        go func() {
            defer wg.Done()
            worker(i)
        }()
    }

    wg.Wait()

}
```

</td>
<td>

```c
#include "../include/ze.h"

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

    printf("\nWorker # %d returned: %d\n",
           cid[2],
           co_group_get_result(wgr, cid[2]).integer);
    printf("\nWorker # %d returned: %s\n",
           cid[1],
           co_group_get_result(wgr, cid[1]).string);
    return 0;
}
```

</td>
</tr>
</table>

The `C++ 20` concurrency **thread** model by way of **future/promise** implemented with same like _semantics_.

Original **C++ 20** example from <https://cplusplus.com/reference/future/future/wait/>

<table>
<tr>
<th>C++ 20</th>
<th>C89</th>
</tr>
<tr>
<td>

<p>

```c++
// future::wait
#include <iostream>       // std::cout
#include <future>         // std::async, std::future
#include <chrono>         // std::chrono::milliseconds

// a non-optimized way of checking for prime numbers:
bool is_prime (int x) {
  for (int i=2; i<x; ++i) if (x%i==0) return false;
  return true;
}

int main ()
{
  // call function asynchronously:
  std::future<bool> fut = std::async (is_prime,194232491);

  std::cout << "checking...\n";
  fut.wait();

  std::cout << "\n194232491 ";
  // guaranteed to be ready (and not block) after wait returns
  if (fut.get())
    std::cout << "is prime.\n";
  else
    std::cout << "is not prime.\n";

  return 0;
}
```

</p>

</td>
<td>

<p>

```c
#include "../include/ze.h"

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

</p>

</td>
</tr>
</table>

### See [examples](https://github.com/zelang-dev/ze/tree/main/examples) folder for more
