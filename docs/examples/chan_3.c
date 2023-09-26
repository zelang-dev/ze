#include <ze.h>
void *co_2(void *arg) {
    channel_t *c = (channel_t *)arg;

    printf("co_2 start\n");
    for (int i = 0; i < 10; i++) {
        printf("received: %lu\n", co_recv(c).u_long);
    }
    printf("co_2 end\n");

    return 0;
}

int co_main(int argc, char **argv) {
    channel_t *c = channel_buf(3);
    int s;

    co_go(co_2, (void *)c);
    for (unsigned long i = 0; i < 10; i++) {
        printf("going to send number: %lu\n", i);
        s = co_send(c, &i);
        printf("status: %d send success: %lu\n", s, i);
    }
    printf("co_main end\n");

    return 0;
}
