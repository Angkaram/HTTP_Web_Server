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

    // Parse POST data to extract username and password
    char *username = strtok(postData, "&");
    char *password = strtok(NULL, "&");

    // Hard-coded credentials (replace with actual authentication logic)
    char *expectedUsername = "admin";
    char *expectedPassword = "password";

    // Authenticate user
    if (strcmp(username, expectedUsername) == 0 && strcmp(password, expectedPassword) == 0) {
        printf("HTTP/1.1 302 Found\r\n");
        printf("Location: /index.html\r\n");
        printf("Set-Cookie: sessionID=%s; HttpOnly\r\n\r\n", "generated_session_id");
    } else {
        // Display error message (authentication failed)
        printf("<html><body>Error: Invalid username or password.</body></html>");
    }

    // Free allocated memory
    free(postData);

    return 0;
}
