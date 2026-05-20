#ifndef GESPROG_STORE_H
#define GESPROG_STORE_H

#include <stddef.h>

#define GESPROG_ID_SIZE 7

typedef enum {
    GESPROG_OK = 0,
    GESPROG_INVALID_ARG,
    GESPROG_NOT_FOUND,
    GESPROG_STORAGE_ERROR
} gesprog_result_t;

typedef struct {
    char id_programa[GESPROG_ID_SIZE];
    char *nombre;
    char *ejecutable;
    char **args;
    size_t args_count;
    char **env;
    size_t env_count;
} gesprog_program_t;

gesprog_result_t gesprog_guardar(const char *aralmac, const char *ejecutable,
                                 const char *const *args, size_t args_count,
                                 const char *const *env, size_t env_count,
                                 char id_out[GESPROG_ID_SIZE]);
gesprog_result_t gesprog_leer(const char *aralmac, const char *id_programa,
                              gesprog_program_t *programa_out);
gesprog_result_t gesprog_listar(const char *aralmac, char ***ids_out, size_t *count_out);
gesprog_result_t gesprog_actualizar(const char *aralmac, const char *id_programa,
                                    const char *nuevo_ejecutable);
gesprog_result_t gesprog_borrar(const char *aralmac, const char *id_programa);
void gesprog_liberar_programa(gesprog_program_t *programa);
void gesprog_liberar_lista(char **ids, size_t count);

#endif

