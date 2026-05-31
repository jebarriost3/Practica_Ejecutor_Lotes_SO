#ifndef EJECUTOR_SERVICE_H
#define EJECUTOR_SERVICE_H

#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#endif

#define EJECUTOR_MAX_PROCESOS 64

typedef enum {
    EJECUTOR_SERVICE_CORRIENDO = 0,
    EJECUTOR_SERVICE_SUSPENDIDO,
    EJECUTOR_SERVICE_TERMINADO
} ejecutor_service_state_t;

typedef struct {
    char id_ejecucion[7];
    char stdin_id[7];
    char stdout_id[7];
    char stderr_id[7];
#ifdef _WIN32
    HANDLE process;
    HANDLE thread;
#else
    pid_t pid;
#endif
    int active;
} ejecutor_process_t;

typedef struct {
    ejecutor_service_state_t state;
    ejecutor_process_t processes[EJECUTOR_MAX_PROCESOS];
} ejecutor_service_t;

void ejecutor_service_init(ejecutor_service_t *service);
ejecutor_service_state_t ejecutor_service_state(const ejecutor_service_t *service);
int ejecutor_handle_json(const char *aralmac, const char *request, char *response,
                         size_t response_size);
int ejecutor_service_handle_json(ejecutor_service_t *service, const char *aralmac,
                                 const char *request, char *response,
                                 size_t response_size);

#endif
