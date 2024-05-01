#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX_PENDING 5
#define BUFFER_SIZE 1024
#define PORT 8080

void start_server(int port);
int create_socket(int port);
void handle_connections(int server_sock);
void process_request(int client_sock);
void handle_get_request(int client_sock, const char* path);
void handle_post_request(int client_sock, const char* path);
void handle_cgi_request(int client_sock, const char* script_path, const char* post_data);
void handle_head_request(int client_sock, const char* path);
void send_response(int client_sock, const char *header, const char *content_type, const char *body, int body_length);
const char* get_mime_type(const char *filename);

int main() {
    start_server(PORT);
    return 0;
}

void start_server(int port) {
    int server_sock = create_socket(port);
    if (server_sock < 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d\n", port);
    handle_connections(server_sock);
}

int create_socket(int port) {
    int sock;
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return -1;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Socket bind failed");
        close(sock);
        return -1;
    }

    if (listen(sock, MAX_PENDING) < 0) {
        perror("Socket listen failed");
        close(sock);
        return -1;
    }

    return sock;
}

void handle_connections(int server_sock) {
    int client_sock;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    while ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len)) >= 0) {
        process_request(client_sock);
        close(client_sock);
    }
}

void process_request(int client_sock) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    ssize_t bytes_read = read(client_sock, buffer, BUFFER_SIZE - 1);

    if (bytes_read <= 0) {
        // If no data is read, or there's a read error, respond with 400 Bad Request
        send_response(client_sock, "HTTP/1.1 400 Bad Request\r\n", "text/html", "<html><body><h1>400 Bad Request</h1></body></html>", 58);
        return;
    }

    char method[10], path[1024], protocol[10];
    if (sscanf(buffer, "%s %s %s", method, path, protocol) != 3) {
        // If the request line is malformed, not capturing three parts, send a 400 error
        send_response(client_sock, "HTTP/1.1 400 Bad Request\r\n", "text/html", "<html><body><h1>400 Bad Request</h1></body></html>", 58);
        return;
    }

    // Check for valid HTTP version
    if (strcmp(protocol, "HTTP/1.1") != 0 && strcmp(protocol, "HTTP/1.0") != 0) {
        send_response(client_sock, "HTTP/1.1 400 Bad Request\r\n", "text/html", "<html><body><h1>400 Bad Request</h1></body></html>", 58);
        return;
    }

    // Validate the method
    if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0 && strcmp(method, "POST") != 0) {  
        send_response(client_sock, "HTTP/1.1 405 Method Not Allowed\r\nAllow: GET, HEAD, POST\r\n\r\n", "text/html", "", 0);
        return;
    }

    if (strcmp(method, "GET") == 0) {
        handle_get_request(client_sock, path);
    } else if (strcmp(method, "HEAD") == 0) {
        handle_head_request(client_sock, path);
    } else {
        // Handle other methods or send an appropriate error response
        send_response(client_sock, "HTTP/1.1 405 Method Not Allowed\r\nAllow: GET, HEAD\r\n\r\n", "text/html", "", 0);
    }

    // Handle POST Requests
    if (strcmp(method, "POST") == 0) {
        if (strcmp(path, "cgi-bin/login.c") == 0 || strcmp(path, "cgi-bin/submitblog.c") == 0) {
            handle_post_request(client_sock, path);
            return; // Ensure no further processing for POST requests
        }
    }
}

void handle_get_request(int client_sock, const char* path) {
    // Implementation for handling GET requests
    char filepath[1024] = "www";
    strcat(filepath, path);

    // Logging the request
    printf("GET request for: %s\n", filepath);

    // Validate the requested path to prevent directory traversal attacks
    if (strstr(path, "../") != NULL || strstr(path, "//") != NULL) {
        // Path contains directory traversal characters, reject the request
        send_response(client_sock, "HTTP/1.1 400 Bad Request\r\n", "text/html", "<html><body><h1>400 Bad Request</h1></body></html>", 58);
        return;
    }

    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) {
        send_response(client_sock, "HTTP/1.1 404 Not Found\r\n", "text/html", "<html><body><h1>404 Not Found</h1></body></html>", 58);
        return;
    }

    const char* mime_type = get_mime_type(filepath);
    if (mime_type == NULL) {
        send_response(client_sock, "HTTP/1.1 415 Unsupported Media Type\r\n", "text/html", "<html><body><h1>415 Unsupported Media Type</h1></body></html>", 61);
        close(file_fd);
        return;
    }

    struct stat file_stat;
    if (fstat(file_fd, &file_stat) < 0) {
        perror("Server Error: Failed to obtain file information");
        send_response(client_sock, "HTTP/1.1 500 Internal Server Error\r\n", "text/html", "<html><body><h1>500 Internal Server Error</h1></body></html>", 59);
        close(file_fd);
        return;
    }

    char *file_buffer = malloc(file_stat.st_size);
    if (read(file_fd, file_buffer, file_stat.st_size) < file_stat.st_size) {
        perror("Server Error: Failed to read the file");
        send_response(client_sock, "HTTP/1.1 500 Internal Server Error\r\n", "text/html", "<html><body><h1>500 Internal Server Error</h1></body></html>", 59);
        free(file_buffer);
        close(file_fd);
        return;
    }

    send_response(client_sock, "HTTP/1.1 200 OK\r\n", mime_type, file_buffer, file_stat.st_size);
    free(file_buffer);
    close(file_fd);
}

void handle_post_request(int client_sock, const char* path) {
    // Implementation for handling POST requests
    char post_data[MAX_PENDING];
    ssize_t bytes_received = recv(client_sock, post_data, sizeof(post_data), 0);
    if (bytes_received <= 0) {
        perror("Error reading POST data");
        close(client_sock);
        return;
    }
    post_data[bytes_received] = '\0'; // Null-terminate the string

    handle_cgi_request(client_sock, path, post_data);
}

void handle_cgi_request(int client_sock, const char* script_path, const char* post_data) {
    // Implementation for handling CGI requests
    // Set up environment variables
    setenv("REQUEST_METHOD", "POST", 1);

    char content_length_str[32];
    sprintf(content_length_str, "%lu", strlen(post_data));
    setenv("CONTENT_LENGTH", content_length_str, 1);

    // Fork a child process
    pid_t pid = fork();
    if (pid < 0) {
        perror("Error forking process");
        send_response(client_sock, "HTTP/1.1 500 Internal Server Error\r\n", "text/html", "<html><body><h1>500 Internal Server Error</h1></body></html>", 59);
        return;
    } else if (pid == 0) {
        // Child process: execute CGI script
        dup2(client_sock, STDOUT_FILENO); // Redirect stdout to client socket
        execl(script_path, script_path, NULL); // Execute CGI script
        perror("Error executing CGI script");
        exit(EXIT_FAILURE);
    } else {
        // Parent process: wait for child to finish
        wait(NULL);
    }
}

void handle_head_request(int client_sock, const char* path) {
    // Implementation for handling HEAD requests
    char filepath[1024] = "www";
    strcat(filepath, path);
    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) {
        send_response(client_sock, "HTTP/1.1 404 Not Found\r\n", "text/html", "<html><body><h1>404 Not Found</h1></body></html>", 58);
        return;
    }

    const char* mime_type = get_mime_type(filepath);
    if (mime_type == NULL) {
        send_response(client_sock, "HTTP/1.1 415 Unsupported Media Type\r\n", "text/html", "<html><body><h1>415 Unsupported Media Type</h1></body></html>", 61);
        close(file_fd);
        return;
    }

    send_response(client_sock, "HTTP/1.1 200 OK\r\n", mime_type, "", 0);
    close(file_fd);
}

void send_response(int client_sock, const char *header, const char *content_type, const char *body, int body_length) {
    // Send HTTP response headers
    send(client_sock, header, strlen(header), 0);
    send(client_sock, "Content-Type: ", 14, 0);
    send(client_sock, content_type, strlen(content_type), 0);
    send(client_sock, "\r\n", 2, 0);
    send(client_sock, "Content-Length: ", 16, 0);
    dprintf(client_sock, "%d", body_length);
    send(client_sock, "\r\n\r\n", 4, 0);

    // Send response content
    send(client_sock, body, body_length, 0);
}

const char* get_mime_type(const char *filename) {
    // Implementation for determining MIME type based on file extension
    if (strstr(filename, ".html")) return "text/html";
    if (strstr(filename, ".css")) return "text/css";
    if (strstr(filename, ".js")) return "application/javascript";
    if (strstr(filename, ".png")) return "image/png";
    if (strstr(filename, ".jpeg") || strstr(filename, ".jpg")) return "image/jpeg";
    if (strstr(filename, ".gif")) return "image/gif";
    return "text/plain";
}
