#include "ejecutor_store.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define RMDIR(path) _rmdir(path)
#else
#include <unistd.h>
#define RMDIR(path) rmdir(path)
#endif

static void cleanup(void)
{
    remove("tmp_ejecutor_aralmac/ejecuciones/e-0001.meta");
    remove("tmp_ejecutor_aralmac/ejecuciones/e-0002.meta");
    RMDIR("tmp_ejecutor_aralmac/ejecuciones");
    RMDIR("tmp_ejecutor_aralmac");
}

static int contains_id(ejecutor_execution_t *items, size_t count, const char *id)
{
    size_t i;
    for (i = 0; i < count; i++) {
        if (strcmp(items[i].id_ejecucion, id) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    char id1[EJECUTOR_ID_SIZE];
    char id2[EJECUTOR_ID_SIZE];
    ejecutor_execution_t ejecucion;
    ejecutor_execution_t *items;
    size_t count;

    cleanup();

    assert(ejecutor_registrar("tmp_ejecutor_aralmac", "p-0001", id1) == EJECUTOR_OK);
    assert(strcmp(id1, "e-0001") == 0);

    assert(ejecutor_leer("tmp_ejecutor_aralmac", id1, &ejecucion) == EJECUTOR_OK);
    assert(strcmp(ejecucion.id_ejecucion, "e-0001") == 0);
    assert(strcmp(ejecucion.id_programa, "p-0001") == 0);
    assert(ejecucion.estado == EJECUTOR_PROCESO_EJECUTANDO);
    assert(ejecucion.tiene_codigo_salida == 0);
    assert(strcmp(ejecutor_estado_texto(ejecucion.estado), "Ejecutando") == 0);

    assert(ejecutor_registrar("tmp_ejecutor_aralmac", "p-0002", id2) == EJECUTOR_OK);
    assert(strcmp(id2, "e-0002") == 0);

    assert(ejecutor_listar("tmp_ejecutor_aralmac", &items, &count) == EJECUTOR_OK);
    assert(count == 2);
    assert(contains_id(items, count, "e-0001"));
    assert(contains_id(items, count, "e-0002"));
    ejecutor_liberar_lista(items);

    assert(ejecutor_marcar_suspendida("tmp_ejecutor_aralmac", id1) == EJECUTOR_OK);
    assert(ejecutor_leer("tmp_ejecutor_aralmac", id1, &ejecucion) == EJECUTOR_OK);
    assert(ejecucion.estado == EJECUTOR_PROCESO_SUSPENDIDO);
    assert(ejecucion.tiene_codigo_salida == 0);

    assert(ejecutor_marcar_ejecutando("tmp_ejecutor_aralmac", id1) == EJECUTOR_OK);
    assert(ejecutor_leer("tmp_ejecutor_aralmac", id1, &ejecucion) == EJECUTOR_OK);
    assert(ejecucion.estado == EJECUTOR_PROCESO_EJECUTANDO);

    assert(ejecutor_marcar_terminada("tmp_ejecutor_aralmac", id1, 7) == EJECUTOR_OK);
    assert(ejecutor_leer("tmp_ejecutor_aralmac", id1, &ejecucion) == EJECUTOR_OK);
    assert(ejecucion.estado == EJECUTOR_PROCESO_TERMINADO);
    assert(ejecucion.tiene_codigo_salida == 1);
    assert(ejecucion.codigo_salida == 7);

    assert(ejecutor_registrar("tmp_ejecutor_aralmac", "x-0001", id1) ==
           EJECUTOR_INVALID_ARG);
    assert(ejecutor_leer("tmp_ejecutor_aralmac", "e-9999", &ejecucion) ==
           EJECUTOR_NOT_FOUND);
    assert(ejecutor_marcar_suspendida("tmp_ejecutor_aralmac", "x-0001") ==
           EJECUTOR_INVALID_ARG);

    cleanup();
    return 0;
}
