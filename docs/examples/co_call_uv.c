#include <ze.h>

char buf[] = "blablabla\n";
const char *path = "write.tmp";

int co_main(int argc, char *argv[]) {
    uv_file fd = -1;

    printf("\nFile closed, status: %d\n", fs_close(fd));
    fd = fs_open(__FILE__, O_RDONLY, 0);
    printf("\nFile opened, fd: %d\n", fd);
    printf("\nFile read: %s\n", fs_read(fd, 663 + 56));
    printf("\nFile really closed, status: %d\n", fs_close(fd));

    fd = fs_open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    printf("\nFile opened for write, fd: %d\n", fd);
    printf("\nFile write, written: %d\n", fs_write(fd, buf, 0));
    printf("\nFile writing closed, status: %d\n", fs_close(fd));

    printf("\nFile deleted, status: %d\n", fs_unlink(path));
    return 0;
}

/******hello  world******/
