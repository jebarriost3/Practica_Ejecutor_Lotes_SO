#ifndef GESFICH_STORE_H
#define GESFICH_STORE_H

#include <stddef.h>

#define GESFICH_ID_SIZE 7

typedef enum {
    GESFICH_OK = 0,
    GESFICH_INVALID_ARG,
    GESFICH_NOT_FOUND,
    GESFICH_STORAGE_ERROR
} gesfich_result_t;

gesfich_result_t gesfich_crear(const char *aralmac, char id_out[GESFICH_ID_SIZE]);
gesfich_result_t gesfich_leer(const char *aralmac, const char *id_fichero,
                              char **contenido_out, size_t *contenido_len_out);
gesfich_result_t gesfich_listar(const char *aralmac, char ***ids_out, size_t *count_out);
gesfich_result_t gesfich_actualizar(const char *aralmac, const char *id_fichero,
                                    const char *ruta_origen);
gesfich_result_t gesfich_borrar(const char *aralmac, const char *id_fichero);
void gesfich_liberar_lista(char **ids, size_t count);

#endif

