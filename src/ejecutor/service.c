#include "ejecutor_service.h"

#include "ejecutor_store.h"
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

static int write_error(char *response, size_t size, const char *message)
{
    return write_json(response, size, "{\"estado\":\"error\",\"mensaje\":\"%s\"}", message);
}

static const char *error_message(ejecutor_result_t result)
{
    if (result == EJECUTOR_NOT_FOUND) {
        return "ejecucion no encontrada";
    }
    if (result == EJECUTOR_INVALID_ARG) {
        return "identificador invalido";
    }
    if (result == EJECUTOR_STORAGE_ERROR) {
        return "error de almacenamiento";
    }
    return "operacion desconocida";
}

static int append_execution(char *response, size_t size, size_t *used,
                            const ejecutor_execution_t *item)
{
    int written = snprintf(response + *used, size - *used,
                           "{\"id-ejecucion\":\"%s\",\"id-programa\":\"%s\","
                           "\"proceso-estado\":\"%s\"",
                           item->id_ejecucion, item->id_programa,
                           ejecutor_estado_texto(item->estado));

    if (written < 0 || (size_t)written >= size - *used) {
        return 0;
    }
    *used += (size_t)written;

    if (item->tiene_codigo_salida) {
        written = snprintf(response + *used, size - *used, ",\"codigo-salida\":%d",
                           item->codigo_salida);
        if (written < 0 || (size_t)written >= size - *used) {
            return 0;
        }
        *used += (size_t)written;
    }

    written = snprintf(response + *used, size - *used, "}");
    if (written < 0 || (size_t)written >= size - *used) {
        return 0;
    }
    *used += (size_t)written;
    return 1;
}

static int handle_ejecutar(const char *aralmac, const char *request, char *response,
                           size_t size)
{
    char id_programa[FIELD_SIZE];
    char id_ejecucion[EJECUTOR_ID_SIZE];
    ejecutor_result_t result;

    if (!json_get_string(request, "id-programa", id_programa, sizeof(id_programa))) {
        return write_error(response, size, "falta campo: id-programa");
    }

    result = ejecutor_registrar(aralmac, id_programa, id_ejecucion);
    if (result != EJECUTOR_OK) {
        return write_error(response, size, error_message(result));
    }

    return write_json(response, size, "{\"estado\":\"ok\",\"id-ejecucion\":\"%s\"}",
                      id_ejecucion);
}

static int handle_estado(const char *aralmac, const char *request, char *response,
                         size_t size)
{
    char id[FIELD_SIZE];
    ejecutor_execution_t item;
    ejecutor_execution_t *items = NULL;
    size_t count = 0;
    size_t used;
    size_t i;
    ejecutor_result_t result;

    if (json_get_string(request, "id-ejecucion", id, sizeof(id))) {
        result = ejecutor_leer(aralmac, id, &item);
        if (result != EJECUTOR_OK) {
            return write_error(response, size, error_message(result));
        }
        used = (size_t)snprintf(response, size, "{\"estado\":\"ok\",\"ejecucion\":");
        return used < size && append_execution(response, size, &used, &item) &&
               write_json(response + used, size - used, "}");
    }

    result = ejecutor_listar(aralmac, &items, &count);
    if (result != EJECUTOR_OK) {
        return write_error(response, size, "error al listar ejecuciones");
    }

    used = (size_t)snprintf(response, size, "{\"estado\":\"ok\",\"ejecuciones\":[");
    if (used >= size) {
        ejecutor_liberar_lista(items);
        return 0;
    }

    for (i = 0; i < count; i++) {
        if (i > 0 && !write_json(response + used, size - used, ",")) {
            ejecutor_liberar_lista(items);
            return 0;
        }
        if (i > 0) {
            used++;
        }
        if (!append_execution(response, size, &used, &items[i])) {
            ejecutor_liberar_lista(items);
            return 0;
        }
    }

    ejecutor_liberar_lista(items);
    return write_json(response + used, size - used, "]}");
}

int ejecutor_handle_json(const char *aralmac, const char *request, char *response,
                         size_t response_size)
{
    char servicio[FIELD_SIZE];
    char operacion[FIELD_SIZE];

    if (response == NULL || response_size == 0) {
        return 0;
    }
    response[0] = '\0';

    if (request == NULL || !protocol_message_size_ok(strlen(request)) ||
        !json_get_string(request, "servicio", servicio, sizeof(servicio)) ||
        strcmp(servicio, "ejecutor") != 0 ||
        !json_get_string(request, "operacion", operacion, sizeof(operacion))) {
        return write_error(response, response_size, "operacion desconocida");
    }

    if (strcmp(operacion, "Ejecutar") == 0) {
        return handle_ejecutar(aralmac, request, response, response_size);
    }
    if (strcmp(operacion, "Estado") == 0) {
        return handle_estado(aralmac, request, response, response_size);
    }
    if (strcmp(operacion, "Suspender") == 0 || strcmp(operacion, "Reasumir") == 0 ||
        strcmp(operacion, "Parar") == 0) {
        return write_json(response, response_size, "{\"estado\":\"ok\"}");
    }

    return write_error(response, response_size, "operacion desconocida");
}

void ejecutor_service_init(ejecutor_service_t *service)
{
    if (service != NULL) {
        service->state = EJECUTOR_SERVICE_CORRIENDO;
    }
}

ejecutor_service_state_t ejecutor_service_state(const ejecutor_service_t *service)
{
    return service == NULL ? EJECUTOR_SERVICE_TERMINADO : service->state;
}

int ejecutor_service_handle_json(ejecutor_service_t *service, const char *aralmac,
                                 const char *request, char *response,
                                 size_t response_size)
{
    char operacion[FIELD_SIZE];

    if (service == NULL) {
        return write_error(response, response_size, "servicio no inicializado");
    }
    if (!json_get_string(request, "operacion", operacion, sizeof(operacion))) {
        return ejecutor_handle_json(aralmac, request, response, response_size);
    }

    if (strcmp(operacion, "Suspender") == 0) {
        service->state = EJECUTOR_SERVICE_SUSPENDIDO;
        return write_json(response, response_size, "{\"estado\":\"ok\"}");
    }
    if (strcmp(operacion, "Reasumir") == 0) {
        service->state = EJECUTOR_SERVICE_CORRIENDO;
        return write_json(response, response_size, "{\"estado\":\"ok\"}");
    }
    if (strcmp(operacion, "Parar") == 0) {
        service->state = EJECUTOR_SERVICE_TERMINADO;
        return write_json(response, response_size, "{\"estado\":\"ok\"}");
    }
    if (service->state == EJECUTOR_SERVICE_TERMINADO) {
        return write_error(response, response_size, "servicio terminado");
    }
    if (service->state == EJECUTOR_SERVICE_SUSPENDIDO &&
        strcmp(operacion, "Ejecutar") == 0) {
        return write_error(response, response_size, "servicio suspendido");
    }

    return ejecutor_handle_json(aralmac, request, response, response_size);
}
