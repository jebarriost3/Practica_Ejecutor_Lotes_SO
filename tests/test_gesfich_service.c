#include "gesfich_service.h"

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
    remove("tmp_service_aralmac/ficheros/f-0001.dat");
    remove("tmp_service_aralmac/ficheros/f-0002.dat");
    RMDIR("tmp_service_aralmac/ficheros");
    RMDIR("tmp_service_aralmac");
    remove("tmp_service_origen.txt");
}

static void assert_response(const char *request, const char *expected)
{
    char response[4096];

    assert(gesfich_handle_json("tmp_service_aralmac", request, response, sizeof(response)));
    assert(strcmp(response, expected) == 0);
}

static void assert_stateful_response(gesfich_service_t *service, const char *request,
                                     const char *expected)
{
    char response[4096];

    assert(gesfich_service_handle_json(service, "tmp_service_aralmac", request, response,
                                       sizeof(response)));
    assert(strcmp(response, expected) == 0);
}

int main(void)
{
    char response[4096];
    gesfich_service_t service;

    cleanup();

    assert_response("{\"servicio\":\"gesfich\",\"operacion\":\"Crear\"}",
                    "{\"estado\":\"ok\",\"id-fichero\":\"f-0001\"}");

    assert_response("{\"servicio\":\"gesfich\",\"operacion\":\"Leer\",\"id-fichero\":\"f-0001\"}",
                    "{\"estado\":\"ok\",\"contenido\":\"\"}");

    write_text_file("tmp_service_origen.txt", "linea 1\nlinea 2\n");
    assert_response("{\"servicio\":\"gesfich\",\"operacion\":\"Actualizar\","
                    "\"id-fichero\":\"f-0001\",\"ruta\":\"tmp_service_origen.txt\"}",
                    "{\"estado\":\"ok\"}");

    assert_response("{\"servicio\":\"gesfich\",\"operacion\":\"Leer\",\"id-fichero\":\"f-0001\"}",
                    "{\"estado\":\"ok\",\"contenido\":\"linea 1\\nlinea 2\\n\"}");

    assert_response("{\"servicio\":\"gesfich\",\"operacion\":\"Crear\"}",
                    "{\"estado\":\"ok\",\"id-fichero\":\"f-0002\"}");

    assert(gesfich_handle_json("tmp_service_aralmac",
                               "{\"servicio\":\"gesfich\",\"operacion\":\"Leer\"}", response,
                               sizeof(response)));
    assert(strstr(response, "\"estado\":\"ok\"") != NULL);
    assert(strstr(response, "\"ficheros\"") != NULL);
    assert(strstr(response, "\"f-0001\"") != NULL);
    assert(strstr(response, "\"f-0002\"") != NULL);

    assert_response("{\"servicio\":\"gesfich\",\"operacion\":\"Borrar\","
                    "\"id-fichero\":\"f-0001\"}",
                    "{\"estado\":\"ok\"}");

    assert_response("{\"servicio\":\"gesfich\",\"operacion\":\"Leer\",\"id-fichero\":\"f-0001\"}",
                    "{\"estado\":\"error\",\"mensaje\":\"fichero no encontrado\"}");

    assert_response("{\"servicio\":\"gesfich\",\"operacion\":\"Actualizar\","
                    "\"id-fichero\":\"f-9999\"}",
                    "{\"estado\":\"error\",\"mensaje\":\"faltan campos: id-fichero, ruta\"}");

    assert_response("{\"servicio\":\"gesfich\",\"operacion\":\"Nada\"}",
                    "{\"estado\":\"error\",\"mensaje\":\"operacion desconocida\"}");

    cleanup();
    gesfich_service_init(&service);
    assert(gesfich_service_state(&service) == GESFICH_SERVICE_CORRIENDO);

    assert_stateful_response(&service, "{\"servicio\":\"gesfich\",\"operacion\":\"Crear\"}",
                             "{\"estado\":\"ok\",\"id-fichero\":\"f-0001\"}");
    assert_stateful_response(&service, "{\"servicio\":\"gesfich\",\"operacion\":\"Suspender\"}",
                             "{\"estado\":\"ok\"}");
    assert(gesfich_service_state(&service) == GESFICH_SERVICE_SUSPENDIDO);

    assert_stateful_response(&service,
                             "{\"servicio\":\"gesfich\",\"operacion\":\"Leer\","
                             "\"id-fichero\":\"f-0001\"}",
                             "{\"estado\":\"error\",\"mensaje\":\"servicio suspendido\"}");
    assert_stateful_response(&service, "{\"servicio\":\"gesfich\",\"operacion\":\"Reasumir\"}",
                             "{\"estado\":\"ok\"}");
    assert(gesfich_service_state(&service) == GESFICH_SERVICE_CORRIENDO);

    assert_stateful_response(&service,
                             "{\"servicio\":\"gesfich\",\"operacion\":\"Leer\","
                             "\"id-fichero\":\"f-0001\"}",
                             "{\"estado\":\"ok\",\"contenido\":\"\"}");
    assert_stateful_response(&service, "{\"servicio\":\"gesfich\",\"operacion\":\"Terminar\"}",
                             "{\"estado\":\"ok\"}");
    assert(gesfich_service_state(&service) == GESFICH_SERVICE_TERMINADO);

    cleanup();
    return 0;
}
