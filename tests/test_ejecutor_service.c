#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "ejecutor_service.h"
#include "gesprog_store.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define MKDIR(path) _mkdir(path)
#define RMDIR(path) _rmdir(path)
#define WAIT_MS(ms) Sleep(ms)
#else
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0777)
#define RMDIR(path) rmdir(path)
#define WAIT_MS(ms) wait_ms(ms)
#endif

#ifndef _WIN32
static void wait_ms(int ms)
{
    struct timespec delay;

    delay.tv_sec = ms / 1000;
    delay.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&delay, NULL);
}
#endif

static void cleanup(void)
{
    remove("tmp_service_ejecutor_aralmac/ejecuciones/e-0001.meta");
    remove("tmp_service_ejecutor_aralmac/ejecuciones/e-0002.meta");
    remove("tmp_service_ejecutor_aralmac/ejecuciones/e-0001.out");
    remove("tmp_service_ejecutor_aralmac/ejecuciones/e-0001.err");
    remove("tmp_service_ejecutor_aralmac/ejecuciones/e-0002.out");
    remove("tmp_service_ejecutor_aralmac/ejecuciones/e-0002.err");
    remove("tmp_service_ejecutor_aralmac/ficheros/f-0001.dat");
    remove("tmp_service_ejecutor_aralmac/ficheros/f-0002.dat");
    remove("tmp_service_ejecutor_aralmac/ficheros/f-0003.dat");
    remove("tmp_service_ejecutor_aralmac/programas/p-0001.bin");
    remove("tmp_service_ejecutor_aralmac/programas/p-0001.meta");
    remove("tmp_service_ejecutor_aralmac/programas/p-0002.bin");
    remove("tmp_service_ejecutor_aralmac/programas/p-0002.meta");
    RMDIR("tmp_service_ejecutor_aralmac/ejecuciones");
    RMDIR("tmp_service_ejecutor_aralmac/ficheros");
    RMDIR("tmp_service_ejecutor_aralmac/programas");
    RMDIR("tmp_service_ejecutor_aralmac");
    remove("tmp_ejecutor_programa.bat");
    remove("tmp_ejecutor_lento.bat");
    remove("tmp_ejecutor_lento.sh");
}

static void write_text_file(const char *path, const char *content)
{
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(content, 1, strlen(content), file) == strlen(content));
    assert(fclose(file) == 0);
}

static void assert_response(const char *request, const char *expected)
{
    char response[4096];

    assert(ejecutor_handle_json("tmp_service_ejecutor_aralmac", request, response,
                                sizeof(response)));
    assert(strcmp(response, expected) == 0);
}

static void assert_stateful(ejecutor_service_t *service, const char *request,
                            const char *expected)
{
    char response[4096];

    assert(ejecutor_service_handle_json(service, "tmp_service_ejecutor_aralmac",
                                        request, response, sizeof(response)));
    assert(strcmp(response, expected) == 0);
}

static void read_text_file(const char *path, char *buffer, size_t buffer_size)
{
    FILE *file = fopen(path, "rb");
    size_t count;

    assert(file != NULL);
    count = fread(buffer, 1, buffer_size - 1, file);
    buffer[count] = '\0';
    assert(fclose(file) == 0);
}

static void wait_until_finished(ejecutor_service_t *service)
{
    char response[4096];
    int i;

    for (i = 0; i < 20; i++) {
        assert(ejecutor_service_handle_json(service, "tmp_service_ejecutor_aralmac",
                                            "{\"servicio\":\"ejecutor\","
                                            "\"operacion\":\"Estado\","
                                            "\"id-ejecucion\":\"e-0001\"}",
                                            response, sizeof(response)));
        if (strstr(response, "\"proceso-estado\":\"Terminado\"") != NULL) {
            return;
        }
        WAIT_MS(100);
    }

    assert(!"la ejecucion no termino a tiempo");
}

int main(void)
{
    char response[4096];
    char id_programa[GESPROG_ID_SIZE];
    char id_programa_lento[GESPROG_ID_SIZE];
    char file_content[128];
    ejecutor_service_t service;

    cleanup();

    assert_response("{\"servicio\":\"ejecutor\",\"operacion\":\"Ejecutar\","
                    "\"id-programa\":\"p-0001\"}",
                    "{\"estado\":\"ok\",\"id-ejecucion\":\"e-0001\"}");
    assert_response("{\"servicio\":\"ejecutor\",\"operacion\":\"Estado\","
                    "\"id-ejecucion\":\"e-0001\"}",
                    "{\"estado\":\"ok\",\"id-ejecucion\":\"e-0001\","
                    "\"id-programa\":\"p-0001\",\"proceso-estado\":\"Ejecutando\"}");
    assert_response("{\"servicio\":\"ejecutor\",\"operacion\":\"Ejecutar\","
                    "\"id-programa\":\"p-0002\"}",
                    "{\"estado\":\"ok\",\"id-ejecucion\":\"e-0002\"}");

    assert(ejecutor_handle_json("tmp_service_ejecutor_aralmac",
                                "{\"servicio\":\"ejecutor\",\"operacion\":\"Estado\"}",
                                response, sizeof(response)));
    assert(strstr(response, "\"procesos\"") != NULL);
    assert(strstr(response, "\"e-0001\"") != NULL);
    assert(strstr(response, "\"e-0002\"") != NULL);

    assert_response("{\"servicio\":\"ejecutor\",\"operacion\":\"Ejecutar\"}",
                    "{\"estado\":\"error\",\"mensaje\":\"falta campo: id-programa\"}");
    assert_response("{\"servicio\":\"ejecutor\",\"operacion\":\"Estado\","
                    "\"id-ejecucion\":\"e-9999\"}",
                    "{\"estado\":\"error\",\"mensaje\":\"ejecucion no encontrada\"}");

    cleanup();
    ejecutor_service_init(&service);

    write_text_file("tmp_ejecutor_programa.bat", "@echo off\necho ejecutor ok\n");
    assert(gesprog_guardar("tmp_service_ejecutor_aralmac", "tmp_ejecutor_programa.bat",
                           NULL, 0, NULL, 0, id_programa) == GESPROG_OK);
    assert(strcmp(id_programa, "p-0001") == 0);
    assert_stateful(&service, "{\"servicio\":\"ejecutor\",\"operacion\":\"Ejecutar\","
                              "\"id-programa\":\"p-0001\"}",
                    "{\"estado\":\"ok\",\"id-ejecucion\":\"e-0001\"}");
    assert(ejecutor_service_handle_json(&service, "tmp_service_ejecutor_aralmac",
                                        "{\"servicio\":\"ejecutor\",\"operacion\":\"Estado\","
                                        "\"id-ejecucion\":\"e-0001\"}",
                                        response, sizeof(response)));
    assert(strstr(response, "\"estado\":\"ok\"") != NULL);
    assert(strstr(response, "\"id-ejecucion\":\"e-0001\"") != NULL);
    assert(strstr(response, "\"id-programa\":\"p-0001\"") != NULL);
    wait_until_finished(&service);

    cleanup();
    ejecutor_service_init(&service);
    assert(MKDIR("tmp_service_ejecutor_aralmac") == 0);
    assert(MKDIR("tmp_service_ejecutor_aralmac/ficheros") == 0);
    write_text_file("tmp_service_ejecutor_aralmac/ficheros/f-0001.dat", "entrada lote\n");
    write_text_file("tmp_service_ejecutor_aralmac/ficheros/f-0002.dat", "");
    write_text_file("tmp_service_ejecutor_aralmac/ficheros/f-0003.dat", "");
#ifdef _WIN32
    write_text_file("tmp_ejecutor_programa.bat", "@echo off\nmore\n");
    assert(gesprog_guardar("tmp_service_ejecutor_aralmac", "tmp_ejecutor_programa.bat",
                           NULL, 0, NULL, 0, id_programa) == GESPROG_OK);
#else
    write_text_file("tmp_ejecutor_lento.sh", "cat\n");
    assert(gesprog_guardar("tmp_service_ejecutor_aralmac", "tmp_ejecutor_lento.sh",
                           NULL, 0, NULL, 0, id_programa) == GESPROG_OK);
#endif
    assert_stateful(&service, "{\"servicio\":\"ejecutor\",\"operacion\":\"Ejecutar\","
                              "\"id-programa\":\"p-0001\",\"stdin\":\"f-0001\","
                              "\"stdout\":\"f-0002\",\"stderr\":\"f-0003\"}",
                    "{\"estado\":\"ok\",\"id-ejecucion\":\"e-0001\"}");
    wait_until_finished(&service);
    read_text_file("tmp_service_ejecutor_aralmac/ficheros/f-0002.dat", file_content,
                   sizeof(file_content));
    assert(strstr(file_content, "entrada lote") != NULL);

#ifdef _WIN32
    write_text_file("tmp_ejecutor_lento.bat", "@echo off\nping -n 6 127.0.0.1 > nul\n");
    assert(gesprog_guardar("tmp_service_ejecutor_aralmac", "tmp_ejecutor_lento.bat",
                           NULL, 0, NULL, 0, id_programa_lento) == GESPROG_OK);
#else
    write_text_file("tmp_ejecutor_lento.sh", "sleep 5\n");
    assert(gesprog_guardar("tmp_service_ejecutor_aralmac", "tmp_ejecutor_lento.sh",
                           NULL, 0, NULL, 0, id_programa_lento) == GESPROG_OK);
#endif
    assert(strcmp(id_programa_lento, "p-0002") == 0);
    assert_stateful(&service, "{\"servicio\":\"ejecutor\",\"operacion\":\"Ejecutar\","
                              "\"id-programa\":\"p-0002\"}",
                    "{\"estado\":\"ok\",\"id-ejecucion\":\"e-0002\"}");
    assert_stateful(&service, "{\"servicio\":\"ejecutor\",\"operacion\":\"Matar\","
                              "\"id-ejecucion\":\"e-0002\"}",
                    "{\"estado\":\"ok\"}");
    assert(ejecutor_service_handle_json(&service, "tmp_service_ejecutor_aralmac",
                                        "{\"servicio\":\"ejecutor\",\"operacion\":\"Estado\","
                                        "\"id-ejecucion\":\"e-0002\"}",
                                        response, sizeof(response)));
    assert(strstr(response, "\"proceso-estado\":\"Terminado\"") != NULL);

    assert_stateful(&service, "{\"servicio\":\"ejecutor\",\"operacion\":\"Suspender\"}",
                    "{\"estado\":\"ok\"}");
    assert(ejecutor_service_state(&service) == EJECUTOR_SERVICE_SUSPENDIDO);
    assert_stateful(&service, "{\"servicio\":\"ejecutor\",\"operacion\":\"Ejecutar\","
                              "\"id-programa\":\"p-0001\"}",
                    "{\"estado\":\"error\",\"mensaje\":\"servicio suspendido\"}");
    assert_stateful(&service, "{\"servicio\":\"ejecutor\",\"operacion\":\"Reasumir\"}",
                    "{\"estado\":\"ok\"}");
    assert_stateful(&service, "{\"servicio\":\"ejecutor\",\"operacion\":\"Parar\"}",
                    "{\"estado\":\"ok\"}");
    assert(ejecutor_service_state(&service) == EJECUTOR_SERVICE_TERMINADO);

    cleanup();
    return 0;
}
