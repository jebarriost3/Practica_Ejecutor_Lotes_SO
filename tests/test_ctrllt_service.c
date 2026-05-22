#include "ctrllt_service.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    ctrllt_target_t targets[8];
    char requests[8][512];
    size_t count;
    int fail;
} fake_router_t;

static int fake_send(ctrllt_target_t target, const char *request, char *response,
                     size_t response_size, void *user_data)
{
    fake_router_t *router = user_data;

    if (router->fail) {
        return 0;
    }
    assert(router->count < 8);
    router->targets[router->count] = target;
    snprintf(router->requests[router->count], sizeof(router->requests[router->count]),
             "%s", request);
    router->count++;

    return snprintf(response, response_size, "{\"estado\":\"ok\",\"servicio\":\"%s\"}",
                    ctrllt_target_name(target)) > 0;
}

static void assert_response(ctrllt_service_t *service, const char *request,
                            const char *expected)
{
    char response[4096];

    assert(ctrllt_service_handle_json(service, request, response, sizeof(response)));
    assert(strcmp(response, expected) == 0);
}

int main(void)
{
    ctrllt_service_t service;
    fake_router_t router;

    memset(&router, 0, sizeof(router));
    ctrllt_service_init(&service, fake_send, &router);

    assert_response(&service, "{\"servicio\":\"gesfich\",\"operacion\":\"Crear\"}",
                    "{\"estado\":\"ok\",\"servicio\":\"gesfich\"}");
    assert(router.count == 1);
    assert(router.targets[0] == CTRLLT_TARGET_GESFICH);
    assert(strcmp(router.requests[0],
                  "{\"servicio\":\"gesfich\",\"operacion\":\"Crear\"}") == 0);

    assert_response(&service, "{\"servicio\":\"gesprog\",\"operacion\":\"Leer\"}",
                    "{\"estado\":\"ok\",\"servicio\":\"gesprog\"}");
    assert_response(&service, "{\"servicio\":\"ejecutor\",\"operacion\":\"Estado\"}",
                    "{\"estado\":\"ok\",\"servicio\":\"ejecutor\"}");
    assert(router.count == 3);

    assert_response(&service, "{\"servicio\":\"desconocido\",\"operacion\":\"Leer\"}",
                    "{\"estado\":\"error\",\"mensaje\":\"servicio desconocido\"}");
    assert_response(&service, "{\"servicio\":\"ctrllt\",\"operacion\":\"Otra\"}",
                    "{\"estado\":\"error\",\"mensaje\":\"operacion ctrllt desconocida\"}");

    assert_response(&service, "{\"servicio\":\"ctrllt\",\"operacion\":\"Terminar\"}",
                    "{\"estado\":\"ok\"}");
    assert(router.count == 6);
    assert(router.targets[3] == CTRLLT_TARGET_GESFICH);
    assert(strcmp(router.requests[3],
                  "{\"servicio\":\"gesfich\",\"operacion\":\"Terminar\"}") == 0);
    assert(router.targets[4] == CTRLLT_TARGET_GESPROG);
    assert(strcmp(router.requests[4],
                  "{\"servicio\":\"gesprog\",\"operacion\":\"Terminar\"}") == 0);
    assert(router.targets[5] == CTRLLT_TARGET_EJECUTOR);
    assert(strcmp(router.requests[5],
                  "{\"servicio\":\"ejecutor\",\"operacion\":\"Parar\"}") == 0);
    assert(ctrllt_service_state(&service) == CTRLLT_SERVICE_TERMINADO);

    memset(&router, 0, sizeof(router));
    router.fail = 1;
    ctrllt_service_init(&service, fake_send, &router);
    assert_response(&service, "{\"servicio\":\"gesfich\",\"operacion\":\"Crear\"}",
                    "{\"estado\":\"error\",\"mensaje\":\"error enviando solicitud al servicio\"}");

    ctrllt_service_init(&service, NULL, NULL);
    assert_response(&service, "{\"servicio\":\"gesfich\",\"operacion\":\"Crear\"}",
                    "{\"estado\":\"error\",\"mensaje\":\"servicio no conectado\"}");

    return 0;
}
