#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("Content-type: text/html\n\n");

    // Retrieve the content length
    char *contentLength = getenv("CONTENT_LENGTH");
    if (!contentLength) {
        printf("<html><body>Error: CONTENT_LENGTH not set or invalid.</body></html>");
        return 1;
    }

    // Convert content length to integer
    int len = atoi(contentLength);
    if (len <= 0) {
        printf("<html><body>Error: Invalid CONTENT_LENGTH value.</body></html>");
        return 1;
    }

    // Allocate memory for POST data
    char *postData = (char *)malloc(len + 1);
    if (!postData) {
        printf("<html><body>Error: Memory allocation failed.</body></html>");
        return 1;
    }

    // Read POST data from stdin
    fread(postData, 1, len, stdin);
    postData[len] = '\0';

    // Parse POST data to extract title and content
    char *title = strtok(postData, "&");
    char *content = strtok(NULL, "&");

    // Open posts.html for appending
    FILE *file = fopen("posts.html", "a");
    if (!file) {
        printf("<html><body>Error: Unable to open posts.html.</body></html>");
        free(postData);
        return 1;
    }

    // Write the blog post data to the file
    fprintf(file, "<div><h2>%s</h2><p>%s</p></div>\n", title, content);
    fclose(file);

    // Redirect to index.html
    printf("HTTP/1.1 302 Found\r\n");
    printf("Location: /index.html\r\n\r\n");

    // Free allocated memory
    free(postData);

    return 0;
}
