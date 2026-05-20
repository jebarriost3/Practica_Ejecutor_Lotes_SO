#include "gesfich_service.h"

#include "gesfich_store.h"
#include "protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIELD_SIZE 512

static int write_response(char *response, size_t response_size, const char *format, ...)
{
    int written;
    va_list args;

    if (response == NULL || response_size == 0) {
        return 0;
    }

    va_start(args, format);
    written = vsnprintf(response, response_size, format, args);
    va_end(args);

    return written >= 0 && (size_t)written < response_size;
}

static int json_get_string(const char *json, const char *key, char *out, size_t out_size)
{
    char pattern[64];
    const char *key_pos;
    const char *colon;
    const char *start;
    const char *end;
    size_t len;

    if (json == NULL || key == NULL || out == NULL || out_size == 0) {
        return 0;
    }

    if (snprintf(pattern, sizeof(pattern), "\"%s\"", key) >= (int)sizeof(pattern)) {
        return 0;
    }

    key_pos = strstr(json, pattern);
    if (key_pos == NULL) {
        return 0;
    }

    colon = strchr(key_pos + strlen(pattern), ':');
    if (colon == NULL) {
        return 0;
    }

    start = strchr(colon + 1, '"');
    if (start == NULL) {
        return 0;
    }
    start++;

    end = start;
    while (*end != '\0') {
        if (*end == '"' && (end == start || *(end - 1) != '\\')) {
            break;
        }
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

static int append_text(char *buffer, size_t buffer_size, size_t *used, const char *text)
{
    int written;

    if (*used >= buffer_size) {
        return 0;
    }

    written = snprintf(buffer + *used, buffer_size - *used, "%s", text);
    if (written < 0 || (size_t)written >= buffer_size - *used) {
        return 0;
    }

    *used += (size_t)written;
    return 1;
}

static int append_json_escaped(char *buffer, size_t buffer_size, size_t *used,
                               const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;

    while (*cursor != '\0') {
        char escaped[8];

        switch (*cursor) {
        case '"':
            if (!append_text(buffer, buffer_size, used, "\\\"")) {
                return 0;
            }
            break;
        case '\\':
            if (!append_text(buffer, buffer_size, used, "\\\\")) {
                return 0;
            }
            break;
        case '\n':
            if (!append_text(buffer, buffer_size, used, "\\n")) {
                return 0;
            }
            break;
        case '\r':
            if (!append_text(buffer, buffer_size, used, "\\r")) {
                return 0;
            }
            break;
        case '\t':
            if (!append_text(buffer, buffer_size, used, "\\t")) {
                return 0;
            }
            break;
        default:
            if (*cursor < 32) {
                if (snprintf(escaped, sizeof(escaped), "\\u%04x", *cursor) >=
                    (int)sizeof(escaped)) {
                    return 0;
                }
                if (!append_text(buffer, buffer_size, used, escaped)) {
                    return 0;
                }
            } else {
                if (*used + 1 >= buffer_size) {
                    return 0;
                }
                buffer[*used] = (char)*cursor;
                (*used)++;
                buffer[*used] = '\0';
            }
            break;
        }
        cursor++;
    }

    return 1;
}

static int write_error(char *response, size_t response_size, const char *message)
{
    size_t used = 0;

    return append_text(response, response_size, &used, "{\"estado\":\"error\",\"mensaje\":\"") &&
           append_json_escaped(response, response_size, &used, message) &&
           append_text(response, response_size, &used, "\"}");
}

static int write_ok(char *response, size_t response_size)
{
    return write_response(response, response_size, "{\"estado\":\"ok\"}");
}

static const char *service_state_name(gesfich_service_state_t state)
{
    switch (state) {
    case GESFICH_SERVICE_SUSPENDIDO:
        return "suspendido";
    case GESFICH_SERVICE_TERMINADO:
        return "terminado";
    case GESFICH_SERVICE_CORRIENDO:
    default:
        return "corriendo";
    }
}

static int write_service_state(char *response, size_t response_size,
                               gesfich_service_state_t state)
{
    return write_response(response, response_size,
                          "{\"estado\":\"ok\",\"servicio\":\"gesfich\","
                          "\"estado-servicio\":\"%s\"}",
                          service_state_name(state));
}

static const char *store_error_message(gesfich_result_t result)
{
    switch (result) {
    case GESFICH_INVALID_ARG:
        return "faltan campos: id-fichero, ruta";
    case GESFICH_NOT_FOUND:
        return "fichero no encontrado";
    case GESFICH_STORAGE_ERROR:
        return "error al listar ficheros";
    case GESFICH_OK:
    default:
        return "operacion desconocida";
    }
}

static int handle_crear(const char *aralmac, char *response, size_t response_size)
{
    char id[GESFICH_ID_SIZE];
    gesfich_result_t result = gesfich_crear(aralmac, id);

    if (result != GESFICH_OK) {
        return write_error(response, response_size, "no se pudo crear el fichero");
    }

    return write_response(response, response_size, "{\"estado\":\"ok\",\"id-fichero\":\"%s\"}",
                          id);
}

static int handle_leer(const char *aralmac, const char *request, char *response,
                       size_t response_size)
{
    char id[FIELD_SIZE];
    char *contenido;
    size_t contenido_len;
    gesfich_result_t result;

    if (!json_get_string(request, "id-fichero", id, sizeof(id))) {
        char **ids;
        size_t count;
        size_t i;
        size_t used = 0;

        result = gesfich_listar(aralmac, &ids, &count);
        if (result != GESFICH_OK) {
            return write_error(response, response_size, "error al listar ficheros");
        }

        if (!append_text(response, response_size, &used, "{\"estado\":\"ok\",\"ficheros\":[")) {
            gesfich_liberar_lista(ids, count);
            return 0;
        }

        for (i = 0; i < count; i++) {
            if (i > 0 && !append_text(response, response_size, &used, ",")) {
                gesfich_liberar_lista(ids, count);
                return 0;
            }
            if (!append_text(response, response_size, &used, "\"") ||
                !append_json_escaped(response, response_size, &used, ids[i]) ||
                !append_text(response, response_size, &used, "\"")) {
                gesfich_liberar_lista(ids, count);
                return 0;
            }
        }

        gesfich_liberar_lista(ids, count);
        return append_text(response, response_size, &used, "]}");
    }

    result = gesfich_leer(aralmac, id, &contenido, &contenido_len);
    if (result != GESFICH_OK) {
        return write_error(response, response_size, "fichero no encontrado");
    }

    {
        size_t used = 0;
        int ok = append_text(response, response_size, &used, "{\"estado\":\"ok\",\"contenido\":\"") &&
                 append_json_escaped(response, response_size, &used, contenido) &&
                 append_text(response, response_size, &used, "\"}");
        (void)contenido_len;
        free(contenido);
        return ok;
    }
}

static int handle_actualizar(const char *aralmac, const char *request, char *response,
                             size_t response_size)
{
    char id[FIELD_SIZE];
    char ruta[FIELD_SIZE];
    gesfich_result_t result;

    if (!json_get_string(request, "id-fichero", id, sizeof(id)) ||
        !json_get_string(request, "ruta", ruta, sizeof(ruta))) {
        return write_error(response, response_size, "faltan campos: id-fichero, ruta");
    }

    result = gesfich_actualizar(aralmac, id, ruta);
    if (result != GESFICH_OK) {
        return write_error(response, response_size, store_error_message(result));
    }

    return write_ok(response, response_size);
}

static int handle_borrar(const char *aralmac, const char *request, char *response,
                         size_t response_size)
{
    char id[FIELD_SIZE];
    gesfich_result_t result;

    if (!json_get_string(request, "id-fichero", id, sizeof(id))) {
        return write_error(response, response_size, "fichero no encontrado");
    }

    result = gesfich_borrar(aralmac, id);
    if (result != GESFICH_OK) {
        return write_error(response, response_size,
                           result == GESFICH_NOT_FOUND ? "fichero no encontrado"
                                                       : "no se pudo actualizar el fichero");
    }

    return write_ok(response, response_size);
}

int gesfich_handle_json(const char *aralmac, const char *request, char *response,
                        size_t response_size)
{
    char servicio[FIELD_SIZE];
    char operacion[FIELD_SIZE];

    if (response == NULL || response_size == 0) {
        return 0;
    }
    response[0] = '\0';

    if (request == NULL || !protocol_message_size_ok(strlen(request))) {
        return write_error(response, response_size, "operacion desconocida");
    }

    if (!json_get_string(request, "servicio", servicio, sizeof(servicio)) ||
        strcmp(servicio, "gesfich") != 0) {
        return write_error(response, response_size, "operacion desconocida");
    }

    if (!json_get_string(request, "operacion", operacion, sizeof(operacion))) {
        return write_error(response, response_size, "operacion desconocida");
    }

    if (strcmp(operacion, "Crear") == 0) {
        return handle_crear(aralmac, response, response_size);
    }
    if (strcmp(operacion, "Leer") == 0) {
        return handle_leer(aralmac, request, response, response_size);
    }
    if (strcmp(operacion, "Actualizar") == 0) {
        return handle_actualizar(aralmac, request, response, response_size);
    }
    if (strcmp(operacion, "Borrar") == 0) {
        return handle_borrar(aralmac, request, response, response_size);
    }
    if (strcmp(operacion, "Suspender") == 0 || strcmp(operacion, "Reasumir") == 0 ||
        strcmp(operacion, "Terminar") == 0) {
        return write_ok(response, response_size);
    }

    return write_error(response, response_size, "operacion desconocida");
}

void gesfich_service_init(gesfich_service_t *service)
{
    if (service != NULL) {
        service->state = GESFICH_SERVICE_CORRIENDO;
    }
}

gesfich_service_state_t gesfich_service_state(const gesfich_service_t *service)
{
    if (service == NULL) {
        return GESFICH_SERVICE_TERMINADO;
    }
    return service->state;
}

int gesfich_service_handle_json(gesfich_service_t *service, const char *aralmac,
                                const char *request, char *response,
                                size_t response_size)
{
    char servicio[FIELD_SIZE];
    char operacion[FIELD_SIZE];

    if (service == NULL) {
        return write_error(response, response_size, "servicio no inicializado");
    }

    if (response == NULL || response_size == 0) {
        return 0;
    }
    response[0] = '\0';

    if (request == NULL || !protocol_message_size_ok(strlen(request))) {
        return write_error(response, response_size, "operacion desconocida");
    }

    if (!json_get_string(request, "servicio", servicio, sizeof(servicio)) ||
        strcmp(servicio, "gesfich") != 0 ||
        !json_get_string(request, "operacion", operacion, sizeof(operacion))) {
        return write_error(response, response_size, "operacion desconocida");
    }

    if (strcmp(operacion, "Suspender") == 0) {
        service->state = GESFICH_SERVICE_SUSPENDIDO;
        return write_service_state(response, response_size, service->state);
    }
    if (strcmp(operacion, "Reasumir") == 0) {
        service->state = GESFICH_SERVICE_CORRIENDO;
        return write_service_state(response, response_size, service->state);
    }
    if (strcmp(operacion, "Terminar") == 0) {
        service->state = GESFICH_SERVICE_TERMINADO;
        return write_service_state(response, response_size, service->state);
    }

    if (service->state == GESFICH_SERVICE_SUSPENDIDO) {
        return write_error(response, response_size, "servicio suspendido");
    }
    if (service->state == GESFICH_SERVICE_TERMINADO) {
        return write_error(response, response_size, "servicio terminado");
    }

    return gesfich_handle_json(aralmac, request, response, response_size);
}
