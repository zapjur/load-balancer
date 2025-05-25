#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include "balancer.h"

#define PORT_SIMPLE 8080
#define PORT_ECHO 8081
#define BUFFER_SIZE 4096

typedef struct {
    int client_fd;
    const char *group;
} ThreadArgs;


void *handle_client(void *arg);

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

int create_listener(int port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) fatal("socket");

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        fatal("bind");

    if (listen(listen_fd, 10) < 0)
        fatal("listen");

    printf("Listening on port %d...\n", port);
    return listen_fd;
}

int main() {
    init_backend_groups();

    int listen_fd_simple = create_listener(PORT_SIMPLE);
    int listen_fd_echo   = create_listener(PORT_ECHO);

    fd_set readfds;
    int maxfd = (listen_fd_simple > listen_fd_echo ? listen_fd_simple : listen_fd_echo) + 1;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(listen_fd_simple, &readfds);
        FD_SET(listen_fd_echo, &readfds);

        if (select(maxfd, &readfds, NULL, NULL, NULL) < 0 && errno != EINTR)
            fatal("select");

        int client_fd;
        const char *group;

        if (FD_ISSET(listen_fd_simple, &readfds)) {
            client_fd = accept(listen_fd_simple, (struct sockaddr *)&client_addr, &client_len);
            group = "simple";
        } else if (FD_ISSET(listen_fd_echo, &readfds)) {
            client_fd = accept(listen_fd_echo, (struct sockaddr *)&client_addr, &client_len);
            group = "echo";
        } else {
            continue;
        }

        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        printf("Accepted from %s:%d for group [%s]\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), group);

        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        args->client_fd = client_fd;
        args->group = group;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, args) != 0) {
            perror("pthread_create");
            close(client_fd);
            free(args);
            continue;
        }

        pthread_detach(tid);
    }
}

void *handle_client(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    int client_fd = args->client_fd;
    const char *group = args->group;
    free(args);

    Backend *backend = get_backend_for_group(group);
    if (!backend) {
        fprintf(stderr, "No backend found for group: %s\n", group);
        close(client_fd);
        return NULL;
    }

    increment_connections(backend);

    int backend_fd = connect_to_backend(backend->host, backend->port);

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
    decrement_connections(backend);
    printf("Connection closed.\n");

    return NULL;
}

