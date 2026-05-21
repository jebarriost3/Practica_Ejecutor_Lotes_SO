#ifndef EJECUTOR_SERVER_H
#define EJECUTOR_SERVER_H

typedef struct {
    const char *request_pipe;
    const char *response_pipe;
    const char *aralmac;
} ejecutor_server_config_t;

int ejecutor_server_run(const ejecutor_server_config_t *config);

#endif
