#ifndef GESFICH_SERVICE_H
#define GESFICH_SERVICE_H

#include <stddef.h>

typedef enum {
    GESFICH_SERVICE_CORRIENDO = 0,
    GESFICH_SERVICE_SUSPENDIDO,
    GESFICH_SERVICE_TERMINADO
} gesfich_service_state_t;

typedef struct {
    gesfich_service_state_t state;
} gesfich_service_t;

void gesfich_service_init(gesfich_service_t *service);
gesfich_service_state_t gesfich_service_state(const gesfich_service_t *service);
int gesfich_handle_json(const char *aralmac, const char *request, char *response,
                        size_t response_size);
int gesfich_service_handle_json(gesfich_service_t *service, const char *aralmac,
                                const char *request, char *response,
                                size_t response_size);

#endif
