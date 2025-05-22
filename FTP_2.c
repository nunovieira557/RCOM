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

// Authenticates the user using the provided username and password.
int authConn(const int socket, const char* user, const char* pass) {
    char userCommand[5+strlen(user)+1];
    char passCommand[5+strlen(pass)+1];
    char answer[MAX_LENGTH];

    // Send USER command.
    sprintf(userCommand, "user %s\r\n", user);
    
    //debug    
    printf("user %s\n",user);

    write(socket, userCommand, strlen(userCommand));
    
    if (readResponse(socket, answer) != SV_READY4PASS) {
        printf("Unknown user '%s'. Abort.\n", user);
    //printf("USER CORRETO\n");      
        exit(-1);
    }

    // Send PASS command.
    sprintf(passCommand, "pass %s\r\n", pass);
    write(socket, passCommand, strlen(passCommand));
    
    /*if (readResponse(socket, answer) != 230) {
        printf("PASS command failed: %s\n", answer);
        exit(1);
    }*/
    printf("Logged in successfully!\n");
    
    return readResponse(socket, answer);
}

// Enables passive mode and retrieves data connection details (IP and port).
int passiveMode(const int socket, char *ip, int *port) {
    char answer[MAX_LENGTH];
    int ip1, ip2, ip3, ip4, port1, port2;
    
    // Send PASV command.
    write(socket, "pasv\r\n", 5);

    int count = readResponse(socket, answer);
    printf("%d",count);
    
    if (readResponse(socket, answer) != SV_PASSIVE) return -1;

    // Parse IP and port from server's response.
    sscanf(answer, PASSIVE_REGEX, &ip1, &ip2, &ip3, &ip4, &port1, &port2);
    *port = port1 * 256 + port2;
    sprintf(ip, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);

    return SV_PASSIVE;
}

// Reads and processes the server's response.
/*int readResponse(const int socket, char* buffer) {
    char byte;
    int index = 0, responseCode;
    ResponseState state = START;
    memset(buffer, 0, MAX_LENGTH);
    printf("AQUIIIII4");
    // Read response line by line until the end.
    while (state != END) {
        read(socket, &byte, 1);
        switch (state) {
            case START:
                printf("AQUIIIII S\n");
                if (byte == ' ') state = SINGLE;
                else if (byte == '-') state = MULTIPLE;
                else if (byte == '\n') state = END;
                else buffer[index++] = byte;
                break;
            case SINGLE:
                printf("AQUIIIII SI\n");
                if (byte == '\n') state = END;
                else buffer[index++] = byte;
                break;
            case MULTIPLE:
                printf("AQUIIIII MUL\n");
                if (byte == '\n') {
                    memset(buffer, 0, MAX_LENGTH);
                    state = START;
                    index = 0;
                } else buffer[index++] = byte;
                break;
            case END:
                break;
            default:
                break;
        }
    }

    // Extract response code from the buffer.
    //sscanf(buffer, RESPCODE_REGEX, &responseCode);
    //responseCode =331;    
    printf("%d\n",responseCode);
    return responseCode;
}*/

/*
int readResponse(const int socket, char* buffer) {
    char line[MAX_LENGTH];
    int code = 0;
    int done = 0;

    memset(buffer, 0, MAX_LENGTH);

    while (!done) {
        int index = 0;
        char byte;

        // ler linha até \n
        while (index < MAX_LENGTH - 1) {
            ssize_t r = read(socket, &byte, 1);
            if (r <= 0) return -1;  // erro leitura

            line[index++] = byte;
            if (byte == '\n') break;
        }
        line[index] = '\0';

        // concatena linha no buffer principal
        strncat(buffer, line, MAX_LENGTH - strlen(buffer) - 1);

        // se a linha tem 4 ou mais caracteres, tenta interpretar código
        if (index >= 4 && isdigit(line[0]) && isdigit(line[1]) && isdigit(line[2])) {
            int currentCode = (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');

            // Se é o primeiro código que aparece, guarda
            if (code == 0) {
                code = currentCode;
            }

            // Se o quarto caractere é espaço, termina a leitura
            if (line[3] == ' ') {
                done = 1;
            }
            // Se for '-', é multiline, continua a ler
        }
    }
    printf("AAAAAAAAAAA%d\n",code);
    return code;
}
*/
int readResponse(const int socket, char* buffer) {
    char line[MAX_LENGTH];
    int responseCode = -1;
    int firstResponse = 1;
    memset(buffer, 0, MAX_LENGTH);
    
    printf("Reading server response...\n");
    
    while (1) {
        memset(line, 0, MAX_LENGTH);
        int i = 0;
        char byte;
        
        // Read one line at a time
        while (i < MAX_LENGTH - 1) {
            int bytesRead = read(socket, &byte, 1);
            if (bytesRead <= 0) {
                printf("Error reading from socket or connection closed (bytes read: %d)\n", bytesRead);
                return -1;
            }
            
            if (byte == '\r') {
                continue; // Skip carriage return
            }
            
            if (byte == '\n') {
                break; // End of line
            }
            
            line[i++] = byte;
        }
        
        line[i] = '\0';
        printf("Received line: '%s'\n", line);
        
        // Extract response code from the first 3 characters
        if (strlen(line) >= 3 && firstResponse) {
            responseCode = (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');
            strcpy(buffer, line);
            firstResponse = 0;
        }
        
        // Check if this is the end of the response
        // Single line response: "XXX message"
        // Multi-line response ends with: "XXX message" (same code as first line)
        if (strlen(line) >= 4) {
            if (line[3] == ' ') {
                // Single line response or end of multi-line response
                break;
            } else if (line[3] == '-') {
                // Start of multi-line response, continue reading
                continue;
            }
        }
        
        // If we can't determine the format, assume single line
        if (strlen(line) >= 3) {
            break;
        }
    }
    
    printf("Final response code: %d\n", responseCode);
    return responseCode;
}


// Sends a RETR command to request a file resource.
int requestResource(const int socket, char *resource) {
    char fileCommand[5+strlen(resource)+1];
    char answer[MAX_LENGTH];

    sprintf(fileCommand, "retr %s\r\n", resource);
    write(socket, fileCommand, sizeof(fileCommand));
    return readResponse(socket, answer);
}

// Downloads the requested file and saves it locally.
int getResource(const int socketA, const int socketB, char *filename) {
    FILE *fd = fopen(filename, "wb");
    if (fd == NULL) {
        printf("Error opening or creating file '%s'\n", filename);
        exit(-1);
    }

    char buffer[MAX_LENGTH];
    int bytes;

    // Read from the data connection and write to the file.
    do {
        bytes = read(socketB, buffer, MAX_LENGTH);
        if (fwrite(buffer, bytes, 1, fd) < 0) return -1;
    } while (bytes);

    fclose(fd);
    return readResponse(socketA, buffer);
}

// Closes both control and data connections.
int closeConnection(const int socketA, const int socketB) {
    char answer[MAX_LENGTH];
    write(socketA, "quit\n", 5);
    if (readResponse(socketA, answer) != SV_GOODBYE) return -1;
    return close(socketA) || close(socketB);
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
    
    //int count = 1;//readResponse(socketA, answer);
    //printf("%d\n",count);
    
    if (socketA < 0 || readResponse(socketA, answer) != SV_READY4AUTH) {
        //
        printf("Socket to '%s' and port %d failed\n", url.ip, FTP_PORT);
        exit(-1);
    }
    int count = authConn(socketA, url.user, url.password);
    //printf("AAAAAAAAAAAAAAAAAA%d\n", count);
    if (authConn(socketA, url.user, url.password) != SV_LOGINSUCCESS) {
        printf("Authentication failed with username = '%s' and password = '%s'.\n", url.user, url.password);
        exit(-1);
    }
    
    int port;
    char ip[MAX_LENGTH];
    if (passiveMode(socketA, ip, &port) != SV_PASSIVE) {
        printf("Passive mode failed\n");
        exit(-1);
    }

    int socketB = createSocket(ip, port);
    if (socketB < 0) {
        printf("Socket to '%s:%d' failed\n", ip, port);
        exit(-1);
    }

    if (requestResource(socketA, url.resource) != SV_READY4TRANSFER) {
        printf("Unknown resource '%s' in '%s:%d'\n", url.resource, ip, port);
        exit(-1);
    }

    if (getResource(socketA, socketB, url.file) != SV_TRANSFER_COMPLETE) {
        printf("Error transferring file '%s' from '%s:%d'\n", url.file, ip, port);
        exit(-1);
    }

    if (closeConnection(socketA, socketB) != 0) {
        printf("Sockets close error\n");
        exit(-1);
    }

    return 0;
}
