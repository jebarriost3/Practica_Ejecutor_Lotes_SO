#ifndef GESPROG_SERVICE_H
#define GESPROG_SERVICE_H

#include <stddef.h>

typedef enum {
    GESPROG_SERVICE_CORRIENDO = 0,
    GESPROG_SERVICE_SUSPENDIDO,
    GESPROG_SERVICE_TERMINADO
} gesprog_service_state_t;

typedef struct {
    gesprog_service_state_t state;
} gesprog_service_t;

void gesprog_service_init(gesprog_service_t *service);
gesprog_service_state_t gesprog_service_state(const gesprog_service_t *service);
int gesprog_handle_json(const char *aralmac, const char *request, char *response,
                        size_t response_size);
int gesprog_service_handle_json(gesprog_service_t *service, const char *aralmac,
                                const char *request, char *response,
                                size_t response_size);

#endif

