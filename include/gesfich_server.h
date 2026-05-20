#ifndef GESFICH_SERVER_H
#define GESFICH_SERVER_H

typedef struct {
    const char *request_pipe;
    const char *response_pipe;
    const char *aralmac;
} gesfich_server_config_t;

int gesfich_server_run(const gesfich_server_config_t *config);

#endif
