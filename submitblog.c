#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POSTS_FILE "posts.html"

int main(void) {
    printf("Content-type: text/html\n\n");

    // Retrieve the content length
    char *contentLength = getenv("CONTENT_LENGTH");
    if (!contentLength) {
        printf("<html><body>Error: CONTENT_LENGTH not set.</body></html>");
        return 1;
    }

    // Allocate memory for POST data
    int len = atoi(contentLength);
    char *postData = (char *)malloc(len + 1);
    if (!postData) {
        printf("<html><body>Error: Memory allocation failed.</body></html>");
        return 1;
    }

    // Read POST data
    fread(postData, 1, len, stdin);
    postData[len] = '\0';

    // Extract title and content from POST data
    char title[256], content[1024];
    sscanf(postData, "title=%[^&]&content=%s", title, content);

    // Open posts file for appending
    FILE *file = fopen(POSTS_FILE, "a");
    if (!file) {
        printf("<html><body>Error: Failed to open posts file.</body></html>");
        free(postData);
        return 1;
    }

    // Write new blog post to file
    fprintf(file, "<div><h2>%s</h2><p>%s</p></div>\n", title, content);
    fclose(file);

    // Redirect to index.html
    printf("Location: /index.html\n\n");

    free(postData);
    return 0;
}
