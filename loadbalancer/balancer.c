#include "balancer.h"
#include <string.h>

static BackendGroup backend_groups[MAX_GROUPS];
static int group_count = 0;

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
            if(group->strategy == ROUND_ROBIN) {
                Backend *b = &group->backends[group->rr_index];
                group->rr_index = (group->rr_index + 1) % group->count;
                return b;
            } else if(group->strategy == LEAST_CONNECTIONS) {
                Backend *least = &group->backends[0];
                for (int j = 1; j < group->count; j++) {
                    if (group->backends[j].active_connections < least->active_connections) {
                        least = &group->backends[j];
                    }
                }
                return least;
            }
        }
    }
    return NULL;
}

void increment_connections(Backend *b) {
    b->active_connections++;
}

void decrement_connections(Backend *b) {
    if (b->active_connections > 0) {
        b->active_connections--;
    }
}