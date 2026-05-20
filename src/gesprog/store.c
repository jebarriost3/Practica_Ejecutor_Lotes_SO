#include "gesprog_store.h"

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

#define PROGRAMAS_DIR "programas"
#define BIN_EXT ".bin"
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

static gesprog_result_t ensure_layout(const char *aralmac)
{
    char path[512];

    if (aralmac == NULL || aralmac[0] == '\0') {
        return GESPROG_INVALID_ARG;
    }

    if (!make_dir_if_needed(aralmac)) {
        return GESPROG_STORAGE_ERROR;
    }

    if (!build_path(path, sizeof(path), aralmac, PROGRAMAS_DIR, NULL)) {
        return GESPROG_STORAGE_ERROR;
    }

    if (!make_dir_if_needed(path)) {
        return GESPROG_STORAGE_ERROR;
    }

    return GESPROG_OK;
}

static char *copy_string(const char *value)
{
    size_t len;
    char *copy;

    if (value == NULL) {
        return NULL;
    }

    len = strlen(value) + 1;
    copy = malloc(len);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, value, len);
    return copy;
}

static const char *base_name(const char *path)
{
    const char *slash;
    const char *backslash;
    const char *base;

    slash = strrchr(path, '/');
    backslash = strrchr(path, '\\');
    base = slash > backslash ? slash : backslash;

    return base == NULL ? path : base + 1;
}

static int id_filename(char *buffer, size_t buffer_size, const char *id_programa,
                       const char *ext)
{
    int written = snprintf(buffer, buffer_size, "%s%s", id_programa, ext);
    return written > 0 && (size_t)written < buffer_size;
}

static gesprog_result_t programa_path(char *buffer, size_t buffer_size, const char *aralmac,
                                      const char *id_programa, const char *ext)
{
    char filename[32];

    if (!protocol_valid_id(id_programa, ID_PROGRAMA)) {
        return GESPROG_INVALID_ARG;
    }

    if (!id_filename(filename, sizeof(filename), id_programa, ext)) {
        return GESPROG_STORAGE_ERROR;
    }

    if (!build_path(buffer, buffer_size, aralmac, PROGRAMAS_DIR, filename)) {
        return GESPROG_STORAGE_ERROR;
    }

    return GESPROG_OK;
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

static gesprog_result_t copy_file(const char *source_path, const char *target_path)
{
    FILE *source;
    FILE *target;
    unsigned char buffer[4096];
    size_t bytes_read;

    source = fopen(source_path, "rb");
    if (source == NULL) {
        return GESPROG_NOT_FOUND;
    }

    target = fopen(target_path, "wb");
    if (target == NULL) {
        fclose(source);
        return GESPROG_STORAGE_ERROR;
    }

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        if (fwrite(buffer, 1, bytes_read, target) != bytes_read) {
            fclose(source);
            fclose(target);
            return GESPROG_STORAGE_ERROR;
        }
    }

    if (ferror(source)) {
        fclose(source);
        fclose(target);
        return GESPROG_STORAGE_ERROR;
    }

    if (fclose(source) != 0 || fclose(target) != 0) {
        return GESPROG_STORAGE_ERROR;
    }

    return GESPROG_OK;
}

static int filename_to_number(const char *name)
{
    char id[GESPROG_ID_SIZE];

    if (strlen(name) != 11 || strcmp(name + 6, META_EXT) != 0) {
        return 0;
    }

    memcpy(id, name, 6);
    id[6] = '\0';

    if (!protocol_valid_id(id, ID_PROGRAMA)) {
        return 0;
    }

    return (id[2] - '0') * 1000 + (id[3] - '0') * 100 + (id[4] - '0') * 10 +
           (id[5] - '0');
}

static gesprog_result_t next_id(const char *aralmac, char id_out[GESPROG_ID_SIZE])
{
    char dir_path[512];
    DIR *dir;
    struct dirent *entry;
    int max_id = 0;

    if (!build_path(dir_path, sizeof(dir_path), aralmac, PROGRAMAS_DIR, NULL)) {
        return GESPROG_STORAGE_ERROR;
    }

    dir = opendir(dir_path);
    if (dir == NULL) {
        return GESPROG_STORAGE_ERROR;
    }

    while ((entry = readdir(dir)) != NULL) {
        int number = filename_to_number(entry->d_name);
        if (number > max_id) {
            max_id = number;
        }
    }

    closedir(dir);

    if (max_id >= 9999 ||
        snprintf(id_out, GESPROG_ID_SIZE, "p-%04d", max_id + 1) >= GESPROG_ID_SIZE) {
        return GESPROG_STORAGE_ERROR;
    }

    return GESPROG_OK;
}

static gesprog_result_t write_metadata(const char *meta_path, const char *id_programa,
                                       const char *nombre, const char *ejecutable,
                                       const char *const *args, size_t args_count,
                                       const char *const *env, size_t env_count)
{
    FILE *file;
    size_t i;

    file = fopen(meta_path, "wb");
    if (file == NULL) {
        return GESPROG_STORAGE_ERROR;
    }

    fprintf(file, "id=%s\n", id_programa);
    fprintf(file, "nombre=%s\n", nombre);
    fprintf(file, "ejecutable=%s\n", ejecutable);
    fprintf(file, "args=%lu\n", (unsigned long)args_count);
    for (i = 0; i < args_count; i++) {
        fprintf(file, "arg=%s\n", args[i]);
    }
    fprintf(file, "env=%lu\n", (unsigned long)env_count);
    for (i = 0; i < env_count; i++) {
        fprintf(file, "envv=%s\n", env[i]);
    }

    if (fclose(file) != 0) {
        return GESPROG_STORAGE_ERROR;
    }

    return GESPROG_OK;
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

static int read_count_prefixed(FILE *file, const char *prefix, size_t *count_out)
{
    char value[64];
    char *end;
    unsigned long count;

    if (!read_prefixed(file, prefix, value, sizeof(value))) {
        return 0;
    }

    count = strtoul(value, &end, 10);
    if (*end != '\0') {
        return 0;
    }

    *count_out = (size_t)count;
    return 1;
}

static int read_string_array(FILE *file, const char *prefix, char ***values_out,
                             size_t count)
{
    char **values = NULL;
    size_t i;

    if (count > 0) {
        values = calloc(count, sizeof(*values));
        if (values == NULL) {
            return 0;
        }
    }

    for (i = 0; i < count; i++) {
        char value[1024];
        if (!read_prefixed(file, prefix, value, sizeof(value))) {
            size_t j;
            for (j = 0; j < i; j++) {
                free(values[j]);
            }
            free(values);
            return 0;
        }
        values[i] = copy_string(value);
        if (values[i] == NULL) {
            size_t j;
            for (j = 0; j < i; j++) {
                free(values[j]);
            }
            free(values);
            return 0;
        }
    }

    *values_out = values;
    return 1;
}

gesprog_result_t gesprog_guardar(const char *aralmac, const char *ejecutable,
                                 const char *const *args, size_t args_count,
                                 const char *const *env, size_t env_count,
                                 char id_out[GESPROG_ID_SIZE])
{
    char bin_path[512];
    char meta_path[512];
    gesprog_result_t result;

    if (ejecutable == NULL || ejecutable[0] == '\0' || id_out == NULL) {
        return GESPROG_INVALID_ARG;
    }

    result = ensure_layout(aralmac);
    if (result != GESPROG_OK) {
        return result;
    }

    if (!file_exists(ejecutable)) {
        return GESPROG_NOT_FOUND;
    }

    result = next_id(aralmac, id_out);
    if (result != GESPROG_OK) {
        return result;
    }

    result = programa_path(bin_path, sizeof(bin_path), aralmac, id_out, BIN_EXT);
    if (result != GESPROG_OK) {
        return result;
    }
    result = programa_path(meta_path, sizeof(meta_path), aralmac, id_out, META_EXT);
    if (result != GESPROG_OK) {
        return result;
    }

    result = copy_file(ejecutable, bin_path);
    if (result != GESPROG_OK) {
        return result;
    }

    return write_metadata(meta_path, id_out, base_name(ejecutable), ejecutable, args, args_count,
                          env, env_count);
}

gesprog_result_t gesprog_leer(const char *aralmac, const char *id_programa,
                              gesprog_program_t *programa_out)
{
    char meta_path[512];
    char value[1024];
    FILE *file;
    gesprog_result_t result;

    if (programa_out == NULL) {
        return GESPROG_INVALID_ARG;
    }

    memset(programa_out, 0, sizeof(*programa_out));

    result = ensure_layout(aralmac);
    if (result != GESPROG_OK) {
        return result;
    }

    result = programa_path(meta_path, sizeof(meta_path), aralmac, id_programa, META_EXT);
    if (result != GESPROG_OK) {
        return result;
    }

    file = fopen(meta_path, "rb");
    if (file == NULL) {
        return GESPROG_NOT_FOUND;
    }

    if (!read_prefixed(file, "id=", value, sizeof(value)) ||
        !protocol_valid_id(value, ID_PROGRAMA)) {
        fclose(file);
        return GESPROG_STORAGE_ERROR;
    }
    strcpy(programa_out->id_programa, value);

    if (!read_prefixed(file, "nombre=", value, sizeof(value))) {
        fclose(file);
        gesprog_liberar_programa(programa_out);
        return GESPROG_STORAGE_ERROR;
    }
    programa_out->nombre = copy_string(value);

    if (!read_prefixed(file, "ejecutable=", value, sizeof(value))) {
        fclose(file);
        gesprog_liberar_programa(programa_out);
        return GESPROG_STORAGE_ERROR;
    }
    programa_out->ejecutable = copy_string(value);

    if (!programa_out->nombre || !programa_out->ejecutable ||
        !read_count_prefixed(file, "args=", &programa_out->args_count) ||
        !read_string_array(file, "arg=", &programa_out->args, programa_out->args_count) ||
        !read_count_prefixed(file, "env=", &programa_out->env_count) ||
        !read_string_array(file, "envv=", &programa_out->env, programa_out->env_count)) {
        fclose(file);
        gesprog_liberar_programa(programa_out);
        return GESPROG_STORAGE_ERROR;
    }

    fclose(file);
    return GESPROG_OK;
}

gesprog_result_t gesprog_listar(const char *aralmac, char ***ids_out, size_t *count_out)
{
    char dir_path[512];
    DIR *dir;
    struct dirent *entry;
    char **ids = NULL;
    size_t count = 0;
    size_t capacity = 0;
    gesprog_result_t result;

    if (ids_out == NULL || count_out == NULL) {
        return GESPROG_INVALID_ARG;
    }

    *ids_out = NULL;
    *count_out = 0;

    result = ensure_layout(aralmac);
    if (result != GESPROG_OK) {
        return result;
    }

    if (!build_path(dir_path, sizeof(dir_path), aralmac, PROGRAMAS_DIR, NULL)) {
        return GESPROG_STORAGE_ERROR;
    }

    dir = opendir(dir_path);
    if (dir == NULL) {
        return GESPROG_STORAGE_ERROR;
    }

    while ((entry = readdir(dir)) != NULL) {
        char id[GESPROG_ID_SIZE];
        if (filename_to_number(entry->d_name) == 0) {
            continue;
        }
        if (count == capacity) {
            size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
            char **new_ids = realloc(ids, new_capacity * sizeof(*new_ids));
            if (new_ids == NULL) {
                gesprog_liberar_lista(ids, count);
                closedir(dir);
                return GESPROG_STORAGE_ERROR;
            }
            ids = new_ids;
            capacity = new_capacity;
        }
        memcpy(id, entry->d_name, 6);
        id[6] = '\0';
        ids[count] = copy_string(id);
        if (ids[count] == NULL) {
            gesprog_liberar_lista(ids, count);
            closedir(dir);
            return GESPROG_STORAGE_ERROR;
        }
        count++;
    }

    closedir(dir);
    *ids_out = ids;
    *count_out = count;
    return GESPROG_OK;
}

gesprog_result_t gesprog_actualizar(const char *aralmac, const char *id_programa,
                                    const char *nuevo_ejecutable)
{
    char bin_path[512];
    char meta_path[512];
    gesprog_program_t programa;
    gesprog_result_t result;

    if (nuevo_ejecutable == NULL || nuevo_ejecutable[0] == '\0') {
        return GESPROG_INVALID_ARG;
    }

    result = gesprog_leer(aralmac, id_programa, &programa);
    if (result != GESPROG_OK) {
        return result;
    }

    result = programa_path(bin_path, sizeof(bin_path), aralmac, id_programa, BIN_EXT);
    if (result != GESPROG_OK) {
        gesprog_liberar_programa(&programa);
        return result;
    }
    result = programa_path(meta_path, sizeof(meta_path), aralmac, id_programa, META_EXT);
    if (result != GESPROG_OK) {
        gesprog_liberar_programa(&programa);
        return result;
    }

    result = copy_file(nuevo_ejecutable, bin_path);
    if (result == GESPROG_OK) {
        result = write_metadata(meta_path, id_programa, base_name(nuevo_ejecutable),
                                nuevo_ejecutable, (const char *const *)programa.args,
                                programa.args_count, (const char *const *)programa.env,
                                programa.env_count);
    }

    gesprog_liberar_programa(&programa);
    return result;
}

gesprog_result_t gesprog_borrar(const char *aralmac, const char *id_programa)
{
    char bin_path[512];
    char meta_path[512];
    gesprog_result_t result;

    result = ensure_layout(aralmac);
    if (result != GESPROG_OK) {
        return result;
    }

    result = programa_path(bin_path, sizeof(bin_path), aralmac, id_programa, BIN_EXT);
    if (result != GESPROG_OK) {
        return result;
    }
    result = programa_path(meta_path, sizeof(meta_path), aralmac, id_programa, META_EXT);
    if (result != GESPROG_OK) {
        return result;
    }

    if (!file_exists(meta_path)) {
        return GESPROG_NOT_FOUND;
    }

    remove(bin_path);
    if (remove(meta_path) != 0) {
        return errno == ENOENT ? GESPROG_NOT_FOUND : GESPROG_STORAGE_ERROR;
    }

    return GESPROG_OK;
}

void gesprog_liberar_programa(gesprog_program_t *programa)
{
    size_t i;

    if (programa == NULL) {
        return;
    }

    free(programa->nombre);
    free(programa->ejecutable);
    for (i = 0; i < programa->args_count; i++) {
        free(programa->args[i]);
    }
    free(programa->args);
    for (i = 0; i < programa->env_count; i++) {
        free(programa->env[i]);
    }
    free(programa->env);
    memset(programa, 0, sizeof(*programa));
}

void gesprog_liberar_lista(char **ids, size_t count)
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
