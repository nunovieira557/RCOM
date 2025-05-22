#include "FTP.h"
#include <ctype.h>

// Parses the FTP URL into its components (user, password, host, resource, and file).
int parse(char *input, struct URL *url) {

    regex_t regex;
    regcomp(&regex, BAR, 0); // Check if the URL starts with "ftp://".
    if (regexec(&regex, input, 0, NULL, 0)) return -1;

    // Determine if the URL includes user credentials.
    regcomp(&regex, AT, 0);
    if (regexec(&regex, input, 0, NULL, 0) != 0) { // No credentials in URL.
        sscanf(input, HOST_REGEX, url->host); // Extract host.
        strcpy(url->user, DEFAULT_USER); // Use default user.
        strcpy(url->password, DEFAULT_PASSWORD); // Use default password.
    } else { // URL includes user and password.
        sscanf(input, HOST_AT_REGEX, url->host); // Extract host.
        sscanf(input, USER_REGEX, url->user); // Extract user.
        sscanf(input, PASS_REGEX, url->password); // Extract password.
    }

    // Extract resource path and file name.
    sscanf(input, RESOURCE_REGEX, url->resource);
    strcpy(url->file, strrchr(input, '/') + 1);

    // Resolve hostname to IP address.
    struct hostent *h;
    if (strlen(url->host) == 0) return -1;
    if ((h = gethostbyname(url->host)) == NULL) {
        printf("Invalid hostname '%s'\n", url->host);
        exit(-1);
    }
    strcpy(url->ip, inet_ntoa(*((struct in_addr *) h->h_addr)));

    // Verify if essential URL components are present.
    return !(strlen(url->host) && strlen(url->user) && 
           strlen(url->password) && strlen(url->resource) && strlen(url->file));
}

// Creates a socket and connects to the given IP and port.
int createSocket(char *ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    // Initialize server address structure.
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    // Create socket.
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }

    // Connect to server.
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    return sockfd;
}

// Reads and processes the server's response - versão corrigida baseada no downloadapp.c
int readResponse(const int socket, char* buffer) {
    char temp[MAX_LENGTH];
    memset(buffer, 0, MAX_LENGTH);

    while (1) {
        memset(temp, 0, MAX_LENGTH);
        int n = read(socket, temp, MAX_LENGTH - 1);
        if (n <= 0)
            break;

        strncat(buffer, temp, MAX_LENGTH - strlen(buffer) - 1);
        printf("Server: %s", temp);  // Debug output

        // Procura por linha de resposta que termina com espaço
        char* line_copy = strdup(temp);
        char* line = strtok(line_copy, "\r\n");
        while (line != NULL) {
            if (strlen(line) >= 4 &&
                isdigit(line[0]) && isdigit(line[1]) && isdigit(line[2]) &&
                line[3] == ' ') {
                int code = atoi(line);
                free(line_copy);
                return code; // Resposta válida como "230 "
            }
            line = strtok(NULL, "\r\n");
        }
        free(line_copy);
    }

    return 0; // Erro ou resposta incompleta
}

// Authenticates the user using the provided username and password - versão corrigida
int authConn(const int socket, const char* user, const char* pass) {
    char userCommand[5+strlen(user)+2+1]; // +2 para \r\n, +1 para \0
    char passCommand[5+strlen(pass)+2+1]; // +2 para \r\n, +1 para \0
    char answer[MAX_LENGTH];
    int response;

    // Envia comando USER
    sprintf(userCommand, "USER %s\r\n", user);
    printf("Sending: %s", userCommand);
    write(socket, userCommand, strlen(userCommand));
    
    response = readResponse(socket, answer);
    printf("USER response: %d - %s\n", response, answer);
    
    if (response != SV_READY4PASS && response != 331) {
        printf("Unknown user '%s'. Server response: %d - %s\n", user, response, answer);
        return -1;
    }

    // Envia comando PASS
    sprintf(passCommand, "PASS %s\r\n", pass);
    printf("Sending: %s", passCommand);
    write(socket, passCommand, strlen(passCommand));
    
    response = readResponse(socket, answer);
    printf("PASS response: %d - %s\n", response, answer);
    
    if (response != 230) {
        printf("PASS command failed: %s\n", answer);
        return -1;
    }
    printf("Logged in successfully!\n");
    
    return 230; // SV_LOGINSUCCESS
}

// Enables passive mode and retrieves data connection details (IP and port) - versão corrigida
int passiveMode(const int socket, char *ip, int *port) {
    char answer[MAX_LENGTH];
    int ip1, ip2, ip3, ip4, port1, port2;
    
    // Envia comando PASV
    printf("Sending: PASV\r\n");
    write(socket, "PASV\r\n", 6);

    int response = readResponse(socket, answer);
    printf("PASV response: %d - %s\n", response, answer);
    
    if (response != SV_PASSIVE && response != 227) {
        printf("Passive mode failed with response: %d\n", response);
        return -1;
    }

    // Parse IP e porta da resposta do servidor usando a lógica do downloadapp.c
    const char* start = strchr(answer, '(');
    if (start == NULL) {
        printf("Could not parse passive mode response\n");
        return -1;
    }
    
    if (sscanf(start, "(%d,%d,%d,%d,%d,%d)", &ip1, &ip2, &ip3, &ip4, &port1, &port2) != 6) {
        printf("Could not parse IP and port from passive mode response\n");
        return -1;
    }
    
    *port = port1 * 256 + port2;
    sprintf(ip, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
    
    printf("Passive mode: IP=%s, Port=%d\n", ip, *port);
    return response;
}

// Sends a RETR command to request a file resource - versão corrigida
int requestResource(const int socket, char *resource) {
    char fileCommand[5+strlen(resource)+2+1]; // +2 para \r\n, +1 para \0
    char answer[MAX_LENGTH];

    sprintf(fileCommand, "RETR %s\r\n", resource);
    printf("Sending: %s", fileCommand);
    write(socket, fileCommand, strlen(fileCommand));
    
    int response = readResponse(socket, answer);
    printf("RETR response: %d - %s\n", response, answer);
    
    return response;
}

// Downloads the requested file and saves it locally.
int getResource(const int socketA, const int socketB, char *filename) {
    FILE *fd = fopen(filename, "wb");
    if (fd == NULL) {
        printf("Error opening or creating file '%s'\n", filename);
        exit(-1);
    }

    char buffer[MAX_LENGTH];
    ssize_t bytes;
    
    printf("Starting file transfer...\n");

    // Read from the data connection and write to the file.
    while ((bytes = read(socketB, buffer, MAX_LENGTH)) > 0) {
        if (fwrite(buffer, 1, bytes, fd) != bytes) {
            printf("Error writing to file\n");
            fclose(fd);
            return -1;
        }
        printf("Downloaded %zd bytes\n", bytes);
    }

    fclose(fd);
    close(socketB); // Close data connection
    
    printf("File transfer completed, reading final response...\n");
    return readResponse(socketA, buffer);
}

// Closes both control and data connections.
int closeConnection(const int socketA, const int socketB) {
    char answer[MAX_LENGTH];
    write(socketA, "QUIT\r\n", 6);
    int quit_response = readResponse(socketA, answer);
    close(socketA);
    // socketB já foi fechado em getResource
    return (quit_response != SV_GOODBYE && quit_response != 221) ? -1 : 0;
}

// Main function: orchestrates the FTP client workflow.
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: ./download ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-1);
    } 

    struct URL url;
    memset(&url, 0, sizeof(url));
    if (parse(argv[1], &url) != 0) {
        printf("Parse error. Usage: ./download ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-1);
    }
    
    printf("Host: %s\nResource: %s\nFile: %s\nUser: %s\nPassword: %s\nIP Address: %s\n", 
           url.host, url.resource, url.file, url.user, url.password, url.ip);

    char answer[MAX_LENGTH];
    printf("Trying to connect to %s:%d...\n", url.ip, FTP_PORT);
    int socketA = createSocket(url.ip, FTP_PORT);
    printf("Connected, socketA = %d\n", socketA);
    
    int welcome_response = readResponse(socketA, answer);
    if (socketA < 0 || (welcome_response != SV_READY4AUTH && welcome_response != 220)) {
        printf("Socket to '%s' and port %d failed\n", url.ip, FTP_PORT);
        exit(-1);
    }
    
    if (authConn(socketA, url.user, url.password) != 230) {
        printf("Authentication failed with username = '%s' and password = '%s'.\n", url.user, url.password);
        exit(-1);
    }
    
    // Definir tipo binário
    write(socketA, "TYPE I\r\n", 8);
    readResponse(socketA, answer);
    
    int port;
    char ip[MAX_LENGTH];
    int pasv_response = passiveMode(socketA, ip, &port);
    if (pasv_response != 227) {
        printf("Passive mode failed\n");
        exit(-1);
    }

    int socketB = createSocket(ip, port);
    if (socketB < 0) {
        printf("Socket to '%s:%d' failed\n", ip, port);
        exit(-1);
    }

    int retr_response = requestResource(socketA, url.resource);
    if (retr_response != SV_READY4TRANSFER && retr_response != 150 && retr_response != 125) {
        printf("Unknown resource '%s' in '%s:%d'\n", url.resource, ip, port);
        exit(-1);
    }

    int transfer_response = getResource(socketA, socketB, url.file);
    if (transfer_response != SV_TRANSFER_COMPLETE && transfer_response != 226) {
        printf("Error transferring file '%s' from '%s:%d'\n", url.file, ip, port);
        exit(-1);
    }

    if (closeConnection(socketA, 0) != 0) {
        printf("Sockets close error\n");
        exit(-1);
    }
    
    printf("CONSEGUISTE ZÉ!\n");
    return 0;
}
