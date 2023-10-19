#include "../include/ze.h"

uv_handle_type scheme_type(string scheme) {
    if (strcmp(scheme, "http") == 0
        || strcmp(scheme, "tcp") == 0
        || strcmp(scheme, "https") == 0
        || strcmp(scheme, "tls") == 0
        || strcmp(scheme, "ftp") == 0
        || strcmp(scheme, "ftps") == 0
        || strcmp(scheme, "ssl") == 0) {
        return UV_TCP;
    } else if (strcmp(scheme, "file") == 0 || strcmp(scheme, "unix") == 0) {
        return UV_NAMED_PIPE;
    } else if (strcmp(scheme, "udp") == 0) {
        return UV_UDP;
    } else {
        return UV_UNKNOWN_HANDLE;
    }
}

static int htoi(char *s) {
    int value;
    int c;

    c = ((unsigned char *)s)[0];
    if (isupper(c))
        c = tolower(c);
    value = (c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10) * 16;

    c = ((unsigned char *)s)[1];
    if (isupper(c))
        c = tolower(c);
    value += c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10;

    return (value);
}

/* rfc1738:

   ...The characters ";",
   "/", "?", ":", "@", "=" and "&" are the characters which may be
   reserved for special meaning within a scheme...

   ...Thus, only alphanumerics, the special characters "$-_.+!*'(),", and
   reserved characters used for their reserved purposes may be used
   unencoded within a URL...

   For added safety, we only leave -_. unencoded.
 */
static unsigned char hex_chars[] = "0123456789ABCDEF";

static const_t _memrchr(const_t s, int c, size_t n) {
    const unsigned char *e;
    if (0 == n) {
        return NULL;
    }

    for (e = (const unsigned char *)s + n - 1; e >= (const unsigned char *)s; e--) {
        if (*e == (const unsigned char)c) {
            return (const_t)e;
        }
    }

    return NULL;
}

static const unsigned char tolower_map[256] = {
0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
0x40,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x5b,0x5c,0x5d,0x5e,0x5f,
0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
};

static string_t method_strings[] = {
"DELETE", "GET", "HEAD", "POST", "PUT", "CONNECT", "OPTIONS", "TRACE", "COPY", "LOCK", "MKCOL", "MOVE", "PROPFIND", "PROPPATCH", "SEARCH", "UNLOCK", "REPORT", "MKACTIVITY", "CHECKOUT", "MERGE", "M-SEARCH", "NOTIFY", "SUBSCRIBE", "UNSUBSCRIBE", "PATCH", "PURGE"
};

#define tolower_ascii(c) (tolower_map[(unsigned char)(c)])

#define PROXY_CONNECTION "proxy-connection"
#define CONNECTION "connection"
#define CONTENT_LENGTH "content-length"
#define TRANSFER_ENCODING "transfer-encoding"
#define UPGRADE "upgrade"
#define CHUNKED "chunked"
#define KEEP_ALIVE "keep-alive"
#define CLOSE "close"

int binary_strcasecmp(string_t s1, size_t len1, string_t s2, size_t len2)
{
    size_t len;
    int c1, c2;

    if (s1 == s2) {
        return 0;
    }

    len = MIN(len1, len2);
    while (len--) {
        c1 = tolower_ascii(*(unsigned char *)s1++);
        c2 = tolower_ascii(*(unsigned char *)s2++);
        if (c1 != c2) {
            return c1 - c2;
        }
    }

    return (int)(len1 - len2);
}

#define string_equals_literal_ci(str, c) \
	(strlen(str) == sizeof(c) - 1 && !binary_strcasecmp(str, strlen(str), (c), sizeof(c) - 1))

static string_t binary_strcspn(string_t s, string_t e, string_t chars) {
    while (*chars) {
        string_t p = memchr(s, *chars, e - s);
        if (p) {
            e = p;
        }
        chars++;
    }
    return e;
}

char *replace_ctrl_ex(char *str, size_t len) {
    unsigned char *s = (unsigned char *)str;
    unsigned char *e = (unsigned char *)str + len;

    if (!str) {
        return (NULL);
    }

    while (s < e) {

        if (iscntrl(*s)) {
            *s = '_';
        }
        s++;
    }

    return (str);
}

url_parse_t *url_parse_ex2(char const *str, size_t length, bool *has_port) {
    char port_buf[6];
    url_parse_t *ret = co_new_by(1, sizeof(url_parse_t));
    char const *s, *e, *p, *pp, *ue;

    *has_port = 0;
    s = str;
    ue = s + length;

    /* parse scheme */
    if ((e = memchr(s, ':', length)) && e != s) {
        /* validate scheme */
        p = s;
        while (p < e) {
            /* scheme = 1*[ lowalpha | digit | "+" | "-" | "." ] */
            if (!isalpha(*p) && !isdigit(*p) && *p != '+' && *p != '.' && *p != '-') {
                if (e + 1 < ue && e < binary_strcspn(s, ue, "?#")) {
                    goto parse_port;
                } else if (s + 1 < ue && *s == '/' && *(s + 1) == '/') { /* relative-scheme URL */
                    s += 2;
                    e = 0;
                    goto parse_host;
                } else {
                    goto just_path;
                }
            }
            p++;
        }

        if (e + 1 == ue) { /* only scheme is available */
            ret->scheme = co_string(s, (e - s));
            replace_ctrl_ex(ret->scheme, strlen(ret->scheme));
            return ret;
        }

        /*
         * certain schemas like mailto: and zlib: may not have any / after them
         * this check ensures we support those.
         */
        if (*(e + 1) != '/') {
            /* check if the data we get is a port this allows us to
             * correctly parse things like a.com:80
             */
            p = e + 1;
            while (p < ue && isdigit(*p)) {
                p++;
            }

            if ((p == ue || *p == '/') && (p - e) < 7) {
                goto parse_port;
            }

            ret->scheme = co_string(s, (e - s));
            replace_ctrl_ex(ret->scheme, strlen(ret->scheme));

            s = e + 1;
            goto just_path;
        } else {
            ret->scheme = co_string(s, (e - s));
            replace_ctrl_ex(ret->scheme, strlen(ret->scheme));

            if (e + 2 < ue && *(e + 2) == '/') {
                s = e + 3;
                if (string_equals_literal_ci(ret->scheme, "file")) {
                    if (e + 3 < ue && *(e + 3) == '/') {
                        /* support windows drive letters as in:
                           file:///c:/somedir/file.txt
                        */
                        if (e + 5 < ue && *(e + 5) == ':') {
                            s = e + 4;
                        }
                        goto just_path;
                    }
                }
            } else {
                s = e + 1;
                goto just_path;
            }
        }
    } else if (e) { /* no scheme; starts with colon: look for port */
    parse_port:
        p = e + 1;
        pp = p;

        while (pp < ue && pp - p < 6 && isdigit(*pp)) {
            pp++;
        }

        if (pp - p > 0 && pp - p < 6 && (pp == ue || *pp == '/')) {
            long port;
            char *end;
            memcpy(port_buf, p, (pp - p));
            port_buf[pp - p] = '\0';
            port = strtol(port_buf, &end, 10);
            if (port >= 0 && port <= 65535 && end != port_buf) {
                *has_port = 1;
                ret->port = (unsigned short)port;
                if (s + 1 < ue && *s == '/' && *(s + 1) == '/') { /* relative-scheme URL */
                    s += 2;
                }
            } else {
                return NULL;
            }
        } else if (p == pp && pp == ue) {
            return NULL;
        } else if (s + 1 < ue && *s == '/' && *(s + 1) == '/') { /* relative-scheme URL */
            s += 2;
        } else {
            goto just_path;
        }
    } else if (s + 1 < ue && *s == '/' && *(s + 1) == '/') { /* relative-scheme URL */
        s += 2;
    } else {
        goto just_path;
    }

parse_host:
    e = binary_strcspn(s, ue, "/?#");

    /* check for login and password */
    if ((p = _memrchr(s, '@', (e - s)))) {
        if ((pp = memchr(s, ':', (p - s)))) {
            ret->user = co_string(s, (pp - s));
            replace_ctrl_ex(ret->user, strlen(ret->user));

            pp++;
            ret->pass = co_string(pp, (p - pp));
            replace_ctrl_ex(ret->pass, strlen(ret->pass));
        } else {
            ret->user = co_string(s, (p - s));
            replace_ctrl_ex(ret->user, strlen(ret->user));
        }

        s = p + 1;
    }

    /* check for port */
    if (s < ue && *s == '[' && *(e - 1) == ']') {
        /* Short circuit portscan,
           we're dealing with an
           IPv6 embedded address */
        p = NULL;
    } else {
        p = _memrchr(s, ':', (e - s));
    }

    if (p) {
        if (!ret->port) {
            p++;
            if (e - p > 5) { /* port cannot be longer then 5 characters */
                return NULL;
            } else if (e - p > 0) {
                long port;
                char *end;
                memcpy(port_buf, p, (e - p));
                port_buf[e - p] = '\0';
                port = strtol(port_buf, &end, 10);
                if (port >= 0 && port <= 65535 && end != port_buf) {
                    *has_port = 1;
                    ret->port = (unsigned short)port;
                } else {
                    return NULL;
                }
            }
            p--;
        }
    } else {
        p = e;
    }

    /* check if we have a valid host, if we don't reject the string as url */
    if ((p - s) < 1) {
        return NULL;
    }

    ret->host = co_string(s, (p - s));
    replace_ctrl_ex(ret->host, strlen(ret->host));

    if (e == ue) {
        return ret;
    }

    s = e;

just_path:

    e = ue;
    p = memchr(s, '#', (e - s));
    if (p) {
        p++;
        if (p < e) {
            ret->fragment = co_string(p, (e - p));
            replace_ctrl_ex(ret->fragment, strlen(ret->fragment));
        }
        e = p - 1;
    }

    p = memchr(s, '?', (e - s));
    if (p) {
        p++;
        if (p < e) {
            ret->query = co_string(p, (e - p));
            replace_ctrl_ex(ret->query, strlen(ret->query));
        }
        e = p - 1;
    }

    if (s < e || s == ue) {
        ret->path = co_string(s, (e - s));
        replace_ctrl_ex(ret->path, strlen(ret->path));
    }

    return ret;
}

url_parse_t *url_parse_ex(char const *str, size_t length) {
    bool has_port;
    return url_parse_ex2(str, length, &has_port);
}

url_parse_t *parse_url(char const *str) {
    url_parse_t *url = url_parse_ex(str, strlen(str));
    url->url_type = scheme_type(url->scheme);

    return url;
}

string url_encode(char const *s, size_t len) {
    register unsigned char c;
    unsigned char *to;
    unsigned char const *from, *end;
    string start, ret;

    from = (unsigned char *)s;
    end = (unsigned char *)s + len;
    start = CO_CALLOC(1, 3 * len);
    to = (unsigned char *)start;

    while (from < end) {
        c = *from++;

        if (c == ' ') {
            *to++ = '+';
        } else if ((c < '0' && c != '-' && c != '.') ||
                   (c < 'A' && c > '9') ||
                   (c > 'Z' && c < 'a' && c != '_') ||
                   (c > 'z')) {
            to[0] = '%';
            to[1] = hex_chars[c >> 4];
            to[2] = hex_chars[c & 15];
            to += 3;
        } else {
            *to++ = c;
        }
    }

    *to = '\0';
    size_t rlen = to - (unsigned char *)start;
    ret = co_new_by(1, rlen);
    memcpy(ret, start, rlen + 1);
    CO_FREE(start);

    return ret;
}

string url_decode(char *str, size_t len) {
    char *dest = str;
    char *data = str;

    while (len--) {
        if (*data == '+') {
            *dest = ' ';
        } else if (*data == '%' && len >= 2 && isxdigit((int)*(data + 1))
                   && isxdigit((int)*(data + 2))) {
            *dest = (char)htoi(data + 1);
            data += 2;
            len -= 2;
        } else {
            *dest = *data;
        }
        data++;
        dest++;
    }
    *dest = '\0';
    return dest;
}

string_t url_status_str(uint16_t const status) {
    switch (status) {
        // Informational 1xx
        case 100: return "Continue";
        case 101: return "Switching Protocols";
        case 102: return "Processing"; // RFC 2518, obsoleted by RFC 4918
        case 103: return "Early Hints";

        // Success 2xx
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 203: return "Non-Authoritative Information";
        case 204: return "No Content";
        case 205: return "Reset Content";
        case 206: return "Partial Content";
        case 207: return "Multi-Status"; // RFC 4918
        case 208: return "Already Reported";
        case 226: return "IM Used";

        // Redirection 3xx
        case 300: return "Multiple Choices";
        case 301: return "Moved Permanently";
        case 302: return "Moved Temporarily";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 305: return "Use Proxy";
        case 306: return "Reserved";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";

        // Client Error 4xx
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 402: return "Payment Required";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 406: return "Not Acceptable";
        case 407: return "Proxy Authentication Required";
        case 408: return "Request Time-out";
        case 409: return "Conflict";
        case 410: return "Gone";
        case 411: return "Length Required";
        case 412: return "Precondition Failed";
        case 413: return "Request Entity Too Large";
        case 414: return "Request-URI Too Large";
        case 415: return "Unsupported Media Type";
        case 416: return "Requested Range Not Satisfiable";
        case 417: return "Expectation Failed";
        case 418: return "I'm a teapot";               // RFC 2324
        case 422: return "Unprocessable Entity";       // RFC 4918
        case 423: return "Locked";                     // RFC 4918
        case 424: return "Failed Dependency";          // RFC 4918
        case 425: return "Unordered Collection";       // RFC 4918
        case 426: return "Upgrade Required";           // RFC 2817
        case 428: return "Precondition Required";      // RFC 6585
        case 429: return "Too Many Requests";          // RFC 6585
        case 431: return "Request Header Fields Too Large";// RFC 6585
        case 444: return "Connection Closed Without Response";
        case 451: return "Unavailable For Legal Reasons";
        case 499: return "Client Closed Request";

        // Server Error 5xx
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Time-out";
        case 505: return "HTTP Version Not Supported";
        case 506: return "Variant Also Negotiates";    // RFC 2295
        case 507: return "Insufficient Storage";       // RFC 4918
        case 509: return "Bandwidth Limit Exceeded";
        case 510: return "Not Extended";               // RFC 2774
        case 511: return "Network Authentication Required"; // RFC 6585
        case 599: return "Network Connect Timeout Error";
        default: return NULL;
    }
}
