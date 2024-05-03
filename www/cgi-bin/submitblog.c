#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("Content-type: text/html\n\n");

    // Retrieve the content/body length from the environment variable
    char *body_length = getenv("CONTENT_LENGTH");
    if (!body_length) {
        printf("<html><body>Error: CONTENT_LENGTH not set or invalid.</body></html>");
        return 1;
    }

    // convert the body length to integer
    int len = atoi(body_length);
    if (len <= 0) {
        printf("<html><body>Error: Invalid CONTENT_LENGTH value.</body></html>");
        return 1;
    }

    // allocate memory for POST with malloc
    char *post_data = (char *)malloc(len + 1);
    if (!post_data) {
        printf("<html><body>Error: Memory allocation failed.</body></html>");
        return 1;
    }

    // POST data in stdin read in
    fread(post_data, 1, len, stdin);
    post_data[len] = '\0';

    // PARSING DONE HERE: parse the POST data here to extract the title and body
    char *title = strtok(post_data, "&");
    char *content = strtok(NULL, "&");

    // open posts.html to do appending
    FILE *file = fopen("posts.html", "a");
    if (!file) {
        printf("<html><body>Error: Unable to open posts.html.</body></html>");
        free(post_data);
        return 1;
    }

    // can write the blog post data to the file now
    fprintf(file, "<div><h2>%s</h2><p>%s</p></div>\n", title, content);
    fclose(file);

    // then once done, redirectd to index.html
    printf("HTTP/1.1 302 Found\r\n");
    printf("Location: /index.html\r\n\r\n");

    // free memory 
    free(post_data);

    return 0;
}
