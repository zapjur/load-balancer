#ifndef BALANCER_H
#define BALANCER_H

#define MAX_BACKENDS 10
#define MAX_GROUPS 10

typedef enum {
    ROUND_ROBIN,
    LEAST_CONNECTIONS
} Strategy;

typedef struct {
    char *host;
    int port;
    int active_connections;
} Backend;

typedef struct {
    char *group_name;
    Backend backends[MAX_BACKENDS];
    int count;
    int rr_index;
    Strategy strategy;
} BackendGroup;

void init_backend_groups();
Backend *get_backend_for_group(const char *group_name);
void increment_connections(Backend *b);
void decrement_connections(Backend *b);

#endif