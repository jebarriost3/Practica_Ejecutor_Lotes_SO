#include "gesfich_store.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define RMDIR(path) _rmdir(path)
#else
#include <unistd.h>
#define RMDIR(path) rmdir(path)
#endif

static void write_text_file(const char *path, const char *content)
{
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(content, 1, strlen(content), file) == strlen(content));
    assert(fclose(file) == 0);
}

static void cleanup(void)
{
    remove("tmp_gesfich_aralmac/ficheros/f-0001.dat");
    remove("tmp_gesfich_aralmac/ficheros/f-0002.dat");
    RMDIR("tmp_gesfich_aralmac/ficheros");
    RMDIR("tmp_gesfich_aralmac");
    remove("tmp_origen.txt");
}

int main(void)
{
    char id1[GESFICH_ID_SIZE];
    char id2[GESFICH_ID_SIZE];
    char *contenido;
    size_t contenido_len;
    char **ids;
    size_t count;

    cleanup();

    assert(gesfich_crear("tmp_gesfich_aralmac", id1) == GESFICH_OK);
    assert(strcmp(id1, "f-0001") == 0);

    assert(gesfich_crear("tmp_gesfich_aralmac", id2) == GESFICH_OK);
    assert(strcmp(id2, "f-0002") == 0);

    assert(gesfich_leer("tmp_gesfich_aralmac", id1, &contenido, &contenido_len) ==
           GESFICH_OK);
    assert(contenido_len == 0);
    assert(strcmp(contenido, "") == 0);
    free(contenido);

    write_text_file("tmp_origen.txt", "entrada para lote\n");
    assert(gesfich_actualizar("tmp_gesfich_aralmac", id1, "tmp_origen.txt") ==
           GESFICH_OK);

    assert(gesfich_leer("tmp_gesfich_aralmac", id1, &contenido, &contenido_len) ==
           GESFICH_OK);
    assert(contenido_len == strlen("entrada para lote\n"));
    assert(strcmp(contenido, "entrada para lote\n") == 0);
    free(contenido);

    assert(gesfich_listar("tmp_gesfich_aralmac", &ids, &count) == GESFICH_OK);
    assert(count == 2);
    assert(strcmp(ids[0], "f-0001") == 0 || strcmp(ids[1], "f-0001") == 0);
    assert(strcmp(ids[0], "f-0002") == 0 || strcmp(ids[1], "f-0002") == 0);
    gesfich_liberar_lista(ids, count);

    assert(gesfich_borrar("tmp_gesfich_aralmac", id1) == GESFICH_OK);
    assert(gesfich_leer("tmp_gesfich_aralmac", id1, &contenido, &contenido_len) ==
           GESFICH_NOT_FOUND);
    assert(gesfich_borrar("tmp_gesfich_aralmac", "f-9999") == GESFICH_NOT_FOUND);
    assert(gesfich_borrar("tmp_gesfich_aralmac", "x-9999") == GESFICH_INVALID_ARG);

    cleanup();
    return 0;
}

