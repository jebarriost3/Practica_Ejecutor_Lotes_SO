#include "gesprog_service.h"

#include "gesprog_store.h"
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

static char *copy_range(const char *start, size_t len)
{
    char *copy = malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, start, len);
    copy[len] = '\0';
    return copy;
}

static void free_string_array(char **values, size_t count)
{
    size_t i;

    if (values == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        free(values[i]);
    }
    free(values);
}

static int json_get_string_array(const char *json, const char *key, char ***values_out,
                                 size_t *count_out)
{
    char pattern[64];
    const char *key_pos;
    const char *cursor;
    char **values = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (values_out == NULL || count_out == NULL) {
        return 0;
    }
    *values_out = NULL;
    *count_out = 0;

    if (snprintf(pattern, sizeof(pattern), "\"%s\"", key) >= (int)sizeof(pattern)) {
        return 0;
    }

    key_pos = strstr(json, pattern);
    if (key_pos == NULL) {
        return 1;
    }

    cursor = strchr(key_pos + strlen(pattern), ':');
    if (cursor == NULL) {
        return 0;
    }
    cursor++;
    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    if (*cursor != '[') {
        return 0;
    }
    cursor++;

    for (;;) {
        const char *start;
        const char *end;

        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
            cursor++;
        }
        if (*cursor == ']') {
            break;
        }
        if (*cursor != '"') {
            free_string_array(values, count);
            return 0;
        }
        start = ++cursor;
        end = start;
        while (*end != '\0') {
            if (*end == '"' && (end == start || *(end - 1) != '\\')) {
                break;
            }
            end++;
        }
        if (*end != '"') {
            free_string_array(values, count);
            return 0;
        }
        if (count == capacity) {
            size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
            char **new_values = realloc(values, new_capacity * sizeof(*new_values));
            if (new_values == NULL) {
                free_string_array(values, count);
                return 0;
            }
            values = new_values;
            capacity = new_capacity;
        }
        values[count] = copy_range(start, (size_t)(end - start));
        if (values[count] == NULL) {
            free_string_array(values, count);
            return 0;
        }
        count++;
        cursor = end + 1;
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
            cursor++;
        }
        if (*cursor == ',') {
            cursor++;
            continue;
        }
        if (*cursor == ']') {
            break;
        }
        free_string_array(values, count);
        return 0;
    }

    *values_out = values;
    *count_out = count;
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

static int append_json_string_array(char *buffer, size_t buffer_size, size_t *used,
                                    char **values, size_t count)
{
    size_t i;

    if (!append_text(buffer, buffer_size, used, "[")) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        if (i > 0 && !append_text(buffer, buffer_size, used, ",")) {
            return 0;
        }
        if (!append_text(buffer, buffer_size, used, "\"") ||
            !append_json_escaped(buffer, buffer_size, used, values[i]) ||
            !append_text(buffer, buffer_size, used, "\"")) {
            return 0;
        }
    }
    return append_text(buffer, buffer_size, used, "]");
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

static const char *store_error_message(gesprog_result_t result)
{
    switch (result) {
    case GESPROG_INVALID_ARG:
        return "faltan campos: id-programa, ruta";
    case GESPROG_NOT_FOUND:
        return "programa no encontrado";
    case GESPROG_STORAGE_ERROR:
        return "error al listar programas";
    case GESPROG_OK:
    default:
        return "operacion desconocida";
    }
}

static int handle_guardar(const char *aralmac, const char *request, char *response,
                          size_t response_size)
{
    char ejecutable[FIELD_SIZE];
    char id[GESPROG_ID_SIZE];
    char **args = NULL;
    char **env = NULL;
    size_t args_count = 0;
    size_t env_count = 0;
    gesprog_result_t result;

    if (!json_get_string(request, "ejecutable", ejecutable, sizeof(ejecutable))) {
        return write_error(response, response_size, "falta campo: ejecutable");
    }
    if (!json_get_string_array(request, "args", &args, &args_count) ||
        !json_get_string_array(request, "env", &env, &env_count)) {
        free_string_array(args, args_count);
        free_string_array(env, env_count);
        return write_error(response, response_size, "operacion desconocida");
    }

    result = gesprog_guardar(aralmac, ejecutable, (const char *const *)args, args_count,
                             (const char *const *)env, env_count, id);
    free_string_array(args, args_count);
    free_string_array(env, env_count);

    if (result != GESPROG_OK) {
        return write_error(response, response_size,
                           result == GESPROG_NOT_FOUND ? "no se pudo guardar el programa"
                                                       : "no se pudo guardar el programa");
    }

    return write_response(response, response_size,
                          "{\"estado\":\"ok\",\"id-programa\":\"%s\"}", id);
}

static int handle_leer_programa(const char *aralmac, const char *id, char *response,
                                size_t response_size)
{
    gesprog_program_t programa;
    gesprog_result_t result;
    size_t used = 0;
    int ok;

    result = gesprog_leer(aralmac, id, &programa);
    if (result != GESPROG_OK) {
        return write_error(response, response_size, "programa no encontrado");
    }

    ok = append_text(response, response_size, &used, "{\"estado\":\"ok\",\"programa\":{") &&
         append_text(response, response_size, &used, "\"id-programa\":\"") &&
         append_json_escaped(response, response_size, &used, programa.id_programa) &&
         append_text(response, response_size, &used, "\",\"nombre\":\"") &&
         append_json_escaped(response, response_size, &used, programa.nombre) &&
         append_text(response, response_size, &used, "\",\"args\":") &&
         append_json_string_array(response, response_size, &used, programa.args,
                                  programa.args_count) &&
         append_text(response, response_size, &used, ",\"env\":") &&
         append_json_string_array(response, response_size, &used, programa.env,
                                  programa.env_count) &&
         append_text(response, response_size, &used, "}}");

    gesprog_liberar_programa(&programa);
    return ok;
}

static int handle_leer(const char *aralmac, const char *request, char *response,
                       size_t response_size)
{
    char id[FIELD_SIZE];
    char **ids;
    size_t count;
    size_t used = 0;
    gesprog_result_t result;
    int ok;

    if (json_get_string(request, "id-programa", id, sizeof(id))) {
        return handle_leer_programa(aralmac, id, response, response_size);
    }

    result = gesprog_listar(aralmac, &ids, &count);
    if (result != GESPROG_OK) {
        return write_error(response, response_size, "error al listar programas");
    }

    ok = append_text(response, response_size, &used, "{\"estado\":\"ok\",\"programas\":") &&
         append_json_string_array(response, response_size, &used, ids, count) &&
         append_text(response, response_size, &used, "}");

    gesprog_liberar_lista(ids, count);
    return ok;
}

static int handle_actualizar(const char *aralmac, const char *request, char *response,
                             size_t response_size)
{
    char id[FIELD_SIZE];
    char ruta[FIELD_SIZE];
    gesprog_result_t result;

    if (!json_get_string(request, "id-programa", id, sizeof(id)) ||
        !json_get_string(request, "ruta", ruta, sizeof(ruta))) {
        return write_error(response, response_size, "faltan campos: id-programa, ruta");
    }

    result = gesprog_actualizar(aralmac, id, ruta);
    if (result != GESPROG_OK) {
        return write_error(response, response_size, store_error_message(result));
    }

    return write_ok(response, response_size);
}

static int handle_borrar(const char *aralmac, const char *request, char *response,
                         size_t response_size)
{
    char id[FIELD_SIZE];
    gesprog_result_t result;

    if (!json_get_string(request, "id-programa", id, sizeof(id))) {
        return write_error(response, response_size, "programa no encontrado");
    }

    result = gesprog_borrar(aralmac, id);
    if (result != GESPROG_OK) {
        return write_error(response, response_size, "programa no encontrado");
    }

    return write_ok(response, response_size);
}

int gesprog_handle_json(const char *aralmac, const char *request, char *response,
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
        strcmp(servicio, "gesprog") != 0 ||
        !json_get_string(request, "operacion", operacion, sizeof(operacion))) {
        return write_error(response, response_size, "operacion desconocida");
    }

    if (strcmp(operacion, "Guardar") == 0) {
        return handle_guardar(aralmac, request, response, response_size);
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

void gesprog_service_init(gesprog_service_t *service)
{
    if (service != NULL) {
        service->state = GESPROG_SERVICE_CORRIENDO;
    }
}

gesprog_service_state_t gesprog_service_state(const gesprog_service_t *service)
{
    if (service == NULL) {
        return GESPROG_SERVICE_TERMINADO;
    }
    return service->state;
}

int gesprog_service_handle_json(gesprog_service_t *service, const char *aralmac,
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
        strcmp(servicio, "gesprog") != 0 ||
        !json_get_string(request, "operacion", operacion, sizeof(operacion))) {
        return write_error(response, response_size, "operacion desconocida");
    }

    if (strcmp(operacion, "Suspender") == 0) {
        service->state = GESPROG_SERVICE_SUSPENDIDO;
        return write_ok(response, response_size);
    }
    if (strcmp(operacion, "Reasumir") == 0) {
        service->state = GESPROG_SERVICE_CORRIENDO;
        return write_ok(response, response_size);
    }
    if (strcmp(operacion, "Terminar") == 0) {
        service->state = GESPROG_SERVICE_TERMINADO;
        return write_ok(response, response_size);
    }

    if (service->state == GESPROG_SERVICE_SUSPENDIDO) {
        return write_error(response, response_size, "servicio suspendido");
    }
    if (service->state == GESPROG_SERVICE_TERMINADO) {
        return write_error(response, response_size, "servicio terminado");
    }

    return gesprog_handle_json(aralmac, request, response, response_size);
}

