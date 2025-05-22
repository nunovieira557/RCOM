#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <ctype.h>
#define BUFFER_SIZE 1024

// Cria socket e conecta ao servidor
int connect_socket(const char* host, int port) {
    struct hostent* server = gethostbyname(host);
    if (!server) {
        fprintf(stderr, "Erro: host nao encontrado: %s\n", host);
        return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) return -1;
    return sock;
}

// Le resposta do servidor e imprime
int read_response(int sock, char* buffer) {
    char temp[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    while (1) {
        memset(temp, 0, BUFFER_SIZE);
        int n = read(sock, temp, BUFFER_SIZE - 1);
        if (n <= 0)
            break;

        strncat(buffer, temp, BUFFER_SIZE - strlen(buffer) - 1);
        printf("%s", temp);  // mostra o que foi lido (útil para debug)

        // procura por linha de resposta que termina com espaço
        char* line = strtok(temp, "\r\n");
        while (line != NULL) {
            if (strlen(line) >= 4 &&
                isdigit(line[0]) && isdigit(line[1]) && isdigit(line[2]) &&
                line[3] == ' ') {
                return atoi(line); // resposta válida como "230 "
            }
            line = strtok(NULL, "\r\n");
        }
    }

    return 0; // erro ou resposta incompleta
}



// Envia comando para o servidor
void send_cmd(int sock, const char* cmd) {
    char full_cmd[BUFFER_SIZE];
    snprintf(full_cmd, BUFFER_SIZE, "%s\r\n", cmd);
    write(sock, full_cmd, strlen(full_cmd));
}

// Extrai IP e porto do modo PASV
int parse_pasv(const char* resp, char* ip, int* port) {
    int h1, h2, h3, h4, p1, p2;
    const char* start = strchr(resp, '(');
    if (!start) return -1;

    if (sscanf(start, "(%d,%d,%d,%d,%d,%d)", &h1, &h2, &h3, &h4, &p1, &p2) != 6)
        return -1;

    sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);
    *port = p1 * 256 + p2;
    return 0;
}


// Faz parsing simples do URL
void parse_url(const char* url, char* user, char* pass, char* host, char* path) {
    // Ex: ftp://user:pass@host/path
    sscanf(url, "ftp://%99[^:]:%99[^@]@%99[^/]/%199[^\n]", user, pass, host, path);
    if (strlen(user) == 0) strcpy(user, "anonymous");
    if (strlen(pass) == 0) strcpy(pass, "guest");
    if (strlen(host) == 0) {
        sscanf(url, "ftp://%99[^/]/%199[^\n]", host, path);
        strcpy(user, "anonymous");
        strcpy(pass, "guest");
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Uso: %s ftp://[user[:pass]@]host/path\n", argv[0]);
        return 1;
    }

    char user[100] = "", pass[100] = "", host[100] = "", path[200] = "";
    char cmd[BUFFER_SIZE], buffer[BUFFER_SIZE], data_ip[32];
    int data_port;

    parse_url(argv[1], user, pass, host, path);

    // Ligacao de controlo
    int ctrl = connect_socket(host, 21);
    if (ctrl < 0) { perror("Erro socket ctrl"); return 1; }

    read_response(ctrl, buffer);
    sprintf(cmd, "USER %s", user); send_cmd(ctrl, cmd); read_response(ctrl, buffer);
    sprintf(cmd, "PASS %s", pass); send_cmd(ctrl, cmd); read_response(ctrl, buffer);
    send_cmd(ctrl, "TYPE I"); read_response(ctrl, buffer);
    send_cmd(ctrl, "PASV"); read_response(ctrl, buffer); printf("Resposta PASV:\n%s\n", buffer);

    if (parse_pasv(buffer, data_ip, &data_port) != 0) {
        fprintf(stderr, "Erro ao obter dados PASV\n"); return 1;
    }

    int data = connect_socket(data_ip, data_port);
    if (data < 0) { perror("Erro socket data"); return 1; }

    sprintf(cmd, "RETR %s", path); send_cmd(ctrl, cmd); read_response(ctrl, buffer);

    // Guardar nome do ficheiro
    char* fname = strrchr(path, '/');
    FILE* f = fopen(fname ? fname + 1 : path, "wb");
    if (!f) { perror("Erro a criar ficheiro"); return 1; }

    ssize_t n;
    while ((n = read(data, buffer, BUFFER_SIZE)) > 0)
        fwrite(buffer, 1, n, f);

    fclose(f);
    close(data);
    read_response(ctrl, buffer);
    send_cmd(ctrl, "QUIT"); read_response(ctrl, buffer);
    close(ctrl);

    printf("Download concluido.\n");
    return 0;
}