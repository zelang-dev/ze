#include <ze.h>

int co_main(int argc, char *argv[]) {
    char hostname[UV_MAXHOSTNAMESIZE];
    size_t len = sizeof(hostname);
    string_t command, port, http;
    string host_part, headers;
    struct hostent *host;

    if (argc >= 1) {
        if (strcmp(argv[1], "--host") == 0 && co_strpos(argv[2], ".") >= 0) {
            memcpy(hostname, argv[2], len);
            command = argc >= 3 ? argv[3] : NULL;
        } else {
            int r = uv_os_gethostname(hostname, &len);
            port = "5000"; //Set the TCP PORT to connect too
            command = argc >= 1 ? argv[1] : NULL;
        }
    } else {
        command = "hi";
    }

    url_parse_t *url = parse_url((co_strpos(hostname, "://") < 0)
                                 ? (string_t)co_concat_by(2, "tcp://", hostname)
                                 : hostname);
    if (url->host)
        host_part = url->host;

    if (argc >= 1) {
        headers = co_concat_by(3, "GET ", (command == NULL ? "/" : command), " HTTP/1.1\r\n");
        headers = co_concat_by(7,
                               headers,
                               "Host: ",
                               host_part,
                               "\r\n",
                               "Accept: */*\r\n",
                               "Content-type: text/html; charset=utf-8\r\n",
                               "Connection: close\r\n\r\n"
        );

        http = headers;
    } else {
        http = command;
    }

    puts(http);
    return 0;

    // Connect to Server
    uv_stream_t *socket = stream_connect_ex(url->url_type, url->host, (url->port == 0 ? 5000 : url->port));

    // Send a command
    stream_write(socket, http);

    // Receive response from server. Loop until the response is finished
    string response = stream_read(socket);

    // echo our command response
    puts(response);
}
