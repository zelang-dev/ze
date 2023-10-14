#include <ze.h>

void handleClient(uv_stream_t *socket) {
    string data = stream_read(socket);

    printf("Received following request: %s\n\n", data);

    if (strcmp(data, "exit") == 0) {
        // exit command will cause this script to quit out
        puts("exit command received");
        exit(0);
    } else if (strcmp(data, "hi") == 0) {
        // hi command
        // write back to the client a response.
        int status = stream_write(socket, "Hello, This is our command run!");
        printf("hi command received: status %d\n", status);
    } else {
    }
}

int co_main(int argc, char *argv[]) {
    uv_stream_t *socket = stream_bind(UV_TCP, "127.0.0.1", 9010);

    while (true) {
        uv_stream_t *connectedSocket = stream_listen(socket, 1024);
        break;
        stream_handler(handleClient, connectedSocket);
    }

    return 0;
}
