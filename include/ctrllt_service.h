#ifndef CTRLLT_SERVICE_H
#define CTRLLT_SERVICE_H

#include <stddef.h>

typedef enum {
    CTRLLT_SERVICE_CORRIENDO = 0,
    CTRLLT_SERVICE_TERMINADO
} ctrllt_service_state_t;

typedef enum {
    CTRLLT_TARGET_GESFICH = 0,
    CTRLLT_TARGET_GESPROG,
    CTRLLT_TARGET_EJECUTOR
} ctrllt_target_t;

typedef int (*ctrllt_send_fn)(ctrllt_target_t target, const char *request,
                              char *response, size_t response_size,
                              void *user_data);

typedef struct {
    ctrllt_service_state_t state;
    ctrllt_send_fn send;
    void *user_data;
} ctrllt_service_t;

void ctrllt_service_init(ctrllt_service_t *service, ctrllt_send_fn send,
                         void *user_data);
ctrllt_service_state_t ctrllt_service_state(const ctrllt_service_t *service);
int ctrllt_service_handle_json(ctrllt_service_t *service, const char *request,
                               char *response, size_t response_size);
const char *ctrllt_target_name(ctrllt_target_t target);

#endif
