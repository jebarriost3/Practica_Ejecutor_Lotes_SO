#include "ejecutor_service.h"

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
    remove("tmp_service_ejecutor_aralmac/ejecuciones/e-0001.meta");
    remove("tmp_service_ejecutor_aralmac/ejecuciones/e-0002.meta");
    RMDIR("tmp_service_ejecutor_aralmac/ejecuciones");
    RMDIR("tmp_service_ejecutor_aralmac");
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

int main(void)
{
    char response[4096];
    ejecutor_service_t service;

    cleanup();

    assert_response("{\"servicio\":\"ejecutor\",\"operacion\":\"Ejecutar\","
                    "\"id-programa\":\"p-0001\"}",
                    "{\"estado\":\"ok\",\"id-ejecucion\":\"e-0001\"}");
    assert_response("{\"servicio\":\"ejecutor\",\"operacion\":\"Estado\","
                    "\"id-ejecucion\":\"e-0001\"}",
                    "{\"estado\":\"ok\",\"ejecucion\":{\"id-ejecucion\":\"e-0001\","
                    "\"id-programa\":\"p-0001\",\"proceso-estado\":\"Ejecutando\"}}");
    assert_response("{\"servicio\":\"ejecutor\",\"operacion\":\"Ejecutar\","
                    "\"id-programa\":\"p-0002\"}",
                    "{\"estado\":\"ok\",\"id-ejecucion\":\"e-0002\"}");

    assert(ejecutor_handle_json("tmp_service_ejecutor_aralmac",
                                "{\"servicio\":\"ejecutor\",\"operacion\":\"Estado\"}",
                                response, sizeof(response)));
    assert(strstr(response, "\"ejecuciones\"") != NULL);
    assert(strstr(response, "\"e-0001\"") != NULL);
    assert(strstr(response, "\"e-0002\"") != NULL);

    assert_response("{\"servicio\":\"ejecutor\",\"operacion\":\"Ejecutar\"}",
                    "{\"estado\":\"error\",\"mensaje\":\"falta campo: id-programa\"}");
    assert_response("{\"servicio\":\"ejecutor\",\"operacion\":\"Estado\","
                    "\"id-ejecucion\":\"e-9999\"}",
                    "{\"estado\":\"error\",\"mensaje\":\"ejecucion no encontrada\"}");

    cleanup();
    ejecutor_service_init(&service);
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
