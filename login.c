#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USERNAME "admin"
#define PASSWORD "password"

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

    // Extract username and password from POST data
    char username[64], password[64];
    sscanf(postData, "username=%[^&]&password=%s", username, password);

    // Check if credentials match
    if (strcmp(username, USERNAME) == 0 && strcmp(password, PASSWORD) == 0) {
        // Successful login: set session cookie and redirect
        char sessionID[17];
        generate_session_id(sessionID); // Function to generate session ID
        
        // Set cookie and redirect
        printf("Set-Cookie: session=%s; HttpOnly\n", sessionID);
        printf("Location: /index.html\n\n");
    } else {
        // Failed login: display error message
        printf("<html><body>Login failed. Please try again.</body></html>");
    }

    free(postData);
    return 0;
}
