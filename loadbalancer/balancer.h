#ifndef BALANCER_H
#define BALANCER_H

#include <pthread.h>

#define MAX_BACKENDS 20
#define MAX_GROUPS 10

typedef enum {
    ROUND_ROBIN,
    LEAST_CONNECTIONS
} Strategy;

typedef struct {
    char *host;
    int port;
    int active_connections;
    int is_alive;
} Backend;

typedef struct {
    char group_name[32];
    Backend backends[MAX_BACKENDS];
    int count;
    int rr_index;
    Strategy strategy;
} BackendGroup;

void init_backend_groups();
Backend *get_backend_for_group(const char *group_name);
void increment_connections(Backend *b);
void decrement_connections(Backend *b);
void *healthcheck_thread(void *arg);
void *metrics_server(void *arg);

extern BackendGroup backend_groups[MAX_GROUPS];
extern int group_count;
extern pthread_mutex_t conn_lock;

#endif