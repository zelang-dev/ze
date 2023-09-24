
#include "../include/ze.h"

void *greetings(void *arg) {
    const char *name = c_const_char(arg);
    for (int i = 0; i < 3; i++) {
        printf("%d ==> %s\n", i, name);
        co_sleep(1);
    }
    return 0;
}

int co_main(int argc, char **argv) {
    puts("Start of main Goroutine");
    co_go(greetings, "John");
    co_go(greetings, "Mary");
    co_sleep(1000);
    puts("End of main Goroutine");
    return 0;
}
