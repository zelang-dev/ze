#include "../include/ze.h"

static void_t fs_init(void_t);
static void_t uv_init(void_t);

static void _close_cb(uv_handle_t *handle) {
    ZE_FREE(handle);
}

static void uv_close_free(void_t handle) {
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
    co_result_set(co, NULL);
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

    if (nread < 0) {
        if (nread == UV_EOF)
            co->halt = true;
        else
            fprintf(stderr, "Error: %s\n", uv_strerror(nread));

        co_result_set(co, (nread == UV_EOF ? 0 : &nread));
        uv_read_stop(stream);
    } else {
        co_result_set(co, (nread > 0 ? buf->base : NULL));
    }

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

    defer(uv_fs_req_cleanup, req);
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
        fprintf(stderr, "failed: %s\n", uv_strerror(result));
        return ZE_ERROR;
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
                break;
            case UV_SHUTDOWN:
                break;
            case UV_UDP_SEND:
                break;
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
        fprintf(stderr, "failed: %s\n", uv_strerror(result));
        return ZE_ERROR;
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
    string file = fs_read(fd, 0);
    fs_close(fd);

    return file;
}

int stream_write(uv_stream_t *handle, string_t text) {
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
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(1, sizeof(values_t));
    args[0].value.object = handle;

    return uv_start(uv_args, args, UV_STREAM, 1, false).char_ptr;
}

void coro_uv_close(uv_handle_t *handle) {
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(1, sizeof(values_t));
    args[0].value.object = handle;

    uv_start(uv_args, args, UV_HANDLE, 1, false);
}

uv_pipe_t *pipe_create(bool is_ipc) {
    uv_pipe_t *pipe = (uv_pipe_t *)co_calloc_full(co_active(), 1, sizeof(uv_pipe_t), uv_close_free);
    int r = uv_pipe_init(co_loop(), pipe, (int)is_ipc);
    if (r) {
        co_panic(uv_strerror(r));
    }

    return pipe;
}

uv_tcp_t *tcp_create(void) {
    uv_tcp_t *tcp = (uv_tcp_t *)co_calloc_full(co_active(), 1, sizeof(uv_tcp_t), uv_close_free);
    int r = uv_tcp_init(co_loop(), tcp);
    if (r) {
        co_panic(uv_strerror(r));
    }

    return tcp;
}

uv_tty_t *tty_create(uv_file fd) {
    uv_tty_t *tty = (uv_tty_t *)co_calloc_full(co_active(), 1, sizeof(uv_tty_t), uv_close_free);
    int r = uv_tty_init(co_loop(), tty, fd, 0);
    if (r) {
        co_panic(uv_strerror(r));
    }

    return tty;
}
