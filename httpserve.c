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
#include "httpserve.h"

#define MAX_PENDING 5
#define FILE_NOT_FOUND "404.html"
#define DEFAULT_FILE "index.html"

int main(int argc, char *argv[]) {
    int port = SERVER_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    start_server(port);
    return 0;
}

void start_server(int port) {
    int server_sock = create_socket(port);
    if (server_sock < 0) {
        perror("Failed to create socket");
        exit(1);
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

    fstat(file_fd, &file_stat);
    read(file_fd, file_buffer, file_stat.st_size);
    send_response(client_sock, "HTTP/1.1 200 OK\r\n", mime_type, file_buffer, file_stat.st_size);
    free(file_buffer);
    close(file_fd);
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

    struct stat file_stat;
    if (fstat(file_fd, &file_stat) < 0) {
        perror("Server Error: Failed to obtain file information");
        send_response(client_sock, "HTTP/1.1 500 Internal Server Error\r\n", "text/html", "<html><body><h1>500 Internal Server Error</h1></body></html>", 59);
        close(file_fd);
        return;
    }

    fstat(file_fd, &file_stat);
    send_response(client_sock, "HTTP/1.1 200 OK\r\n", mime_type, "", 0);
    close(file_fd);
}

void handle_post_request(int client_sock, const char* path) {
    // Handle POST request logic here
    // Currently not implemented, sending 405 Method Not Allowed
    send_response(client_sock, "HTTP/1.1 405 Method Not Allowed\r\nAllow: GET, HEAD, POST\r\n\r\n", "text/html", "", 0);
}

void send_response(int client_sock, const char *header, const char *content_type, const char *body, int body_length) {
    char response[BUFFER_SIZE];
    sprintf(response, "%sContent-Type: %s\r\nContent-Length: %d\r\n\r\n%s", header, content_type, body_length, body);
    send(client_sock, response, strlen(response), 0);
    if (body_length > 0 && body != NULL) {
        send(client_sock, body, body_length, 0); // Send the body separately for binary data and textual data
    }
}

const char* get_mime_type(const char *filename) {
    if (strstr(filename, ".html")) return "text/html";
    if (strstr(filename, ".css")) return "text/css";
    if (strstr(filename, ".js")) return "application/javascript";
    if (strstr(filename, ".png")) return "image/png";
    if (strstr(filename, ".jpeg") || strstr(filename, ".jpg")) return "image/jpeg";
    if (strstr(filename, ".gif")) return "image/gif";
    return "text/plain";
}
