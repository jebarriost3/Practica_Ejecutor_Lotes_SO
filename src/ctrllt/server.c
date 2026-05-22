#include "ctrllt_server.h"

#include "ctrllt_service.h"
#include "protocol.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

typedef struct {
    const char *request_pipe;
    const char *response_pipe;
#ifdef _WIN32
    HANDLE handle;
#endif
} ctrllt_endpoint_t;

typedef struct {
    ctrllt_endpoint_t gesfich;
    ctrllt_endpoint_t gesprog;
    ctrllt_endpoint_t ejecutor;
} ctrllt_router_t;

typedef struct {
#ifdef _WIN32
    HANDLE input;
    HANDLE output;
    int split;
#else
    int input;
    int output;
    const char *request_pipe;
    const char *response_pipe;
#endif
} ctrllt_pipe_t;

static int write_error(char *response, size_t size, const char *message)
{
    int written = snprintf(response, size, "{\"estado\":\"error\",\"mensaje\":\"%s\"}",
                           message);
    return written >= 0 && (size_t)written < size;
}

#ifdef _WIN32
static int connect_pipe(HANDLE pipe)
{
    return ConnectNamedPipe(pipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED;
}

static int open_server_pipe(ctrllt_pipe_t *pipe, const ctrllt_server_config_t *config)
{
    DWORD access =
        config->client_response_pipe == NULL ? PIPE_ACCESS_DUPLEX : PIPE_ACCESS_INBOUND;

    pipe->split = config->client_response_pipe != NULL;
    pipe->input = CreateNamedPipeA(config->client_request_pipe, access,
                                   PIPE_TYPE_BYTE | PIPE_WAIT, 1, MSG_MAX_LEN + 2,
                                   MSG_MAX_LEN + 2, 0, NULL);
    if (pipe->input == INVALID_HANDLE_VALUE) {
        return 0;
    }

    if (pipe->split) {
        pipe->output = CreateNamedPipeA(config->client_response_pipe, PIPE_ACCESS_OUTBOUND,
                                        PIPE_TYPE_BYTE | PIPE_WAIT, 1, MSG_MAX_LEN + 2,
                                        MSG_MAX_LEN + 2, 0, NULL);
        if (pipe->output == INVALID_HANDLE_VALUE) {
            CloseHandle(pipe->input);
            return 0;
        }
    } else {
        pipe->output = pipe->input;
    }

    return connect_pipe(pipe->input) && (!pipe->split || connect_pipe(pipe->output));
}

static int read_char(ctrllt_pipe_t *pipe, char *ch)
{
    DWORD count = 0;
    return ReadFile(pipe->input, ch, 1, &count, NULL) && count == 1;
}

static int write_all_handle(HANDLE handle, const char *text, size_t len)
{
    DWORD count = 0;
    return WriteFile(handle, text, (DWORD)len, &count, NULL) && count == len;
}

static int write_all(ctrllt_pipe_t *pipe, const char *text, size_t len)
{
    return write_all_handle(pipe->output, text, len);
}

static void flush_pipe(ctrllt_pipe_t *pipe)
{
    FlushFileBuffers(pipe->output);
}

static void close_server_pipe(ctrllt_pipe_t *pipe)
{
    flush_pipe(pipe);
    DisconnectNamedPipe(pipe->input);
    CloseHandle(pipe->input);
    if (pipe->split) {
        DisconnectNamedPipe(pipe->output);
        CloseHandle(pipe->output);
    }
}

static int open_service_endpoint(ctrllt_endpoint_t *endpoint)
{
    endpoint->handle = CreateFileA(endpoint->request_pipe, GENERIC_READ | GENERIC_WRITE, 0,
                                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    return endpoint->handle != INVALID_HANDLE_VALUE;
}

static int open_service_endpoints(ctrllt_router_t *router)
{
    if (!open_service_endpoint(&router->gesfich)) {
        return 0;
    }
    if (!open_service_endpoint(&router->gesprog)) {
        CloseHandle(router->gesfich.handle);
        return 0;
    }
    if (!open_service_endpoint(&router->ejecutor)) {
        CloseHandle(router->gesfich.handle);
        CloseHandle(router->gesprog.handle);
        return 0;
    }
    return 1;
}

static void close_service_endpoints(ctrllt_router_t *router)
{
    CloseHandle(router->gesfich.handle);
    CloseHandle(router->gesprog.handle);
    CloseHandle(router->ejecutor.handle);
}

static int service_send_message(const ctrllt_endpoint_t *endpoint, const char *request,
                                char *response, size_t response_size)
{
    char ch;
    size_t used = 0;

    if (endpoint->handle == INVALID_HANDLE_VALUE) {
        return 0;
    }

    if (!write_all_handle(endpoint->handle, request, strlen(request)) ||
        !write_all_handle(endpoint->handle, "\n", 1)) {
        return 0;
    }
    FlushFileBuffers(endpoint->handle);

    while (used + 1 < response_size) {
        DWORD count = 0;
        if (!ReadFile(endpoint->handle, &ch, 1, &count, NULL) || count != 1) {
            return 0;
        }
        if (ch == '\n') {
            if (used > 0 && response[used - 1] == '\r') {
                used--;
            }
            response[used] = '\0';
            return 1;
        }
        response[used++] = ch;
    }

    return 0;
}
#else
static int open_server_pipe(ctrllt_pipe_t *pipe, const ctrllt_server_config_t *config)
{
    pipe->request_pipe = config->client_request_pipe;
    pipe->response_pipe = config->client_response_pipe;

    if ((mkfifo(config->client_request_pipe, 0666) != 0 && errno != EEXIST) ||
        (mkfifo(config->client_response_pipe, 0666) != 0 && errno != EEXIST)) {
        return 0;
    }

    pipe->input = open(config->client_request_pipe, O_RDWR);
    if (pipe->input < 0) {
        return 0;
    }
    pipe->output = open(config->client_response_pipe, O_WRONLY);
    if (pipe->output < 0) {
        close(pipe->input);
        return 0;
    }
    return 1;
}

static int read_char(ctrllt_pipe_t *pipe, char *ch)
{
    return read(pipe->input, ch, 1) == 1;
}

static int write_all_fd(int fd, const char *text, size_t len)
{
    size_t used = 0;

    while (used < len) {
        ssize_t written = write(fd, text + used, len - used);
        if (written <= 0) {
            return 0;
        }
        used += (size_t)written;
    }
    return 1;
}

static int write_all(ctrllt_pipe_t *pipe, const char *text, size_t len)
{
    return write_all_fd(pipe->output, text, len);
}

static void flush_pipe(ctrllt_pipe_t *pipe)
{
    (void)pipe;
}

static void close_server_pipe(ctrllt_pipe_t *pipe)
{
    close(pipe->input);
    close(pipe->output);
    unlink(pipe->request_pipe);
    unlink(pipe->response_pipe);
}

static int service_send_message(const ctrllt_endpoint_t *endpoint, const char *request,
                                char *response, size_t response_size)
{
    int output;
    int input;
    char ch;
    size_t used = 0;

    input = open(endpoint->response_pipe, O_RDWR);
    if (input < 0) {
        return 0;
    }
    output = open(endpoint->request_pipe, O_WRONLY);
    if (output < 0) {
        close(input);
        return 0;
    }

    if (!write_all_fd(output, request, strlen(request)) || !write_all_fd(output, "\n", 1)) {
        close(output);
        close(input);
        return 0;
    }
    close(output);

    while (used + 1 < response_size) {
        if (read(input, &ch, 1) != 1) {
            close(input);
            return 0;
        }
        if (ch == '\n') {
            if (used > 0 && response[used - 1] == '\r') {
                used--;
            }
            response[used] = '\0';
            close(input);
            return 1;
        }
        response[used++] = ch;
    }

    close(input);
    return 0;
}

static int open_service_endpoints(ctrllt_router_t *router)
{
    (void)router;
    return 1;
}

static void close_service_endpoints(ctrllt_router_t *router)
{
    (void)router;
}
#endif

static int read_message(ctrllt_pipe_t *pipe, char *buffer, size_t size)
{
    size_t used = 0;
    int too_long = 0;

    for (;;) {
        char ch;

        if (!read_char(pipe, &ch)) {
            return used == 0 ? 0 : -1;
        }
        if (ch == '\n') {
            if (too_long) {
                return -2;
            }
            if (used > 0 && buffer[used - 1] == '\r') {
                used--;
            }
            buffer[used] = '\0';
            return used == 0 ? -3 : 1;
        }
        if (used + 1 >= size || used >= MSG_MAX_LEN) {
            too_long = 1;
        } else if (!too_long) {
            buffer[used++] = ch;
        }
    }
}

static int write_message(ctrllt_pipe_t *pipe, const char *message)
{
    if (!write_all(pipe, message, strlen(message)) || !write_all(pipe, "\n", 1)) {
        return 0;
    }
    flush_pipe(pipe);
    return 1;
}

static const ctrllt_endpoint_t *endpoint_for_target(const ctrllt_router_t *router,
                                                    ctrllt_target_t target)
{
    switch (target) {
    case CTRLLT_TARGET_GESFICH:
        return &router->gesfich;
    case CTRLLT_TARGET_GESPROG:
        return &router->gesprog;
    case CTRLLT_TARGET_EJECUTOR:
        return &router->ejecutor;
    default:
        return NULL;
    }
}

static int router_send(ctrllt_target_t target, const char *request, char *response,
                       size_t response_size, void *user_data)
{
    const ctrllt_router_t *router = user_data;
    const ctrllt_endpoint_t *endpoint = endpoint_for_target(router, target);

    if (endpoint == NULL || endpoint->request_pipe == NULL) {
        return 0;
    }
#ifndef _WIN32
    if (endpoint->response_pipe == NULL) {
        return 0;
    }
#endif
    return service_send_message(endpoint, request, response, response_size);
}

int ctrllt_server_run(const ctrllt_server_config_t *config)
{
    ctrllt_pipe_t pipe;
    ctrllt_router_t router;
    ctrllt_service_t service;
    char request[MSG_MAX_LEN + 1];
    char response[MSG_MAX_LEN + 1];

    memset(&router, 0, sizeof(router));
    router.gesfich.request_pipe = config->gesfich_request_pipe;
    router.gesfich.response_pipe = config->gesfich_response_pipe;
    router.gesprog.request_pipe = config->gesprog_request_pipe;
    router.gesprog.response_pipe = config->gesprog_response_pipe;
    router.ejecutor.request_pipe = config->ejecutor_request_pipe;
    router.ejecutor.response_pipe = config->ejecutor_response_pipe;
#ifdef _WIN32
    router.gesfich.handle = INVALID_HANDLE_VALUE;
    router.gesprog.handle = INVALID_HANDLE_VALUE;
    router.ejecutor.handle = INVALID_HANDLE_VALUE;
#endif

    if (!open_server_pipe(&pipe, config)) {
        fprintf(stderr, "Error al abrir tuberia nombrada de ctrllt.\n");
        return 0;
    }
    if (!open_service_endpoints(&router)) {
        fprintf(stderr, "Error al conectar ctrllt con servicios internos.\n");
        close_server_pipe(&pipe);
        return 0;
    }

    ctrllt_service_init(&service, router_send, &router);

    while (ctrllt_service_state(&service) != CTRLLT_SERVICE_TERMINADO) {
        int result = read_message(&pipe, request, sizeof(request));

        if (result == 0) {
            break;
        }
        if (result == -2) {
            write_error(response, sizeof(response), "mensaje demasiado largo");
        } else if (result == -3) {
            write_error(response, sizeof(response), "mensaje vacio");
        } else if (result < 0) {
            write_error(response, sizeof(response), "error de comunicacion");
        } else if (!ctrllt_service_handle_json(&service, request, response,
                                               sizeof(response))) {
            write_error(response, sizeof(response), "respuesta demasiado larga");
        }

        if (!write_message(&pipe, response)) {
            break;
        }
    }

    close_service_endpoints(&router);
    close_server_pipe(&pipe);
    return 1;
}
