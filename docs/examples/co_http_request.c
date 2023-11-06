#include <ze.h>

int co_main(int argc, char *argv[]) {
    http_t *parser = http_for(HTTP_REQUEST, NULL, 1.1);
    string request = co_concat_by(13, "GET /index.html HTTP/1.1", CRLF,
                                  "Host: url.com", CRLF,
                                  "Accept: */*", CRLF,
                                  "User-Agent: uv_client", CRLF,
                                  "X-Powered-By: ZeLang", CRLF,
                                  "Connection: close", CRLF, CRLF);

    if (is_str_eq(request, http_request(parser, HTTP_GET, "http://url.com/index.html", NULL, NULL, NULL, "x-powered-by=ZeLang;")))
        printf("\nThey match!\n\n%sThe uri is: %s\n", request, parser->uri);

    char raw[] = "POST /path?free=one&open=two HTTP/1.1\n\
 User-Agent: PHP-SOAP/BeSimpleSoapClient\n\
 Host: url.com:80\n\
 Accept: */*\n\
 Accept-Encoding: deflate, gzip\n\
 Content-Type:text/xml; charset=utf-8\n\
 Content-Length: 1108\n\
 Expect: 100-continue\n\
 \n\n\
 <b>hello world</b>";

    parse_http(parser, raw);

    ZE_ASSERT(is_str_eq("PHP-SOAP/BeSimpleSoapClient", get_header(parser, "User-Agent")));
    ZE_ASSERT(is_str_eq("url.com:80", get_header(parser, "Host")));
    ZE_ASSERT(is_str_eq("*/*", get_header(parser, "Accept")));
    ZE_ASSERT(is_str_eq("deflate, gzip", get_header(parser, "Accept-Encoding")));
    ZE_ASSERT(is_str_eq("text/xml; charset=utf-8", get_header(parser, "Content-Type")));
    ZE_ASSERT(is_str_eq("1108", get_header(parser, "Content-Length")));
    ZE_ASSERT(is_str_eq("100-continue", get_header(parser, "Expect")));
    ZE_ASSERT(is_str_eq("utf-8", get_variable(parser, "Content-Type", "charset")));
    ZE_ASSERT(is_str_eq("", get_variable(parser, "Expect", "charset")));

    ZE_ASSERT(has_header(parser, "Pragma") == false);

    ZE_ASSERT(is_str_eq("<b>hello world</b>", parser->body));
    ZE_ASSERT(is_str_eq("one", get_parameter(parser, "free")));
    ZE_ASSERT(is_str_eq("POST", parser->method));
    ZE_ASSERT(is_str_eq("/path", parser->path));
    ZE_ASSERT(is_str_eq("HTTP/1.1", parser->protocol));

    return 0;
}
