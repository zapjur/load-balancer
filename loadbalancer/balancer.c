#include "balancer.h"
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

static BackendGroup backend_groups[MAX_GROUPS];
static int group_count = 0;
static pthread_mutex_t conn_lock = PTHREAD_MUTEX_INITIALIZER;

void init_backend_groups() {
    BackendGroup simple = {
      .group_name = "simple",
      .strategy = ROUND_ROBIN,
      .count = 2,
      .rr_index = 0
    };

    simple.backends[0] = (Backend) {
        .host = "backend1",
        .port = 9001,
        .active_connections = 0
    };

    simple.backends[1] = (Backend) {
            .host = "backend2",
            .port = 9001,
            .active_connections = 0
    };
    backend_groups[group_count++] = simple;

    BackendGroup echo = {
            .group_name = "echo",
            .strategy = LEAST_CONNECTIONS,
            .count = 2,
            .rr_index = 0
    };
    echo.backends[0] = (Backend) {
            .host = "backend3",
            .port = 9001,
            .active_connections = 0
    };

    echo.backends[1] = (Backend) {
            .host = "backend4",
            .port = 9001,
            .active_connections = 0
    };
    backend_groups[group_count++] = echo;
}

Backend *get_backend_for_group(const char *group_name) {
    for (int i = 0; i < group_count; i++) {
        BackendGroup *group = &backend_groups[i];
        if (strcmp(group->group_name, group_name) == 0) {
            if (group->strategy == ROUND_ROBIN) {
                for (int attempts = 0; attempts < group->count; attempts++) {
                    Backend *b = &group->backends[group->rr_index];
                    group->rr_index = (group->rr_index + 1) % group->count;
                    if (b->is_alive) return b;
                }
            } else if (group->strategy == LEAST_CONNECTIONS) {
                Backend *least = NULL;
                for (int j = 0; j < group->count; j++) {
                    if (!group->backends[j].is_alive) continue;
                    if (!least || group->backends[j].active_connections < least->active_connections) {
                        least = &group->backends[j];
                    }
                }
                return least;
            }
        }
    }
    return NULL;
}

int check_backend_alive(Backend *b) {
    int sockfd;
    struct sockaddr_in addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return 0;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(b->port);

    struct hostent *he = gethostbyname(b->host);
    if (!he) {
        close(sockfd);
        return 0;
    }
    memcpy(&addr.sin_addr, he->h_addr, he->h_length);

    int result = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    close(sockfd);
    return (result == 0);
}

void *healthcheck_thread(void *arg) {
    while (1) {
        for (int i = 0; i < group_count; i++) {
            BackendGroup *group = &backend_groups[i];
            for (int j = 0; j < group->count; j++) {
                Backend *b = &group->backends[j];
                int alive = check_backend_alive(b);
                pthread_mutex_lock(&conn_lock);
                b->is_alive = alive;
                pthread_mutex_unlock(&conn_lock);

                printf("[Healthcheck] %s:%d is %s\n",
                       b->host, b->port, alive ? "UP" : "DOWN");
            }
        }
        sleep(5);
    }
    return NULL;
}

void increment_connections(Backend *b) {
    pthread_mutex_lock(&conn_lock);
    b->active_connections++;
    pthread_mutex_unlock(&conn_lock);
}

void decrement_connections(Backend *b) {
    pthread_mutex_lock(&conn_lock);
    if (b->active_connections > 0)
        b->active_connections--;
    pthread_mutex_unlock(&conn_lock);
}