#include "../include/ze.h"

static void_t fs_init(void_t);
static void_t uv_init(void_t);

thread_local uv_args_t *uv_server_args = NULL;

void_t uv_error_post(routine_t *co, int r) {
    co->loop_erred = true;
    co->loop_code = r;
    co->status = ZE_ERRED;
    fprintf(stderr, "failed: %s\n", uv_strerror(r));
    return NULL;
}

static void _close_cb(uv_handle_t *handle) {
    if (UV_UNKNOWN_HANDLE == ((uv_handle_t *)handle)->type)
        return;

    memset(handle, 0, sizeof(uv_handle_t));
    ZE_FREE(handle);
}

void uv_close_free(void_t handle) {
    if (!uv_is_closing((uv_handle_t *)handle))
        uv_close((uv_handle_t *)handle, _close_cb);
}

static value_t fs_start(uv_args_t *uv_args, values_t *args, uv_fs_type fs_type, size_t n_args, bool is_path) {
    uv_args->args = args;
    uv_args->type = ZE_EVENT_ARG;
    uv_args->fs_type = fs_type;
    uv_args->n_args = n_args;
    uv_args->is_path = is_path;

    return co_event(fs_init, uv_args);
}

static value_t uv_start(uv_args_t *uv_args, values_t *args, int type, size_t n_args, bool is_request) {
    uv_args->args = args;
    uv_args->type = ZE_EVENT_ARG;
    uv_args->is_request = is_request;
    if (is_request)
        uv_args->req_type = type;
    else
        uv_args->handle_type = type;

    uv_args->n_args = n_args;

    return co_event(uv_init, uv_args);
}

static void close_cb(uv_handle_t *handle) {
    uv_args_t *uv = (uv_args_t *)uv_handle_get_data(handle);
    routine_t *co = uv->context;

    co->halt = true;
    co_resuming(co->context);
    co_scheduler();
}

static void error_catch(void_t uv) {
    routine_t *co = ((uv_args_t *)uv)->context;

    if (co_recover() != NULL) {
        if (uv_loop_alive(co_loop()) && uv_server_args) {
            co->halt = true;
            co->loop_erred = true;
            co->status = ZE_ERRED;
            scheduler_info_log = false;
            uv_server_args = NULL;
            if (co->context->wait_group)
                hash_free(co->context->wait_group);

            if (co->context->event_group)
                hash_free(co->context->event_group);


            if (uv_loop_alive(co_loop())) {
                uv_walk(co_loop(), (uv_walk_cb)uv_close_free, NULL);
                uv_run(co_loop(), UV_RUN_DEFAULT);

                uv_stop(co_loop());
                uv_run(co_loop(), UV_RUN_DEFAULT);
            }
        }
    }
}

static void connect_cb(uv_connect_t *client, int status) {
    uv_args_t *uv = (uv_args_t *)uv_req_get_data((uv_req_t *)client);
    values_t *args = uv->args;
    routine_t *co = uv->context;

    if (status) {
        fprintf(stderr, "Error: %s\n", uv_strerror(status));
    }

    co->halt = true;
    co_result_set(co, (status < 0 ? &status : args[0].value.object));
    co_resuming(co->context);
    co_scheduler();
}

static void connection_cb(uv_stream_t *server, int status) {
    uv_args_t *uv = (uv_args_t *)uv_handle_get_data((uv_handle_t *)server);
    routine_t *co = uv->context;
    uv_loop_t *uvLoop = co_loop();
    void_t handle = NULL;
    int r = status;

    co->halt = true;
    scheduler_info_log = false;
    if (status == 0) {
        if (uv->bind_type == UV_TCP) {
            handle = ZE_CALLOC(1, sizeof(uv_tcp_t));
            r = uv_tcp_init(uvLoop, (uv_tcp_t *)handle);
        } else if (uv->bind_type == UV_NAMED_PIPE) {
            handle = ZE_CALLOC(1, sizeof(uv_pipe_t));
            r = uv_pipe_init(uvLoop, (uv_pipe_t *)handle, 0);
        }

        if (!r)
            r = uv_accept(stream(server), stream(handle));

        if (!r)
            co_result_set(co, stream(handle));
    }

    if (r) {
        fprintf(stderr, "Error: %s\n", uv_strerror(r));
        if (handle != NULL)
            ZE_FREE(handle);

        co_result_set(co, &r);
    }

    co_resuming(co->context);
    co_scheduler();
}

static void write_cb(uv_write_t *req, int status) {
    uv_args_t *uv = (uv_args_t *)uv_req_get_data((uv_req_t *)req);
    routine_t *co = uv->context;
    if (status < 0) {
        fprintf(stderr, "Error: %s\n", uv_strerror(status));
    }

    co->halt = true;
    co_result_set(co, (status < 0 ? &status : 0));
    co_resuming(co->context);
    co_scheduler();
}

static void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    uv_args_t *uv = (uv_args_t *)uv_handle_get_data(handle);
    routine_t *co = uv->context;

    buf->base = (string)co_calloc_full(co, 1, suggested_size + 1, ZE_FREE);
    buf->len = suggested_size;
}

static void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    uv_args_t *uv = (uv_args_t *)uv_handle_get_data((uv_handle_t *)stream);
    routine_t *co = uv->context;

    co->halt = true;
    if (nread < 0) {
        if (nread != UV_EOF)
            fprintf(stderr, "Error: %s\n", uv_strerror(nread));

        co_result_set(co, (nread == UV_EOF ? 0 : &nread));
    } else if (nread == UV_EAGAIN) {
        co_result_set(co, (void *)UV_EAGAIN);
    } else {
        co_result_set(co, (nread > 0 ? buf->base : NULL));
    }

    uv_read_stop(stream);
    co_resuming(co->context);
    co_scheduler();
}

static void fs_cb(uv_fs_t *req) {
    ssize_t result = uv_fs_get_result(req);
    uv_args_t *fs = (uv_args_t *)uv_req_get_data((uv_req_t *)req);
    routine_t *co = fs->context;
    bool override = false;

    if (result < 0) {
        fprintf(stderr, "Error: %s\n", uv_strerror((int)result));
    } else {
        void_t fs_ptr = uv_fs_get_ptr(req);
        uv_fs_type fs_type = uv_fs_get_type(req);

        switch (fs_type) {
            case UV_FS_CLOSE:
            case UV_FS_SYMLINK:
            case UV_FS_LINK:
            case UV_FS_CHMOD:
            case UV_FS_RENAME:
            case UV_FS_UNLINK:
            case UV_FS_RMDIR:
            case UV_FS_MKDIR:
            case UV_FS_CHOWN:
            case UV_FS_UTIME:
            case UV_FS_FUTIME:
            case UV_FS_FCHMOD:
            case UV_FS_FCHOWN:
            case UV_FS_FTRUNCATE:
            case UV_FS_FDATASYNC:
            case UV_FS_FSYNC:
            case UV_FS_OPEN:
            case UV_FS_WRITE:
            case UV_FS_SENDFILE:
                break;
            case UV_FS_SCANDIR:
                break;
            case UV_FS_STATFS:
            case UV_FS_LSTAT:
            case UV_FS_STAT:
            case UV_FS_FSTAT:
                override = true;
                memcpy(fs->stat, &req->statbuf, sizeof(fs->stat));
                co_result_set(co, fs->stat);
                break;
            case UV_FS_READLINK:
                break;
            case UV_FS_READ:
                override = true;
                co_result_set(co, fs->buffer);
                break;
            case UV_FS_UNKNOWN:
            case UV_FS_CUSTOM:
            default:
                fprintf(stderr, "type; %d not supported.\n", fs_type);
                break;
        }
    }

    co->halt = true;
    if (!override)
        co_result_set(co, (values_t *)uv_fs_get_result(req));

    co_resuming(co->context);
    co_scheduler();
}

static void_t fs_init(void_t uv_args) {
    uv_fs_t *req = co_new_by(1, sizeof(uv_fs_t));
    uv_loop_t *uvLoop = co_loop();
    uv_args_t *fs = (uv_args_t *)uv_args;
    values_t *args = fs->args;
    uv_uid_t uid, gid;
    uv_file in_fd;
    size_t length;
    int64_t offset;
    string_t n_path;
    double atime, mtime;
    int flags, mode;
    int result = -4058;

    co_defer(FUNC_VOID(uv_fs_req_cleanup), req);
    if (fs->is_path) {
        string_t path = var_char_ptr(args[0]);
        switch (fs->fs_type) {
            case UV_FS_OPEN:
                flags = var_int(args[1]);
                mode = var_int(args[2]);
                result = uv_fs_open(uvLoop, req, path, flags, mode, fs_cb);
                break;
            case UV_FS_UNLINK:
                result = uv_fs_unlink(uvLoop, req, path, fs_cb);
                break;
            case UV_FS_MKDIR:
                mode = var_int(args[1]);
                result = uv_fs_mkdir(uvLoop, req, path, mode, fs_cb);
                break;
            case UV_FS_RENAME:
                n_path = var_char_ptr(args[1]);
                result = uv_fs_rename(uvLoop, req, path, n_path, fs_cb);
                break;
            case UV_FS_CHMOD:
                mode = var_int(args[1]);
                result = uv_fs_chmod(uvLoop, req, path, mode, fs_cb);
                break;
            case UV_FS_UTIME:
                atime = var_double(args[1]);
                mtime = var_double(args[2]);
                result = uv_fs_utime(uvLoop, req, path, atime, mtime, fs_cb);
                break;
            case UV_FS_CHOWN:
                uid = var_unsigned_char(args[1]);
                gid = var_unsigned_char(args[2]);
                result = uv_fs_chown(uvLoop, req, path, uid, gid, fs_cb);
                break;
            case UV_FS_LINK:
                n_path = var_char_ptr(args[1]);
                result = uv_fs_link(uvLoop, req, path, n_path, fs_cb);
                break;
            case UV_FS_SYMLINK:
                n_path = var_char_ptr(args[1]);
                flags = var_int(args[2]);
                result = uv_fs_symlink(uvLoop, req, path, n_path, flags, fs_cb);
                break;
            case UV_FS_RMDIR:
                result = uv_fs_rmdir(uvLoop, req, path, fs_cb);
                break;
            case UV_FS_LSTAT:
                n_path = var_char_ptr(args[1]);
                result = uv_fs_lstat(uvLoop, req, path, fs_cb);
                break;
            case UV_FS_STAT:
                result = uv_fs_stat(uvLoop, req, path, fs_cb);
                break;
            case UV_FS_STATFS:
                result = uv_fs_statfs(uvLoop, req, path, fs_cb);
                break;
            case UV_FS_SCANDIR:
                flags = var_int(args[1]);
                result = uv_fs_scandir(uvLoop, req, path, flags, fs_cb);
                break;
            case UV_FS_READLINK:
                result = uv_fs_readlink(uvLoop, req, path, fs_cb);
                break;
            case UV_FS_UNKNOWN:
            case UV_FS_CUSTOM:
            default:
                fprintf(stderr, "type; %d not supported.\n", fs->fs_type);
                break;
        }
    } else {
        uv_file fd = var_int(args[0]);
        switch (fs->fs_type) {
            case UV_FS_FSTAT:
                result = uv_fs_fstat(uvLoop, req, fd, fs_cb);
                break;
            case UV_FS_SENDFILE:
                in_fd = var_int(args[1]);
                offset = var_long_long(args[2]);
                length = var_size_t(args[3]);
                result = uv_fs_sendfile(uvLoop, req, fd, in_fd, offset, length, fs_cb);
                break;
            case UV_FS_CLOSE:
                result = uv_fs_close(uvLoop, req, fd, fs_cb);
                break;
            case UV_FS_FSYNC:
                result = uv_fs_fsync(uvLoop, req, fd, fs_cb);
                break;
            case UV_FS_FDATASYNC:
                result = uv_fs_fdatasync(uvLoop, req, fd, fs_cb);
                break;
            case UV_FS_FTRUNCATE:
                offset = var_long_long(args[1]);
                result = uv_fs_ftruncate(uvLoop, req, fd, offset, fs_cb);
                break;
            case UV_FS_FCHMOD:
                mode = var_int(args[1]);
                result = uv_fs_fchmod(uvLoop, req, fd, mode, fs_cb);
                break;
            case UV_FS_FUTIME:
                atime = var_double(args[1]);
                mtime = var_double(args[2]);
                result = uv_fs_futime(uvLoop, req, fd, atime, mtime, fs_cb);
                break;
            case UV_FS_FCHOWN:
                uid = var_unsigned_char(args[1]);
                gid = var_unsigned_char(args[2]);
                result = uv_fs_fchown(uvLoop, req, fd, uid, gid, fs_cb);
                break;
            case UV_FS_READ:
                offset = var_long_long(args[1]);
                result = uv_fs_read(uvLoop, req, fd, &fs->bufs, 1, offset, fs_cb);
                break;
            case UV_FS_WRITE:
                offset = var_long_long(args[1]);
                result = uv_fs_write(uvLoop, req, fd, &fs->bufs, 1, offset, fs_cb);
                break;
            case UV_FS_UNKNOWN:
            case UV_FS_CUSTOM:
            default:
                fprintf(stderr, "type; %d not supported.\n", fs->fs_type);
                break;
        }
    }

    if (result) {
        return uv_error_post(co_active(), result);
    }

    fs->context = co_active();
    uv_req_set_data((uv_req_t *)req, (void_t)fs);
    return 0;
}

static void_t uv_init(void_t uv_args) {
    uv_args_t *uv = (uv_args_t *)uv_args;
    values_t *args = uv->args;
    int result = -4083;

    uv_handle_t *stream = var_cast(uv_handle_t, args[0]);
    uv->context = co_active();
    if (uv->is_request) {
        uv_req_t *req;
        switch (uv->req_type) {
            case UV_WRITE:
                req = co_new_by(1, sizeof(uv_write_t));
                result = uv_write((uv_write_t *)req, (uv_stream_t *)stream, &uv->bufs, 1, write_cb);
                break;
            case UV_CONNECT:
                req = co_new_by(1, sizeof(uv_connect_t));
                int scheme = var_int(args[2]);
                switch (scheme) {
                    case UV_NAMED_PIPE:
                        uv->handle_type = UV_NAMED_PIPE;
                        uv_pipe_connect((uv_connect_t *)req, (uv_pipe_t *)stream, var_const_char_512(args[3]), connect_cb);
                        result = 0;
                        break;
                    default:
                        uv->handle_type = UV_TCP;
                        result = uv_tcp_connect((uv_connect_t *)req, (uv_tcp_t *)stream, var_cast(const struct sockaddr, args[1]), connect_cb);
                        break;
                }
                break;
            case UV_SHUTDOWN:
                break;
            case UV_UDP_SEND:
            case UV_WORK:
                break;
            case UV_GETADDRINFO:
                break;
            case UV_GETNAMEINFO:
                break;
            case UV_RANDOM:
                break;
            case UV_UNKNOWN_REQ:
            default:
                fprintf(stderr, "type; %d not supported.\n", uv->req_type);
                break;
        }

        uv_req_set_data(req, (void_t)uv);
    } else {
        uv_handle_set_data(stream, (void_t)uv);
        switch (uv->handle_type) {
            case UV_ASYNC:
                break;
            case UV_CHECK:
                break;
            case UV_FS_EVENT:
                break;
            case UV_FS_POLL:
                break;
            case UV_IDLE:
                break;
            case UV_NAMED_PIPE:
                break;
            case UV_POLL:
                break;
            case UV_PREPARE:
                break;
            case UV_STREAM + UV_NAMED_PIPE + UV_TCP + UV_UDP:
                result = uv_listen((uv_stream_t *)stream, var_int(args[1]), connection_cb);
                if (!result) {
                    char ip[32];
                    struct sockaddr name;
                    int length;
                    memset(&name, -1, sizeof length);
                    length = sizeof name;
                    switch (uv->bind_type) {
                        case UV_NAMED_PIPE:
                            break;
                        case UV_UDP:
                            result = uv_udp_getsockname((const uv_udp_t *)stream, &name, &length);
                            break;
                        default:
                            result = uv_tcp_getsockname((const uv_tcp_t *)stream, &name, &length);
                            break;
                    }

                    if (co_strpos(var_char_ptr(args[3]), ":") >= 0) {
                        uv_ip6_name((const struct sockaddr_in6 *)&name, (char *)ip, sizeof ip);
                    } else if (co_strpos(var_char_ptr(args[3]), ".") >= 0) {
                        uv_ip4_name((const struct sockaddr_in *)&name, (char *)ip, sizeof ip);
                    }

                    if (uv_server_args == NULL) {
                        uv_server_args = uv;
                    }

                    ZE_INFO("Listening to %s:%d for connections.\n", ip, var_int(args[4]));
                    scheduler_info_log = false;
                }
                break;
            case UV_PROCESS:
                break;
            case UV_STREAM:
                result = uv_read_start((uv_stream_t *)stream, alloc_cb, read_cb);
                break;
            case UV_TCP:
                break;
            case UV_HANDLE:
                result = 0;
                uv_close(stream, close_cb);
                break;
            case UV_TIMER:
                break;
            case UV_TTY:
                break;
            case UV_UDP:
                break;
            case UV_SIGNAL:
                break;
            case UV_FILE:
                break;
            case UV_UNKNOWN_HANDLE:
            default:
                fprintf(stderr, "type; %d not supported.\n", uv->handle_type);
                break;
        }
    }

    if (result) {
        uv->context->context->loop_code = result;
        uv_error_post(uv->context, result);
        co_resuming(uv->context->context);
    }

    return 0;
}

uv_file fs_open(string_t path, int flags, int mode) {
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(3, sizeof(values_t));

    args[0].value.char_ptr = (string)path;
    args[1].value.integer = flags;
    args[2].value.integer = mode;

    return (uv_file)fs_start(uv_args, args, UV_FS_OPEN, 3, true).integer;
}

int fs_unlink(string_t path) {
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(1, sizeof(values_t));
    args[0].value.char_ptr = (string)path;

    return (uv_file)fs_start(uv_args, args, UV_FS_UNLINK, 1, true).integer;
}

uv_stat_t *fs_fstat(uv_file fd) {
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(1, sizeof(values_t));
    args[0].value.integer = fd;

    return (uv_stat_t *)fs_start(uv_args, args, UV_FS_FSTAT, 1, false).object;
}

int fs_fsync(uv_file fd) {
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(1, sizeof(values_t));
    args[0].value.integer = fd;

    return fs_start(uv_args, args, UV_FS_FSYNC, 1, false).integer;
}

string fs_read(uv_file fd, int64_t offset) {
    uv_stat_t *stat = fs_fstat(fd);
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    uv_args->buffer = co_new_by(1, stat->st_size + 1);
    uv_args->bufs = uv_buf_init(uv_args->buffer, stat->st_size);

    args = (values_t *)co_new_by(2, sizeof(values_t));
    args[0].value.integer = fd;
    args[1].value.u_int = offset;

    return fs_start(uv_args, args, UV_FS_READ, 2, false).char_ptr;
}

int fs_write(uv_file fd, string_t text, int64_t offset) {
    size_t size = sizeof(text) + 1;
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    uv_args->buffer = co_new_by(1, size);
    memcpy(uv_args->buffer, text, size);
    uv_args->bufs = uv_buf_init(uv_args->buffer, size);

    args = (values_t *)co_new_by(2, sizeof(values_t));
    args[0].value.integer = fd;
    args[1].value.u_int = offset;

    return fs_start(uv_args, args, UV_FS_WRITE, 2, false).integer;
}

int fs_close(uv_file fd) {
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(1, sizeof(values_t));

    args[0].value.integer = fd;
    return fs_start(uv_args, args, UV_FS_CLOSE, 1, false).integer;
}

string fs_readfile(string_t path) {
    uv_file fd = fs_open(path, O_RDONLY, 0);
    if (fd) {
        string file = fs_read(fd, 0);
        fs_close(fd);

        return file;
    }

    return (string)0;
}

void stream_handler(void (*connected)(uv_stream_t *), uv_stream_t *client) {
    co_handler(FUNC_VOID(connected), client, uv_close_free);
}

int stream_write(uv_stream_t *handle, string_t text) {
    if (handle == NULL)
        return -1;

    size_t size = strlen(text) + 1;
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    uv_args->buffer = co_new_by(1, size);
    memcpy(uv_args->buffer, text, size);
    uv_args->bufs = uv_buf_init(uv_args->buffer, size);

    args = (values_t *)co_new_by(1, sizeof(values_t));
    args[0].value.object = handle;

    return uv_start(uv_args, args, UV_WRITE, 1, true).integer;
}

string stream_read(uv_stream_t *handle) {
    if (handle == NULL)
        return NULL;

    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(1, sizeof(values_t));
    args[0].value.object = handle;

    return uv_start(uv_args, args, UV_STREAM, 1, false).char_ptr;
}

uv_stream_t *stream_connect(string_t address) {
    if (address == NULL)
        return NULL;

    url_parse_t *url = parse_url(address);
    if (url == NULL)
        return NULL;

    return stream_connect_ex(url->url_type, (string_t)url->host, url->port);
}

uv_stream_t *stream_connect_ex(uv_handle_type scheme, string_t address, int port) {
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;
    void_t addr_set = NULL;
    void_t handle = NULL;
    int r = 0;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    if (co_strpos(address, ":") >= 0) {
        r = uv_ip6_addr(address, port, (struct sockaddr_in6 *)uv_args->in6);
        addr_set = uv_args->in6;
    } else if (co_strpos(address, ".") >= 0) {
        r = uv_ip4_addr(address, port, (struct sockaddr_in *)uv_args->in4);
        addr_set = uv_args->in4;
    }

    if (!r) {
        return uv_error_post(co_active(), r);
    }

    switch (scheme) {
        case UV_NAMED_PIPE:
            handle = pipe_create(false);
            break;
        case UV_UDP:
            handle = udp_create();
            r = uv_udp_connect(handle, (const struct sockaddr *)addr_set);
            if (r) {
                return uv_error_post(co_active(), r);
            }
            return stream(handle);
        default:
            handle = tcp_create();
            break;
    }

    args = (values_t *)co_new_by(4, sizeof(values_t));
    args[0].value.object = handle;
    args[1].value.object = addr_set;
    args[2].value.integer = scheme;
    args[3].value.char_ptr = (string)address;

    return (uv_stream_t *)uv_start(uv_args, args, UV_CONNECT, 4, true).object;
}

uv_stream_t *stream_listen(uv_stream_t *stream, int backlog) {
    if (stream == NULL)
        return NULL;

    uv_args_t *uv_args = (uv_args_t *)uv_handle_get_data((uv_handle_t *)stream);
    values_t *args = uv_args->args;

    args[0].value.object = stream;
    args[1].value.integer = backlog;

    value_t handle = uv_start(uv_args, args, UV_STREAM + UV_NAMED_PIPE + UV_TCP + UV_UDP, 5, false);
    if (handle.object == NULL)
        return NULL;

    return (uv_stream_t *)handle.object;
}

uv_stream_t *stream_bind(string_t address, int flags) {
    if (address == NULL)
        return NULL;

    url_parse_t *url = parse_url(address);
    if (url == NULL)
        return NULL;

    return stream_bind_ex(url->url_type, (string_t)url->host, url->port, flags);
}

uv_stream_t *stream_bind_ex(uv_handle_type scheme, string_t address, int port, int flags) {
    void_t addr_set, handle;
    int r = 0;

    uv_args_t *uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    co_defer_recover(error_catch, uv_args);
    if (co_strpos(address, ":") >= 0) {
        r = uv_ip6_addr(address, port, (struct sockaddr_in6 *)uv_args->in6);
        addr_set = uv_args->in6;
    } else if (co_strpos(address, ".") >= 0) {
        r = uv_ip4_addr(address, port, (struct sockaddr_in *)uv_args->in4);
        addr_set = uv_args->in4;
    }

    if (!r) {
        switch (scheme) {
            case UV_NAMED_PIPE:
                handle = pipe_create(false);
                r = uv_pipe_bind(handle, address);
                break;
            case UV_UDP:
                handle = udp_create();
                r = uv_udp_bind(handle, (const struct sockaddr *)addr_set, flags);
                break;
            default:
                handle = tcp_create();
                r = uv_tcp_bind(handle, (const struct sockaddr *)addr_set, flags);
                break;
        }
    }

    if (r) {
        return uv_error_post(co_active(), r);
    }

    values_t *args = (values_t *)co_new_by(5, sizeof(values_t));

    args[0].value.object = handle;
    args[2].value.object = addr_set;
    args[3].value.char_ptr = (string)address;
    args[4].value.integer = port;

    uv_args->bind_type = scheme;
    uv_args->args = args;
    uv_handle_set_data((uv_handle_t *)handle, (void_t)uv_args);

    return stream(handle);
}

void coro_uv_close(uv_handle_t *handle) {
    if (handle == NULL)
        return;

    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(1, sizeof(values_t));
    args[0].value.object = handle;

    uv_start(uv_args, args, UV_HANDLE, 1, false);
}

uv_udp_t *udp_create() {
    uv_udp_t *udp = (uv_udp_t *)co_calloc_full(co_active(), 1, sizeof(uv_udp_t), uv_close_free);
    int r = uv_udp_init(co_loop(), udp);
    if (r) {
        return uv_error_post(co_active(), r);
    }

    return udp;
}

uv_pipe_t *pipe_create(bool is_ipc) {
    uv_pipe_t *pipe = (uv_pipe_t *)co_calloc_full(co_active(), 1, sizeof(uv_pipe_t), uv_close_free);
    int r = uv_pipe_init(co_loop(), pipe, (int)is_ipc);
    if (r) {
        return uv_error_post(co_active(), r);
    }

    return pipe;
}

uv_tcp_t *tcp_create() {
    uv_tcp_t *tcp = (uv_tcp_t *)co_calloc_full(co_active(), 1, sizeof(uv_tcp_t), uv_close_free);
    int r = uv_tcp_init(co_loop(), tcp);
    if (r) {
        return uv_error_post(co_active(), r);
    }

    return tcp;
}

uv_tty_t *tty_create(uv_file fd) {
    uv_tty_t *tty = (uv_tty_t *)co_calloc_full(co_active(), 1, sizeof(uv_tty_t), uv_close_free);
    int r = uv_tty_init(co_loop(), tty, fd, 0);
    if (r) {
        return uv_error_post(co_active(), r);
    }

    return tty;
}
