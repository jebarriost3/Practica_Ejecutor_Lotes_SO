#include "gesfich_store.h"

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

#define FICHEROS_DIR "ficheros"
#define FICHERO_EXT ".dat"

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

static gesfich_result_t ensure_layout(const char *aralmac)
{
    char path[512];

    if (aralmac == NULL || aralmac[0] == '\0') {
        return GESFICH_INVALID_ARG;
    }

    if (!make_dir_if_needed(aralmac)) {
        return GESFICH_STORAGE_ERROR;
    }

    if (!build_path(path, sizeof(path), aralmac, FICHEROS_DIR, NULL)) {
        return GESFICH_STORAGE_ERROR;
    }

    if (!make_dir_if_needed(path)) {
        return GESFICH_STORAGE_ERROR;
    }

    return GESFICH_OK;
}

static int id_filename(char *buffer, size_t buffer_size, const char *id_fichero)
{
    int written = snprintf(buffer, buffer_size, "%s%s", id_fichero, FICHERO_EXT);
    return written > 0 && (size_t)written < buffer_size;
}

static gesfich_result_t fichero_path(char *buffer, size_t buffer_size, const char *aralmac,
                                     const char *id_fichero)
{
    char filename[32];

    if (!protocol_valid_id(id_fichero, ID_FICHERO)) {
        return GESFICH_INVALID_ARG;
    }

    if (!id_filename(filename, sizeof(filename), id_fichero)) {
        return GESFICH_STORAGE_ERROR;
    }

    if (!build_path(buffer, buffer_size, aralmac, FICHEROS_DIR, filename)) {
        return GESFICH_STORAGE_ERROR;
    }

    return GESFICH_OK;
}

static int file_exists(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    fclose(file);
    return 1;
}

static int filename_to_number(const char *name)
{
    char id[GESFICH_ID_SIZE];

    if (strlen(name) != 10 || strcmp(name + 6, FICHERO_EXT) != 0) {
        return 0;
    }

    memcpy(id, name, 6);
    id[6] = '\0';

    if (!protocol_valid_id(id, ID_FICHERO)) {
        return 0;
    }

    return (id[2] - '0') * 1000 + (id[3] - '0') * 100 + (id[4] - '0') * 10 +
           (id[5] - '0');
}

static gesfich_result_t next_id(const char *aralmac, char id_out[GESFICH_ID_SIZE])
{
    char dir_path[512];
    DIR *dir;
    struct dirent *entry;
    int max_id = 0;

    if (!build_path(dir_path, sizeof(dir_path), aralmac, FICHEROS_DIR, NULL)) {
        return GESFICH_STORAGE_ERROR;
    }

    dir = opendir(dir_path);
    if (dir == NULL) {
        return GESFICH_STORAGE_ERROR;
    }

    while ((entry = readdir(dir)) != NULL) {
        int number = filename_to_number(entry->d_name);
        if (number > max_id) {
            max_id = number;
        }
    }

    closedir(dir);

    if (max_id >= 9999) {
        return GESFICH_STORAGE_ERROR;
    }

    if (snprintf(id_out, GESFICH_ID_SIZE, "f-%04d", max_id + 1) >= GESFICH_ID_SIZE) {
        return GESFICH_STORAGE_ERROR;
    }
    return GESFICH_OK;
}

static char *copy_string(const char *value)
{
    size_t len = strlen(value) + 1;
    char *copy = malloc(len);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, value, len);
    return copy;
}

gesfich_result_t gesfich_crear(const char *aralmac, char id_out[GESFICH_ID_SIZE])
{
    char path[512];
    FILE *file;
    gesfich_result_t result;

    if (id_out == NULL) {
        return GESFICH_INVALID_ARG;
    }

    result = ensure_layout(aralmac);
    if (result != GESFICH_OK) {
        return result;
    }

    result = next_id(aralmac, id_out);
    if (result != GESFICH_OK) {
        return result;
    }

    result = fichero_path(path, sizeof(path), aralmac, id_out);
    if (result != GESFICH_OK) {
        return result;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return GESFICH_STORAGE_ERROR;
    }

    if (fclose(file) != 0) {
        return GESFICH_STORAGE_ERROR;
    }

    return GESFICH_OK;
}

gesfich_result_t gesfich_leer(const char *aralmac, const char *id_fichero,
                              char **contenido_out, size_t *contenido_len_out)
{
    char path[512];
    FILE *file;
    long file_size;
    char *buffer;
    size_t read_count;
    gesfich_result_t result;

    if (contenido_out == NULL || contenido_len_out == NULL) {
        return GESFICH_INVALID_ARG;
    }

    *contenido_out = NULL;
    *contenido_len_out = 0;

    result = ensure_layout(aralmac);
    if (result != GESFICH_OK) {
        return result;
    }

    result = fichero_path(path, sizeof(path), aralmac, id_fichero);
    if (result != GESFICH_OK) {
        return result;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return GESFICH_NOT_FOUND;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return GESFICH_STORAGE_ERROR;
    }

    file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return GESFICH_STORAGE_ERROR;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return GESFICH_STORAGE_ERROR;
    }

    buffer = malloc((size_t)file_size + 1);
    if (buffer == NULL) {
        fclose(file);
        return GESFICH_STORAGE_ERROR;
    }

    read_count = fread(buffer, 1, (size_t)file_size, file);
    if (read_count != (size_t)file_size || ferror(file)) {
        free(buffer);
        fclose(file);
        return GESFICH_STORAGE_ERROR;
    }

    buffer[file_size] = '\0';
    fclose(file);

    *contenido_out = buffer;
    *contenido_len_out = (size_t)file_size;
    return GESFICH_OK;
}

gesfich_result_t gesfich_listar(const char *aralmac, char ***ids_out, size_t *count_out)
{
    char dir_path[512];
    DIR *dir;
    struct dirent *entry;
    char **ids = NULL;
    size_t count = 0;
    size_t capacity = 0;
    gesfich_result_t result;

    if (ids_out == NULL || count_out == NULL) {
        return GESFICH_INVALID_ARG;
    }

    *ids_out = NULL;
    *count_out = 0;

    result = ensure_layout(aralmac);
    if (result != GESFICH_OK) {
        return result;
    }

    if (!build_path(dir_path, sizeof(dir_path), aralmac, FICHEROS_DIR, NULL)) {
        return GESFICH_STORAGE_ERROR;
    }

    dir = opendir(dir_path);
    if (dir == NULL) {
        return GESFICH_STORAGE_ERROR;
    }

    while ((entry = readdir(dir)) != NULL) {
        char id[GESFICH_ID_SIZE];

        if (filename_to_number(entry->d_name) == 0) {
            continue;
        }

        if (count == capacity) {
            size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
            char **new_ids = realloc(ids, new_capacity * sizeof(*new_ids));
            if (new_ids == NULL) {
                gesfich_liberar_lista(ids, count);
                closedir(dir);
                return GESFICH_STORAGE_ERROR;
            }
            ids = new_ids;
            capacity = new_capacity;
        }

        memcpy(id, entry->d_name, 6);
        id[6] = '\0';
        ids[count] = copy_string(id);
        if (ids[count] == NULL) {
            gesfich_liberar_lista(ids, count);
            closedir(dir);
            return GESFICH_STORAGE_ERROR;
        }
        count++;
    }

    closedir(dir);
    *ids_out = ids;
    *count_out = count;
    return GESFICH_OK;
}

gesfich_result_t gesfich_actualizar(const char *aralmac, const char *id_fichero,
                                    const char *ruta_origen)
{
    char path[512];
    FILE *source;
    FILE *target;
    unsigned char buffer[4096];
    size_t bytes_read;
    gesfich_result_t result;

    if (ruta_origen == NULL || ruta_origen[0] == '\0') {
        return GESFICH_INVALID_ARG;
    }

    result = ensure_layout(aralmac);
    if (result != GESFICH_OK) {
        return result;
    }

    result = fichero_path(path, sizeof(path), aralmac, id_fichero);
    if (result != GESFICH_OK) {
        return result;
    }

    if (!file_exists(path)) {
        return GESFICH_NOT_FOUND;
    }

    source = fopen(ruta_origen, "rb");
    if (source == NULL) {
        return GESFICH_NOT_FOUND;
    }

    target = fopen(path, "wb");
    if (target == NULL) {
        fclose(source);
        return GESFICH_STORAGE_ERROR;
    }

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        if (fwrite(buffer, 1, bytes_read, target) != bytes_read) {
            fclose(source);
            fclose(target);
            return GESFICH_STORAGE_ERROR;
        }
    }

    if (ferror(source)) {
        fclose(source);
        fclose(target);
        return GESFICH_STORAGE_ERROR;
    }

    if (fclose(source) != 0 || fclose(target) != 0) {
        return GESFICH_STORAGE_ERROR;
    }

    return GESFICH_OK;
}

gesfich_result_t gesfich_borrar(const char *aralmac, const char *id_fichero)
{
    char path[512];
    gesfich_result_t result;

    result = ensure_layout(aralmac);
    if (result != GESFICH_OK) {
        return result;
    }

    result = fichero_path(path, sizeof(path), aralmac, id_fichero);
    if (result != GESFICH_OK) {
        return result;
    }

    if (remove(path) != 0) {
        return errno == ENOENT ? GESFICH_NOT_FOUND : GESFICH_STORAGE_ERROR;
    }

    return GESFICH_OK;
}

void gesfich_liberar_lista(char **ids, size_t count)
{
    size_t i;

    if (ids == NULL) {
        return;
    }

    for (i = 0; i < count; i++) {
        free(ids[i]);
    }
    free(ids);
}
