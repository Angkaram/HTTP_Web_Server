// imports
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

//global constants
#define MAX_PENDING 5
#define BUFFER_SIZE 1024
#define PORT 8080

// function prototypes
void start_server(int port);
int create_socket(int port);
void handle_connections(int server_socket);
void process_request(int client_socket);
void handle_get_request(int client_socket, const char* path);
void handle_post_request(int client_socket, const char* path, const char* post_data);
void handle_cgi_request(int client_socket, const char* script_path, const char* post_data);
void handle_head_request(int client_socket, const char* path);
void send_response(int client_socket, const char *header, const char *content_type, const char *body, int body_length);
const char* get_mime_type(const char *filename);

// main method
int main() {
    start_server(PORT);
    return 0;
}

// create a socket to start the server on port 8080
void start_server(int port) {
    int server_socket = create_socket(port);
    if (server_socket < 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }
    printf("Server started on port %d\n", port);
    handle_connections(server_socket);
}

// all socket related preparation done here
int create_socket(int port) {
    int sock;
    struct sockaddr_in server_address;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return -1;
    }

    int option = 1; // 1 enables the SO_REUSEADDR option
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    // initialize with server addr info (family, IP addr, ANY)
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port); // htons is sneaky, converts to netowork 'byte order'

    if (bind(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
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

void handle_connections(int server_socket) {
    int client_socket;
    struct sockaddr_in client_address;
    socklen_t client_addr_len = sizeof(client_address);

    while ((client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_addr_len)) >= 0) {
        process_request(client_socket);
        close(client_socket);
    }
}

void process_request(int client_socket) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    ssize_t bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);

    if (bytes_read <= 0) {
        // if no bytes are read or a read error happens, respond with 400 Bad Request
        send_response(client_socket, "HTTP/1.1 400 Bad Request\n", "text/html", "<html><body><h1>400 Bad Request</h1></body></html>", 58);
        return;
    }

    // Log the full HTTP request
    printf("LOG: Full HTTP request:\n%s\n", buffer);

    char method[10], path[1024], protocol[10];
    if (sscanf(buffer, "%s %s %s", method, path, protocol) != 3) {
        // if the request line is not created correctly or not capturing three parts, send a 400 error
        send_response(client_socket, "HTTP/1.1 400 Bad Request\n", "text/html", "<html><body><h1>400 Bad Request</h1></body></html>", 58);
        return;
    }

    // Log the headers and path
    printf("LOG: HEADERS FOLLOW:\n%s\n", buffer);
    printf("LOG: PATH FOLLOWS:\n%s\n", path);

    // Extract POST data from the request buffer
    char* post_data_start = strstr(buffer, "\r\n\r\n");
    char* post_data = NULL;
    if (post_data_start != NULL) {
        post_data = post_data_start + 4; // jump past "\r\n\r\n"
    }

    // Log the body if it exists
    if (post_data != NULL) {
        printf("LOG: BODY FOLLOWS:\n%s\n", post_data);
    }

    // also check for a valid HTTP version, else 400 error
    if (strcmp(protocol, "HTTP/1.1") != 0 && strcmp(protocol, "HTTP/1.0") != 0) {
        send_response(client_socket, "HTTP/1.1 400 Bad Request\n", "text/html", "<html><body><h1>400 Bad Request</h1></body></html>", 58);
        return;
    }

    // valid method or get a 405
    if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0 && strcmp(method, "POST") != 0) {  
        send_response(client_socket, "HTTP/1.1 405 Method Not Allowed\nAllow: GET, HEAD, POST\n\n", "text/html", "", 0);
        return;
    }

    if (strcmp(method, "GET") == 0) {
        handle_get_request(client_socket, path);
    } else if (strcmp(method, "HEAD") == 0) {
        handle_head_request(client_socket, path);
    } else if (strcmp(method, "POST") == 0) {
        if (post_data == NULL) {
            // if no post data is found, respond with 400 error
            send_response(client_socket, "HTTP/1.1 400 Bad Request\n", "text/html", "<html><body><h1>400 Bad Request</h1></body></html>", 58);
            return;
        }

        printf("CGI script path: %s\n", path);

        // CGI path and related code
        // first check if the request path matches the CGI script paths
        if (strcmp(path, "cgi-bin/login.cgi") == 0 || strcmp(path, "cgi-bin/submitblog.cgi") == 0) {
            // if so, set the correct MIME type for CGI script response
            send_response(client_socket, "HTTP/1.1 200 OK\n", "text/html", "", 0);
            // now handle the CGI request
            handle_cgi_request(client_socket, path, post_data);
            return; // stop processing once done
        } else { // no CGI script, so handle POST requests
            handle_post_request(client_socket, path, post_data);
            return;
        }
    }
}

void handle_get_request(int client_socket, const char* path) {
    char filepath[1024] = "www";
    strcat(filepath, path);
    printf("GET request for: %s\n", filepath); // Debugging statement

    // log the request in terminal window
    printf("GET request for: %s\n", filepath);

    // security and validation for the requested path to prevent illegal directory traversal
    if (strstr(path, "../") != NULL || strstr(path, "//") != NULL) {
        // if the path contains directory traversal characters, reject it
        send_response(client_socket, "HTTP/1.1 400 Bad Request\n", "text/html", "<html><body><h1>400 Bad Request</h1></body></html>", 58);
        return;
    }

    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) { // if file descriptor doesnt exist, 404 error
        send_response(client_socket, "HTTP/1.1 404 Not Found\n", "text/html", "<html><body><h1>404 Not Found</h1></body></html>", 58);
        return;
    }

    const char* mime_type = get_mime_type(filepath);
    if (mime_type == NULL) { // wrong or unsupported MIME type, so throw 415
        send_response(client_socket, "HTTP/1.1 415 Unsupported Media Type\n", "text/html", "<html><body><h1>415 Unsupported Media Type</h1></body></html>", 61);
        close(file_fd);
        return;
    }

    struct stat file_stat;
    if (fstat(file_fd, &file_stat) < 0) { // pretty much all other errors, give 500 error
        perror("Server Error: Failed to obtain file information");
        send_response(client_socket, "HTTP/1.1 500 Internal Server Error\n", "text/html", "<html><body><h1>500 Internal Server Error</h1></body></html>", 59);
        close(file_fd);
        return;
    }

    char *file_buffer = malloc(file_stat.st_size);
    if (read(file_fd, file_buffer, file_stat.st_size) < file_stat.st_size) {
        perror("Server Error: Failed to read the file");
        send_response(client_socket, "HTTP/1.1 500 Internal Server Error\n", "text/html", "<html><body><h1>500 Internal Server Error</h1></body></html>", 59);
        free(file_buffer);
        close(file_fd);
        return;
    }

    // if we are not finding any errors, OK
    send_response(client_socket, "HTTP/1.1 200 OK\n", mime_type, file_buffer, file_stat.st_size);
    free(file_buffer); // free the buffer once done
    close(file_fd); // close the file descriptor
}

void handle_post_request(int client_socket, const char* path, const char* post_data_param) {
    char post_data[MAX_PENDING];
    ssize_t bytes_received = recv(client_socket, post_data, sizeof(post_data), 0);
    if (bytes_received <= 0) {
        perror("Error reading POST data");
        close(client_socket);
        return;
    }
    post_data[bytes_received] = '\0'; // Yes, you should Null-terminate the string

    handle_cgi_request(client_socket, path, post_data);
}

void handle_cgi_request(int client_socket, const char* script_path, const char* post_data) {
    // environment variables
    setenv("REQUEST_METHOD", "POST", 1);

    char body_length_str[35]; // professor said this was the length
    sprintf(body_length_str, "%lu", strlen(post_data));
    setenv("CONTENT_LENGTH", body_length_str, 1);

    // here i fork a child process
    pid_t pid = fork();
    if (pid < 0) { // throw error if pid < 0
        perror("Error forking process");
        send_response(client_socket, "HTTP/1.1 500 Internal Server Error\r\n", "text/html", "<html><body><h1>500 Internal Server Error</h1></body></html>", 59);
        return;
    } else if (pid == 0) { // otherwise execute the CGI script as a child process using dup2
        dup2(client_socket, STDOUT_FILENO); // this redirects stdout to the client socket
        execl(script_path, script_path, NULL); // execl executes the CGI script
        perror("Error executing CGI script");
        exit(EXIT_FAILURE);
    } else {
        // now the parent waits for child to finish
        wait(NULL);
        // Read the output from the CGI script and send it back to the client
        char cgi_output[1024];
        ssize_t bytes_read;
        while ((bytes_read = read(client_socket, cgi_output, sizeof(cgi_output))) > 0) {
            send(client_socket, cgi_output, bytes_read, 0);
        }
    }
}

// similar to handle_get_request
void handle_head_request(int client_socket, const char* path) {
    char filepath[1024] = "www";
    strcat(filepath, path);
    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) {
        send_response(client_socket, "HTTP/1.1 404 Not Found\r\n", "text/html", "<html><body><h1>404 Not Found</h1></body></html>", 58);
        return;
    }

    const char* mime_type = get_mime_type(filepath);
    if (mime_type == NULL) {
        send_response(client_socket, "HTTP/1.1 415 Unsupported Media Type\r\n", "text/html", "<html><body><h1>415 Unsupported Media Type</h1></body></html>", 61);
        close(file_fd);
        return;
    }
    send_response(client_socket, "HTTP/1.1 200 OK\r\n", mime_type, "", 0);
    close(file_fd);
}

void send_response(int client_socket, const char *header, const char *content_type, const char *body, int body_length) {
    // response headers
    send(client_socket, header, strlen(header), 0);
    send(client_socket, "Content-Type: ", 14, 0);
    send(client_socket, content_type, strlen(content_type), 0);
    send(client_socket, "\r\n", 2, 0);
    send(client_socket, "Content-Length: ", 16, 0);
    dprintf(client_socket, "%d", body_length);
    send(client_socket, "\r\n\r\n", 4, 0);

    // now send the response body/content
    send(client_socket, body, body_length, 0);
}

// handle all MIME types required
const char* get_mime_type(const char *filename) {
    printf("Checking MIME type for file: %s\n", filename); // Debugging statement

    if (strstr(filename, ".html")) return "text/html";
    if (strstr(filename, ".css")) return "text/css";
    if (strstr(filename, ".js")) return "application/javascript";
    if (strstr(filename, ".png")) return "image/png";
    if (strstr(filename, ".jpeg") || strstr(filename, ".jpg")) return "image/jpeg";
    if (strstr(filename, ".gif")) return "image/gif";
    return "text/plain";
}
