#ifndef EJECUTOR_STORE_H
#define EJECUTOR_STORE_H

#include <stddef.h>

#define EJECUTOR_ID_SIZE 7

typedef enum {
    EJECUTOR_OK = 0,
    EJECUTOR_INVALID_ARG,
    EJECUTOR_NOT_FOUND,
    EJECUTOR_STORAGE_ERROR
} ejecutor_result_t;

typedef enum {
    EJECUTOR_PROCESO_EJECUTANDO = 0,
    EJECUTOR_PROCESO_SUSPENDIDO,
    EJECUTOR_PROCESO_TERMINADO
} ejecutor_process_state_t;

typedef struct {
    char id_ejecucion[EJECUTOR_ID_SIZE];
    char id_programa[EJECUTOR_ID_SIZE];
    ejecutor_process_state_t estado;
    int codigo_salida;
    int tiene_codigo_salida;
} ejecutor_execution_t;

ejecutor_result_t ejecutor_registrar(const char *aralmac, const char *id_programa,
                                     char id_out[EJECUTOR_ID_SIZE]);
ejecutor_result_t ejecutor_leer(const char *aralmac, const char *id_ejecucion,
                                ejecutor_execution_t *ejecucion_out);
ejecutor_result_t ejecutor_listar(const char *aralmac, ejecutor_execution_t **items_out,
                                  size_t *count_out);
ejecutor_result_t ejecutor_marcar_ejecutando(const char *aralmac,
                                             const char *id_ejecucion);
ejecutor_result_t ejecutor_marcar_suspendida(const char *aralmac,
                                             const char *id_ejecucion);
ejecutor_result_t ejecutor_marcar_terminada(const char *aralmac,
                                            const char *id_ejecucion,
                                            int codigo_salida);
const char *ejecutor_estado_texto(ejecutor_process_state_t estado);
void ejecutor_liberar_lista(ejecutor_execution_t *items);

#endif
