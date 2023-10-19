#include <ze.h>

int co_main(int argc, char *argv[]) {
    url_parse_t *url = parse_url("http://secret:hideout@zelang.dev:80/this/is/a/very/deep/directory/structure/and/file.html?lots=1&of=2&parameters=3&too=4&here=5#some_page_ref123");

    printf("[url_type] => %d\n", url->url_type);
    printf("[scheme] => %s\n", url->scheme);
    printf("[host] => %s\n", url->host);
    printf("[user] => %s\n", url->user);
    printf("[pass] => %s\n", url->pass);
    printf("[port] => %d\n", url->port);
    printf("[path] => %s\n", url->path);
    printf("[query] => %s\n", url->query);
    printf("[fragment] => %s\n", url->fragment);

   if (url->query) {
       int i = 0;
       char **token = co_str_split(url->query, "&", &i);

       for (int x = 0; x < i; x++) {
           char **parts = co_str_split(token[x], "=", NULL);
           printf("%s = %s\n", parts[0], parts[1]);
       }
   }

   printf("%s\n", co_concat_by(3, "testing ", "this ", "thing"));
   return 0;
}
