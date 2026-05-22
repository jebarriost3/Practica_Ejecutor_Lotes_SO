#include "ctrllt_service.h"

#include "protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define FIELD_SIZE 512

static int write_json(char *response, size_t size, const char *format, ...)
{
    int written;
    va_list args;

    if (response == NULL || size == 0) {
        return 0;
    }

    va_start(args, format);
    written = vsnprintf(response, size, format, args);
    va_end(args);

    return written >= 0 && (size_t)written < size;
}

static int write_error(char *response, size_t size, const char *message)
{
    return write_json(response, size, "{\"estado\":\"error\",\"mensaje\":\"%s\"}", message);
}

static int json_get_string(const char *json, const char *key, char *out, size_t out_size)
{
    char pattern[64];
    const char *p;
    const char *start;
    const char *end;
    size_t len;

    if (json == NULL || key == NULL || out == NULL || out_size == 0) {
        return 0;
    }
    if (snprintf(pattern, sizeof(pattern), "\"%s\"", key) >= (int)sizeof(pattern)) {
        return 0;
    }

    p = strstr(json, pattern);
    p = p == NULL ? NULL : strchr(p + strlen(pattern), ':');
    start = p == NULL ? NULL : strchr(p + 1, '"');
    if (start == NULL) {
        return 0;
    }
    start++;

    end = start;
    while (*end != '\0' && !(*end == '"' && (end == start || *(end - 1) != '\\'))) {
        end++;
    }
    if (*end != '"') {
        return 0;
    }

    len = (size_t)(end - start);
    if (len >= out_size) {
        return 0;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    return 1;
}

static int service_target(const char *name, ctrllt_target_t *target_out)
{
    if (strcmp(name, "gesfich") == 0) {
        *target_out = CTRLLT_TARGET_GESFICH;
        return 1;
    }
    if (strcmp(name, "gesprog") == 0) {
        *target_out = CTRLLT_TARGET_GESPROG;
        return 1;
    }
    if (strcmp(name, "ejecutor") == 0) {
        *target_out = CTRLLT_TARGET_EJECUTOR;
        return 1;
    }
    return 0;
}

const char *ctrllt_target_name(ctrllt_target_t target)
{
    switch (target) {
    case CTRLLT_TARGET_GESFICH:
        return "gesfich";
    case CTRLLT_TARGET_GESPROG:
        return "gesprog";
    case CTRLLT_TARGET_EJECUTOR:
        return "ejecutor";
    default:
        return "desconocido";
    }
}

void ctrllt_service_init(ctrllt_service_t *service, ctrllt_send_fn send,
                         void *user_data)
{
    if (service == NULL) {
        return;
    }
    service->state = CTRLLT_SERVICE_CORRIENDO;
    service->send = send;
    service->user_data = user_data;
}

ctrllt_service_state_t ctrllt_service_state(const ctrllt_service_t *service)
{
    return service == NULL ? CTRLLT_SERVICE_TERMINADO : service->state;
}

static int forward_request(ctrllt_service_t *service, ctrllt_target_t target,
                           const char *request, char *response,
                           size_t response_size)
{
    if (service->send == NULL) {
        return write_error(response, response_size, "servicio no conectado");
    }
    if (!service->send(target, request, response, response_size, service->user_data)) {
        return write_error(response, response_size,
                           "error enviando solicitud al servicio");
    }
    return 1;
}

static int send_shutdown(ctrllt_service_t *service, ctrllt_target_t target,
                         const char *request, char *response,
                         size_t response_size)
{
    char service_response[MSG_MAX_LEN + 1];

    if (service->send == NULL) {
        return write_error(response, response_size, "servicio no conectado");
    }
    if (!service->send(target, request, service_response, sizeof(service_response),
                       service->user_data)) {
        return write_error(response, response_size,
                           "error enviando solicitud al servicio");
    }
    return 1;
}

static int handle_terminar(ctrllt_service_t *service, char *response,
                           size_t response_size)
{
    if (!send_shutdown(service, CTRLLT_TARGET_GESFICH,
                       "{\"servicio\":\"gesfich\",\"operacion\":\"Terminar\"}",
                       response, response_size) ||
        !send_shutdown(service, CTRLLT_TARGET_GESPROG,
                       "{\"servicio\":\"gesprog\",\"operacion\":\"Terminar\"}",
                       response, response_size) ||
        !send_shutdown(service, CTRLLT_TARGET_EJECUTOR,
                       "{\"servicio\":\"ejecutor\",\"operacion\":\"Parar\"}",
                       response, response_size)) {
        return 1;
    }

    service->state = CTRLLT_SERVICE_TERMINADO;
    return write_json(response, response_size, "{\"estado\":\"ok\"}");
}

int ctrllt_service_handle_json(ctrllt_service_t *service, const char *request,
                               char *response, size_t response_size)
{
    char servicio[FIELD_SIZE];
    char operacion[FIELD_SIZE];
    ctrllt_target_t target;

    if (service == NULL) {
        return write_error(response, response_size, "servicio no conectado");
    }
    if (response == NULL || response_size == 0) {
        return 0;
    }
    response[0] = '\0';

    if (request == NULL || !protocol_message_size_ok(strlen(request)) ||
        !json_get_string(request, "servicio", servicio, sizeof(servicio))) {
        return write_error(response, response_size, "servicio desconocido");
    }

    if (strcmp(servicio, "ctrllt") == 0) {
        if (!json_get_string(request, "operacion", operacion, sizeof(operacion)) ||
            strcmp(operacion, "Terminar") != 0) {
            return write_error(response, response_size, "operacion ctrllt desconocida");
        }
        return handle_terminar(service, response, response_size);
    }

    if (!service_target(servicio, &target)) {
        return write_error(response, response_size, "servicio desconocido");
    }

    return forward_request(service, target, request, response, response_size);
}
