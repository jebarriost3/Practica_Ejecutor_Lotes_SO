#include "gesprog_store.h"

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

static void write_text_file(const char *path, const char *content)
{
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(content, 1, strlen(content), file) == strlen(content));
    assert(fclose(file) == 0);
}

static void cleanup(void)
{
    remove("tmp_gesprog_aralmac/programas/p-0001.bin");
    remove("tmp_gesprog_aralmac/programas/p-0001.meta");
    remove("tmp_gesprog_aralmac/programas/p-0002.bin");
    remove("tmp_gesprog_aralmac/programas/p-0002.meta");
    RMDIR("tmp_gesprog_aralmac/programas");
    RMDIR("tmp_gesprog_aralmac");
    remove("tmp_programa.sh");
    remove("tmp_programa_v2.sh");
}

int main(void)
{
    const char *args[] = {"--modo", "rapido"};
    const char *env[] = {"APP_MODE=batch"};
    char id1[GESPROG_ID_SIZE];
    char id2[GESPROG_ID_SIZE];
    gesprog_program_t programa;
    char **ids;
    size_t count;

    cleanup();
    write_text_file("tmp_programa.sh", "#!/bin/sh\necho hola\n");
    write_text_file("tmp_programa_v2.sh", "#!/bin/sh\necho adios\n");

    assert(gesprog_guardar("tmp_gesprog_aralmac", "tmp_programa.sh", args, 2, env, 1,
                           id1) == GESPROG_OK);
    assert(strcmp(id1, "p-0001") == 0);

    assert(gesprog_leer("tmp_gesprog_aralmac", id1, &programa) == GESPROG_OK);
    assert(strcmp(programa.id_programa, "p-0001") == 0);
    assert(strcmp(programa.nombre, "tmp_programa.sh") == 0);
    assert(strcmp(programa.ejecutable, "tmp_programa.sh") == 0);
    assert(programa.args_count == 2);
    assert(strcmp(programa.args[0], "--modo") == 0);
    assert(strcmp(programa.args[1], "rapido") == 0);
    assert(programa.env_count == 1);
    assert(strcmp(programa.env[0], "APP_MODE=batch") == 0);
    gesprog_liberar_programa(&programa);

    assert(gesprog_guardar("tmp_gesprog_aralmac", "tmp_programa.sh", NULL, 0, NULL, 0,
                           id2) == GESPROG_OK);
    assert(strcmp(id2, "p-0002") == 0);

    assert(gesprog_listar("tmp_gesprog_aralmac", &ids, &count) == GESPROG_OK);
    assert(count == 2);
    assert(strcmp(ids[0], "p-0001") == 0 || strcmp(ids[1], "p-0001") == 0);
    assert(strcmp(ids[0], "p-0002") == 0 || strcmp(ids[1], "p-0002") == 0);
    gesprog_liberar_lista(ids, count);

    assert(gesprog_actualizar("tmp_gesprog_aralmac", id1, "tmp_programa_v2.sh") ==
           GESPROG_OK);
    assert(gesprog_leer("tmp_gesprog_aralmac", id1, &programa) == GESPROG_OK);
    assert(strcmp(programa.nombre, "tmp_programa_v2.sh") == 0);
    assert(strcmp(programa.ejecutable, "tmp_programa_v2.sh") == 0);
    assert(programa.args_count == 2);
    gesprog_liberar_programa(&programa);

    assert(gesprog_borrar("tmp_gesprog_aralmac", id1) == GESPROG_OK);
    assert(gesprog_leer("tmp_gesprog_aralmac", id1, &programa) == GESPROG_NOT_FOUND);
    assert(gesprog_borrar("tmp_gesprog_aralmac", "p-9999") == GESPROG_NOT_FOUND);
    assert(gesprog_borrar("tmp_gesprog_aralmac", "x-9999") == GESPROG_INVALID_ARG);
    assert(gesprog_guardar("tmp_gesprog_aralmac", "no_existe", NULL, 0, NULL, 0, id1) ==
           GESPROG_NOT_FOUND);

    cleanup();
    return 0;
}

