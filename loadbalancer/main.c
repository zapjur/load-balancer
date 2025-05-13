#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>
#include <netdb.h>

#define LISTEN_PORT 8080
#define BACKEND_IP "backend1"
#define BACKEND_PORT 9001
#define BUFFER_SIZE 4096

void fatal(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int connect_to_backend(const char *ip, int port) {
    int sockfd;
    struct sockaddr_in backend_addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        fatal("socket");

    memset(&backend_addr, 0, sizeof(backend_addr));
    backend_addr.sin_family = AF_INET;
    backend_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &backend_addr.sin_addr) <= 0) {
        struct hostent *he = gethostbyname(ip);
        if (!he) fatal("gethostbyname");
        memcpy(&backend_addr.sin_addr, he->h_addr, he->h_length);
    }

    if (connect(sockfd, (struct sockaddr *)&backend_addr, sizeof(backend_addr)) < 0)
        fatal("connect to backend");

    return sockfd;
}

int main() {
    int listen_fd, client_fd, backend_fd;
    struct sockaddr_in listen_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) fatal("socket");

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(LISTEN_PORT);

    if (bind(listen_fd, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0)
        fatal("bind");

    if (listen(listen_fd, 5) < 0)
        fatal("listen");

    printf("Load Balancer listening on port %d...\n", LISTEN_PORT);

    while (1) {
        client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
            fatal("accept");

        printf("Accepted connection from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        backend_fd = connect_to_backend(BACKEND_IP, BACKEND_PORT);

        fd_set fds;
        char buffer[BUFFER_SIZE];
        int maxfd = (client_fd > backend_fd ? client_fd : backend_fd) + 1;

        while (1) {
            FD_ZERO(&fds);
            FD_SET(client_fd, &fds);
            FD_SET(backend_fd, &fds);

            int activity = select(maxfd, &fds, NULL, NULL, NULL);
            if (activity < 0 && errno != EINTR)
                break;

            if (FD_ISSET(client_fd, &fds)) {
                ssize_t n = recv(client_fd, buffer, BUFFER_SIZE, 0);
                if (n <= 0) break;
                send(backend_fd, buffer, n, 0);
            }

            if (FD_ISSET(backend_fd, &fds)) {
                ssize_t n = recv(backend_fd, buffer, BUFFER_SIZE, 0);
                if (n <= 0) break;
                send(client_fd, buffer, n, 0);
            }
        }

        close(client_fd);
        close(backend_fd);
        printf("Connection closed.\n");
    }

    close(listen_fd);
    return 0;
}
