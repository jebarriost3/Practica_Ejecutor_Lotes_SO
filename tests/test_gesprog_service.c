#include "gesprog_service.h"

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
    remove("tmp_service_gesprog_aralmac/programas/p-0001.bin");
    remove("tmp_service_gesprog_aralmac/programas/p-0001.meta");
    remove("tmp_service_gesprog_aralmac/programas/p-0002.bin");
    remove("tmp_service_gesprog_aralmac/programas/p-0002.meta");
    RMDIR("tmp_service_gesprog_aralmac/programas");
    RMDIR("tmp_service_gesprog_aralmac");
    remove("tmp_service_programa.sh");
    remove("tmp_service_programa_v2.sh");
}

static void assert_response(const char *request, const char *expected)
{
    char response[4096];

    assert(gesprog_handle_json("tmp_service_gesprog_aralmac", request, response,
                               sizeof(response)));
    assert(strcmp(response, expected) == 0);
}

static void assert_stateful_response(gesprog_service_t *service, const char *request,
                                     const char *expected)
{
    char response[4096];

    assert(gesprog_service_handle_json(service, "tmp_service_gesprog_aralmac", request,
                                       response, sizeof(response)));
    assert(strcmp(response, expected) == 0);
}

int main(void)
{
    char response[4096];
    gesprog_service_t service;

    cleanup();
    write_text_file("tmp_service_programa.sh", "#!/bin/sh\necho hola\n");
    write_text_file("tmp_service_programa_v2.sh", "#!/bin/sh\necho adios\n");

    assert_response("{\"servicio\":\"gesprog\",\"operacion\":\"Guardar\","
                    "\"ejecutable\":\"tmp_service_programa.sh\","
                    "\"args\":[\"--modo\",\"rapido\"],\"env\":[\"APP_MODE=batch\"]}",
                    "{\"estado\":\"ok\",\"id-programa\":\"p-0001\"}");

    assert_response("{\"servicio\":\"gesprog\",\"operacion\":\"Leer\","
                    "\"id-programa\":\"p-0001\"}",
                    "{\"estado\":\"ok\",\"programa\":{\"id-programa\":\"p-0001\","
                    "\"nombre\":\"tmp_service_programa.sh\","
                    "\"args\":[\"--modo\",\"rapido\"],\"env\":[\"APP_MODE=batch\"]}}");

    assert_response("{\"servicio\":\"gesprog\",\"operacion\":\"Guardar\","
                    "\"ejecutable\":\"tmp_service_programa.sh\"}",
                    "{\"estado\":\"ok\",\"id-programa\":\"p-0002\"}");

    assert(gesprog_handle_json("tmp_service_gesprog_aralmac",
                               "{\"servicio\":\"gesprog\",\"operacion\":\"Leer\"}", response,
                               sizeof(response)));
    assert(strstr(response, "\"estado\":\"ok\"") != NULL);
    assert(strstr(response, "\"programas\"") != NULL);
    assert(strstr(response, "\"p-0001\"") != NULL);
    assert(strstr(response, "\"p-0002\"") != NULL);

    assert_response("{\"servicio\":\"gesprog\",\"operacion\":\"Actualizar\","
                    "\"id-programa\":\"p-0001\",\"ruta\":\"tmp_service_programa_v2.sh\"}",
                    "{\"estado\":\"ok\"}");

    assert_response("{\"servicio\":\"gesprog\",\"operacion\":\"Borrar\","
                    "\"id-programa\":\"p-0001\"}",
                    "{\"estado\":\"ok\"}");

    assert_response("{\"servicio\":\"gesprog\",\"operacion\":\"Leer\","
                    "\"id-programa\":\"p-0001\"}",
                    "{\"estado\":\"error\",\"mensaje\":\"programa no encontrado\"}");

    assert_response("{\"servicio\":\"gesprog\",\"operacion\":\"Actualizar\","
                    "\"id-programa\":\"p-9999\"}",
                    "{\"estado\":\"error\",\"mensaje\":\"faltan campos: id-programa, ruta\"}");

    assert_response("{\"servicio\":\"gesprog\",\"operacion\":\"Nada\"}",
                    "{\"estado\":\"error\",\"mensaje\":\"operacion desconocida\"}");

    cleanup();
    write_text_file("tmp_service_programa.sh", "#!/bin/sh\necho hola\n");
    gesprog_service_init(&service);
    assert(gesprog_service_state(&service) == GESPROG_SERVICE_CORRIENDO);

    assert_stateful_response(&service, "{\"servicio\":\"gesprog\",\"operacion\":\"Guardar\","
                                      "\"ejecutable\":\"tmp_service_programa.sh\"}",
                             "{\"estado\":\"ok\",\"id-programa\":\"p-0001\"}");
    assert_stateful_response(&service, "{\"servicio\":\"gesprog\",\"operacion\":\"Suspender\"}",
                             "{\"estado\":\"ok\"}");
    assert(gesprog_service_state(&service) == GESPROG_SERVICE_SUSPENDIDO);
    assert_stateful_response(&service, "{\"servicio\":\"gesprog\",\"operacion\":\"Leer\","
                                      "\"id-programa\":\"p-0001\"}",
                             "{\"estado\":\"error\",\"mensaje\":\"servicio suspendido\"}");
    assert_stateful_response(&service, "{\"servicio\":\"gesprog\",\"operacion\":\"Reasumir\"}",
                             "{\"estado\":\"ok\"}");
    assert(gesprog_service_state(&service) == GESPROG_SERVICE_CORRIENDO);
    assert_stateful_response(&service, "{\"servicio\":\"gesprog\",\"operacion\":\"Terminar\"}",
                             "{\"estado\":\"ok\"}");
    assert(gesprog_service_state(&service) == GESPROG_SERVICE_TERMINADO);

    cleanup();
    return 0;
}
