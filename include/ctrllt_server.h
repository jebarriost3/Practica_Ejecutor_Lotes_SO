#ifndef CTRLLT_SERVER_H
#define CTRLLT_SERVER_H

typedef struct {
    const char *client_request_pipe;
    const char *client_response_pipe;
    const char *gesfich_request_pipe;
    const char *gesfich_response_pipe;
    const char *gesprog_request_pipe;
    const char *gesprog_response_pipe;
    const char *ejecutor_request_pipe;
    const char *ejecutor_response_pipe;
} ctrllt_server_config_t;

int ctrllt_server_run(const ctrllt_server_config_t *config);

#endif
