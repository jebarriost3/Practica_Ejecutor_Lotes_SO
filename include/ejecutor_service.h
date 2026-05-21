#ifndef EJECUTOR_SERVICE_H
#define EJECUTOR_SERVICE_H

#include <stddef.h>

typedef enum {
    EJECUTOR_SERVICE_CORRIENDO = 0,
    EJECUTOR_SERVICE_SUSPENDIDO,
    EJECUTOR_SERVICE_TERMINADO
} ejecutor_service_state_t;

typedef struct {
    ejecutor_service_state_t state;
} ejecutor_service_t;

void ejecutor_service_init(ejecutor_service_t *service);
ejecutor_service_state_t ejecutor_service_state(const ejecutor_service_t *service);
int ejecutor_handle_json(const char *aralmac, const char *request, char *response,
                         size_t response_size);
int ejecutor_service_handle_json(ejecutor_service_t *service, const char *aralmac,
                                 const char *request, char *response,
                                 size_t response_size);

#endif
