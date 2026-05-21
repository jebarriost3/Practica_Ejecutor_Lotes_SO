#include "ejecutor_store.h"

#include "protocol.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#define MKDIR(path) mkdir(path, 0777)
#endif

#define EJECUCIONES_DIR "ejecuciones"
#define META_EXT ".meta"

static int make_dir_if_needed(const char *path)
{
    if (MKDIR(path) == 0) {
        return 1;
    }
    return errno == EEXIST;
}

static int build_path(char *buffer, size_t buffer_size, const char *a, const char *b,
                      const char *c)
{
    int written;

    if (c == NULL) {
        written = snprintf(buffer, buffer_size, "%s/%s", a, b);
    } else {
        written = snprintf(buffer, buffer_size, "%s/%s/%s", a, b, c);
    }

    return written > 0 && (size_t)written < buffer_size;
}

static ejecutor_result_t ensure_layout(const char *aralmac)
{
    char path[512];

    if (aralmac == NULL || aralmac[0] == '\0') {
        return EJECUTOR_INVALID_ARG;
    }

    if (!make_dir_if_needed(aralmac)) {
        return EJECUTOR_STORAGE_ERROR;
    }

    if (!build_path(path, sizeof(path), aralmac, EJECUCIONES_DIR, NULL)) {
        return EJECUTOR_STORAGE_ERROR;
    }

    if (!make_dir_if_needed(path)) {
        return EJECUTOR_STORAGE_ERROR;
    }

    return EJECUTOR_OK;
}

const char *ejecutor_estado_texto(ejecutor_process_state_t estado)
{
    switch (estado) {
    case EJECUTOR_PROCESO_EJECUTANDO:
        return "Ejecutando";
    case EJECUTOR_PROCESO_SUSPENDIDO:
        return "Suspendido";
    case EJECUTOR_PROCESO_TERMINADO:
        return "Terminado";
    default:
        return "Desconocido";
    }
}

static int texto_estado(const char *texto, ejecutor_process_state_t *estado_out)
{
    if (strcmp(texto, "Ejecutando") == 0) {
        *estado_out = EJECUTOR_PROCESO_EJECUTANDO;
        return 1;
    }
    if (strcmp(texto, "Suspendido") == 0) {
        *estado_out = EJECUTOR_PROCESO_SUSPENDIDO;
        return 1;
    }
    if (strcmp(texto, "Terminado") == 0) {
        *estado_out = EJECUTOR_PROCESO_TERMINADO;
        return 1;
    }
    return 0;
}

static int id_filename(char *buffer, size_t buffer_size, const char *id_ejecucion)
{
    int written = snprintf(buffer, buffer_size, "%s%s", id_ejecucion, META_EXT);
    return written > 0 && (size_t)written < buffer_size;
}

static ejecutor_result_t ejecucion_path(char *buffer, size_t buffer_size,
                                        const char *aralmac,
                                        const char *id_ejecucion)
{
    char filename[32];

    if (!protocol_valid_id(id_ejecucion, ID_EJECUCION)) {
        return EJECUTOR_INVALID_ARG;
    }

    if (!id_filename(filename, sizeof(filename), id_ejecucion)) {
        return EJECUTOR_STORAGE_ERROR;
    }

    if (!build_path(buffer, buffer_size, aralmac, EJECUCIONES_DIR, filename)) {
        return EJECUTOR_STORAGE_ERROR;
    }

    return EJECUTOR_OK;
}

static int filename_to_number(const char *name)
{
    char id[EJECUTOR_ID_SIZE];

    if (strlen(name) != 11 || strcmp(name + 6, META_EXT) != 0) {
        return 0;
    }

    memcpy(id, name, 6);
    id[6] = '\0';

    if (!protocol_valid_id(id, ID_EJECUCION)) {
        return 0;
    }

    return (id[2] - '0') * 1000 + (id[3] - '0') * 100 + (id[4] - '0') * 10 +
           (id[5] - '0');
}

static ejecutor_result_t next_id(const char *aralmac, char id_out[EJECUTOR_ID_SIZE])
{
    char dir_path[512];
    DIR *dir;
    struct dirent *entry;
    int max_id = 0;

    if (!build_path(dir_path, sizeof(dir_path), aralmac, EJECUCIONES_DIR, NULL)) {
        return EJECUTOR_STORAGE_ERROR;
    }

    dir = opendir(dir_path);
    if (dir == NULL) {
        return EJECUTOR_STORAGE_ERROR;
    }

    while ((entry = readdir(dir)) != NULL) {
        int number = filename_to_number(entry->d_name);
        if (number > max_id) {
            max_id = number;
        }
    }

    closedir(dir);

    if (max_id >= 9999 ||
        snprintf(id_out, EJECUTOR_ID_SIZE, "e-%04d", max_id + 1) >= EJECUTOR_ID_SIZE) {
        return EJECUTOR_STORAGE_ERROR;
    }

    return EJECUTOR_OK;
}

static ejecutor_result_t write_metadata(const char *meta_path,
                                        const ejecutor_execution_t *ejecucion)
{
    FILE *file = fopen(meta_path, "wb");
    if (file == NULL) {
        return EJECUTOR_STORAGE_ERROR;
    }

    fprintf(file, "id-ejecucion=%s\n", ejecucion->id_ejecucion);
    fprintf(file, "id-programa=%s\n", ejecucion->id_programa);
    fprintf(file, "proceso-estado=%s\n", ejecutor_estado_texto(ejecucion->estado));
    if (ejecucion->tiene_codigo_salida) {
        fprintf(file, "codigo-salida=%d\n", ejecucion->codigo_salida);
    } else {
        fprintf(file, "codigo-salida=\n");
    }

    if (fclose(file) != 0) {
        return EJECUTOR_STORAGE_ERROR;
    }

    return EJECUTOR_OK;
}

static int read_line(FILE *file, char *buffer, size_t buffer_size)
{
    size_t len;

    if (fgets(buffer, (int)buffer_size, file) == NULL) {
        return 0;
    }

    len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[--len] = '\0';
    }
    if (len > 0 && buffer[len - 1] == '\r') {
        buffer[--len] = '\0';
    }

    return 1;
}

static int read_prefixed(FILE *file, const char *prefix, char *buffer, size_t buffer_size)
{
    char line[1024];
    size_t prefix_len = strlen(prefix);

    if (!read_line(file, line, sizeof(line)) || strncmp(line, prefix, prefix_len) != 0) {
        return 0;
    }

    if (strlen(line + prefix_len) >= buffer_size) {
        return 0;
    }

    strcpy(buffer, line + prefix_len);
    return 1;
}

static ejecutor_result_t actualizar_estado(const char *aralmac, const char *id_ejecucion,
                                           ejecutor_process_state_t estado,
                                           int tiene_codigo_salida,
                                           int codigo_salida)
{
    char meta_path[512];
    ejecutor_execution_t ejecucion;
    ejecutor_result_t result;

    result = ejecutor_leer(aralmac, id_ejecucion, &ejecucion);
    if (result != EJECUTOR_OK) {
        return result;
    }

    ejecucion.estado = estado;
    ejecucion.tiene_codigo_salida = tiene_codigo_salida;
    ejecucion.codigo_salida = codigo_salida;

    result = ejecucion_path(meta_path, sizeof(meta_path), aralmac, id_ejecucion);
    if (result != EJECUTOR_OK) {
        return result;
    }

    return write_metadata(meta_path, &ejecucion);
}

ejecutor_result_t ejecutor_registrar(const char *aralmac, const char *id_programa,
                                     char id_out[EJECUTOR_ID_SIZE])
{
    char meta_path[512];
    ejecutor_execution_t ejecucion;
    ejecutor_result_t result;

    if (id_out == NULL || !protocol_valid_id(id_programa, ID_PROGRAMA)) {
        return EJECUTOR_INVALID_ARG;
    }

    result = ensure_layout(aralmac);
    if (result != EJECUTOR_OK) {
        return result;
    }

    result = next_id(aralmac, id_out);
    if (result != EJECUTOR_OK) {
        return result;
    }

    result = ejecucion_path(meta_path, sizeof(meta_path), aralmac, id_out);
    if (result != EJECUTOR_OK) {
        return result;
    }

    memset(&ejecucion, 0, sizeof(ejecucion));
    strcpy(ejecucion.id_ejecucion, id_out);
    strcpy(ejecucion.id_programa, id_programa);
    ejecucion.estado = EJECUTOR_PROCESO_EJECUTANDO;

    return write_metadata(meta_path, &ejecucion);
}

ejecutor_result_t ejecutor_leer(const char *aralmac, const char *id_ejecucion,
                                ejecutor_execution_t *ejecucion_out)
{
    char meta_path[512];
    char value[1024];
    FILE *file;
    ejecutor_result_t result;

    if (ejecucion_out == NULL) {
        return EJECUTOR_INVALID_ARG;
    }

    memset(ejecucion_out, 0, sizeof(*ejecucion_out));

    result = ensure_layout(aralmac);
    if (result != EJECUTOR_OK) {
        return result;
    }

    result = ejecucion_path(meta_path, sizeof(meta_path), aralmac, id_ejecucion);
    if (result != EJECUTOR_OK) {
        return result;
    }

    file = fopen(meta_path, "rb");
    if (file == NULL) {
        return EJECUTOR_NOT_FOUND;
    }

    if (!read_prefixed(file, "id-ejecucion=", value, sizeof(value)) ||
        !protocol_valid_id(value, ID_EJECUCION)) {
        fclose(file);
        return EJECUTOR_STORAGE_ERROR;
    }
    strcpy(ejecucion_out->id_ejecucion, value);

    if (!read_prefixed(file, "id-programa=", value, sizeof(value)) ||
        !protocol_valid_id(value, ID_PROGRAMA)) {
        fclose(file);
        return EJECUTOR_STORAGE_ERROR;
    }
    strcpy(ejecucion_out->id_programa, value);

    if (!read_prefixed(file, "proceso-estado=", value, sizeof(value)) ||
        !texto_estado(value, &ejecucion_out->estado)) {
        fclose(file);
        return EJECUTOR_STORAGE_ERROR;
    }

    if (!read_prefixed(file, "codigo-salida=", value, sizeof(value))) {
        fclose(file);
        return EJECUTOR_STORAGE_ERROR;
    }

    if (value[0] != '\0') {
        char *end;
        long code = strtol(value, &end, 10);
        if (*end != '\0') {
            fclose(file);
            return EJECUTOR_STORAGE_ERROR;
        }
        ejecucion_out->codigo_salida = (int)code;
        ejecucion_out->tiene_codigo_salida = 1;
    }

    fclose(file);
    return EJECUTOR_OK;
}

ejecutor_result_t ejecutor_listar(const char *aralmac, ejecutor_execution_t **items_out,
                                  size_t *count_out)
{
    char dir_path[512];
    DIR *dir;
    struct dirent *entry;
    ejecutor_execution_t *items = NULL;
    size_t count = 0;
    size_t capacity = 0;
    ejecutor_result_t result;

    if (items_out == NULL || count_out == NULL) {
        return EJECUTOR_INVALID_ARG;
    }

    *items_out = NULL;
    *count_out = 0;

    result = ensure_layout(aralmac);
    if (result != EJECUTOR_OK) {
        return result;
    }

    if (!build_path(dir_path, sizeof(dir_path), aralmac, EJECUCIONES_DIR, NULL)) {
        return EJECUTOR_STORAGE_ERROR;
    }

    dir = opendir(dir_path);
    if (dir == NULL) {
        return EJECUTOR_STORAGE_ERROR;
    }

    while ((entry = readdir(dir)) != NULL) {
        char id[EJECUTOR_ID_SIZE];
        if (filename_to_number(entry->d_name) == 0) {
            continue;
        }

        if (count == capacity) {
            size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
            ejecutor_execution_t *new_items =
                realloc(items, new_capacity * sizeof(*new_items));
            if (new_items == NULL) {
                free(items);
                closedir(dir);
                return EJECUTOR_STORAGE_ERROR;
            }
            items = new_items;
            capacity = new_capacity;
        }

        memcpy(id, entry->d_name, 6);
        id[6] = '\0';
        result = ejecutor_leer(aralmac, id, &items[count]);
        if (result != EJECUTOR_OK) {
            free(items);
            closedir(dir);
            return result;
        }
        count++;
    }

    closedir(dir);
    *items_out = items;
    *count_out = count;
    return EJECUTOR_OK;
}

ejecutor_result_t ejecutor_marcar_ejecutando(const char *aralmac,
                                             const char *id_ejecucion)
{
    return actualizar_estado(aralmac, id_ejecucion, EJECUTOR_PROCESO_EJECUTANDO, 0, 0);
}

ejecutor_result_t ejecutor_marcar_suspendida(const char *aralmac,
                                             const char *id_ejecucion)
{
    return actualizar_estado(aralmac, id_ejecucion, EJECUTOR_PROCESO_SUSPENDIDO, 0, 0);
}

ejecutor_result_t ejecutor_marcar_terminada(const char *aralmac,
                                            const char *id_ejecucion,
                                            int codigo_salida)
{
    return actualizar_estado(aralmac, id_ejecucion, EJECUTOR_PROCESO_TERMINADO, 1,
                             codigo_salida);
}

void ejecutor_liberar_lista(ejecutor_execution_t *items)
{
    free(items);
}
