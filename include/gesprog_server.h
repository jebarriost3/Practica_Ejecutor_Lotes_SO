#ifndef GESPROG_SERVER_H
#define GESPROG_SERVER_H

typedef struct {
    const char *request_pipe;
    const char *response_pipe;
    const char *aralmac;
} gesprog_server_config_t;

int gesprog_server_run(const gesprog_server_config_t *config);

#endif

