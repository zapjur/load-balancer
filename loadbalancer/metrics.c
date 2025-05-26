#include "balancer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>

#define METRICS_PORT 9090
#define METRICS_BUFFER_SIZE 8192

void *metrics_server(void *arg) {
    int server_fd, client_fd;
    struct sockaddr_in addr;
    char buffer[METRICS_BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[METRICS] socket");
        pthread_exit(NULL);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(METRICS_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[METRICS] bind");
        close(server_fd);
        pthread_exit(NULL);
    }

    if (listen(server_fd, 5) < 0) {
        perror("[METRICS] listen");
        close(server_fd);
        pthread_exit(NULL);
    }

    printf("[METRICS] Listening on port %d...\n", METRICS_PORT);

    while (1) {
        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("[METRICS] accept");
            continue;
        }

        read(client_fd, buffer, sizeof(buffer));

        if (strncmp(buffer, "GET /metrics", 12) != 0) {
            const char *notFound = "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nNot found";
            write(client_fd, notFound, strlen(notFound));
            close(client_fd);
            continue;
        }

        char response[METRICS_BUFFER_SIZE];
        char body[METRICS_BUFFER_SIZE] = "";

        pthread_mutex_lock(&conn_lock);
        for (int i = 0; i < group_count; i++) {
            BackendGroup *g = &backend_groups[i];
            for (int j = 0; j < g->count; j++) {
                Backend *b = &g->backends[j];
                char line[256];
                snprintf(line, sizeof(line),
                         "backend_up{group=\"%s\",host=\"%s\",port=\"%d\"} %d\n"
                         "backend_connections{group=\"%s\",host=\"%s\"} %d\n",
                         g->group_name, b->host, b->port, b->is_alive,
                         g->group_name, b->host, b->active_connections);
                strncat(body, line, sizeof(body) - strlen(body) - 1);
            }
        }
        pthread_mutex_unlock(&conn_lock);

        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %lu\r\n\r\n%s",
                 strlen(body), body);

        write(client_fd, response, strlen(response));
        close(client_fd);
    }

    close(server_fd);
    pthread_exit(NULL);
}
