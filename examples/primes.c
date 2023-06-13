#include "../coroutine.h"

int quiet = 0;
int goal = 0;
int buffer = 0;

void *prime_co(void *arg)
{
    channel_t *c, *nc;
    int p, i;
    c = arg;

    p = co_recv(c)->value.integer;
    if (p > goal)
        exit(0);
    if (!quiet)
    {
        printf(coroutine_get_name());
        printf("%d\n", p);
    }
    nc = co_make_buf(buffer);
    co_go(prime_co, nc);
    for (;;)
    {
        i = co_recv(c)->value.integer;
        if (i % p)
            co_send(nc, &i);
    }

    return 0;
}

int co_main(int argc, char **argv)
{
    int i;
    channel_t *c;

    if (argc > 1)
        goal = atoi(argv[1]);
    else
        goal = 100;
    printf("goal=%d\n", goal);

    c = co_make_buf(buffer);
    co_go(prime_co, c);
    for (i = 2;; i++)
        co_send(c, &i);

    return 0;
}
