#include "gesfich_server.h"

#include "gesfich_service.h"
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
} gesfich_pipe_t;

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

static int open_pipe(gesfich_pipe_t *pipe, const gesfich_server_config_t *config)
{
    DWORD access = config->response_pipe == NULL ? PIPE_ACCESS_DUPLEX : PIPE_ACCESS_INBOUND;

    pipe->split = config->response_pipe != NULL;
    pipe->input = CreateNamedPipeA(config->request_pipe, access, PIPE_TYPE_BYTE | PIPE_WAIT,
                                   1, MSG_MAX_LEN + 2, MSG_MAX_LEN + 2, 0, NULL);
    if (pipe->input == INVALID_HANDLE_VALUE) {
        return 0;
    }

    if (pipe->split) {
        pipe->output = CreateNamedPipeA(config->response_pipe, PIPE_ACCESS_OUTBOUND,
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

static int read_char(gesfich_pipe_t *pipe, char *ch)
{
    DWORD count = 0;
    return ReadFile(pipe->input, ch, 1, &count, NULL) && count == 1;
}

static int write_all(gesfich_pipe_t *pipe, const char *text, size_t len)
{
    DWORD count = 0;
    return WriteFile(pipe->output, text, (DWORD)len, &count, NULL) && count == len;
}

static void close_pipe(gesfich_pipe_t *pipe)
{
    DisconnectNamedPipe(pipe->input);
    CloseHandle(pipe->input);
    if (pipe->split) {
        DisconnectNamedPipe(pipe->output);
        CloseHandle(pipe->output);
    }
}
#else
static int open_pipe(gesfich_pipe_t *pipe, const gesfich_server_config_t *config)
{
    pipe->request_pipe = config->request_pipe;
    pipe->response_pipe = config->response_pipe;

    if ((mkfifo(config->request_pipe, 0666) != 0 && errno != EEXIST) ||
        (mkfifo(config->response_pipe, 0666) != 0 && errno != EEXIST)) {
        return 0;
    }

    pipe->input = open(config->request_pipe, O_RDONLY);
    if (pipe->input < 0) {
        return 0;
    }
    pipe->output = open(config->response_pipe, O_WRONLY);
    if (pipe->output < 0) {
        close(pipe->input);
        return 0;
    }
    return 1;
}

static int read_char(gesfich_pipe_t *pipe, char *ch)
{
    return read(pipe->input, ch, 1) == 1;
}

static int write_all(gesfich_pipe_t *pipe, const char *text, size_t len)
{
    return write(pipe->output, text, len) == (ssize_t)len;
}

static void close_pipe(gesfich_pipe_t *pipe)
{
    close(pipe->input);
    close(pipe->output);
    unlink(pipe->request_pipe);
    unlink(pipe->response_pipe);
}
#endif

static int read_message(gesfich_pipe_t *pipe, char *buffer, size_t size)
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

static int write_message(gesfich_pipe_t *pipe, const char *message)
{
    return write_all(pipe, message, strlen(message)) && write_all(pipe, "\n", 1);
}

int gesfich_server_run(const gesfich_server_config_t *config)
{
    gesfich_pipe_t pipe;
    gesfich_service_t service;
    char request[MSG_MAX_LEN + 1];
    char response[MSG_MAX_LEN + 1];

    if (!open_pipe(&pipe, config)) {
        fprintf(stderr, "Error al abrir tuberia nombrada.\n");
        return 0;
    }

    gesfich_service_init(&service);

    while (gesfich_service_state(&service) != GESFICH_SERVICE_TERMINADO) {
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
        } else if (!gesfich_service_handle_json(&service, config->aralmac, request,
                                                response, sizeof(response))) {
            write_error(response, sizeof(response), "respuesta demasiado larga");
        }

        if (!write_message(&pipe, response)) {
            break;
        }
    }

    close_pipe(&pipe);
    return 1;
}
