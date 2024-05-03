// similar to Professor Posnett's testcgi.c file

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

    // convert body length to integer
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

    // PARSING DONE HERE: parse the POST data here to extract the username and password
    char *username = strtok(post_data, "&");
    char *password = strtok(NULL, "&");

    // obviously a security issue but this is a test
    char *expectedUsername = "admin";
    char *expectedPassword = "password";

    // auth
    if (strcmp(username, expectedUsername) == 0 && strcmp(password, expectedPassword) == 0) {
        printf("HTTP/1.1 302 Found\r\n");
        printf("Location: /index.html\r\n");
        printf("Set-Cookie: sessionID=%s; HttpOnly\r\n\r\n", "generated_session_id");
    } else {
        // auth failed
        printf("<html><body>Error: Invalid username or password.</body></html>");
    }

    // free memory once done
    free(post_data);

    return 0;
}
