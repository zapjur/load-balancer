#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define DEFAULT_PORT 9003
#define BUFFER_SIZE 1024
#define PREFIX_BUFFER 256

void fatal(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int port = DEFAULT_PORT;

    char prefix[PREFIX_BUFFER] = "";

    if (argc >= 2) {
        port = atoi(argv[1]);
    }

    if (argc >= 3) {
        snprintf(prefix, PREFIX_BUFFER, "%s ", argv[2]);
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        fatal("socket failed");

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        fatal("setsockopt failed");

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        fatal("bind failed");

    if (listen(server_fd, 5) < 0)
        fatal("listen failed");

    printf("Echo backend listening on port %d...\n", port);

    while(1) {
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len)) < 0)
            fatal("accept failed");

        printf("Accepted connection from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        char buffer[BUFFER_SIZE];
        ssize_t bytes_received;

        while ((bytes_received = recv(client_fd, buffer, sizeof(buffer), 0)) > 0) {
            buffer[bytes_received] = '\0';
            char response[BUFFER_SIZE + PREFIX_BUFFER];
            snprintf(response, sizeof(response), "%s%s", prefix, buffer);
            send(client_fd, response, strlen(response), 0);
        }

        if (bytes_received < 0) {
            perror("recv failed");
        }

        close(client_fd);
    }
    close(server_fd);
    return 0;
}