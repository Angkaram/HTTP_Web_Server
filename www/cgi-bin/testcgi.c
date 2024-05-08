#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {

    // Retrieve the content length from the environment variable
    char *len_str = getenv("CONTENT_LENGTH");
    if (!len_str) {
        printf("HTTP/1.1 400 Bad Request\n\n");
        printf("<html><body>Error: CONTENT_LENGTH not set.</body></html>");
        return 1;
    }

    int len = atoi(len_str);
    if (len == 0) {
        printf("HTTP/1.1 200 OK\n\n");
        printf("<html><body>No word provided.</body></html>");
        return 0;
    }

    // Allocate memory for the input data and read it from stdin
    char *data = (char *)malloc(len + 1);
    if (!data) {
        printf("HTTP/1.1 500 Internal Server Error\n\n");
        printf("<html><body>Error: Failed to allocate memory.</body></html>");
        return 1;
    }

    fread(data, 1, len, stdin);
    data[len] = '\0'; // Null-terminate the input

    // Output the HTTP response
    printf("HTTP/1.1 200 OK\n\n");
    printf("<html><body>You provided: %s</body></html>", data);
    free(data);
    return 0;
}