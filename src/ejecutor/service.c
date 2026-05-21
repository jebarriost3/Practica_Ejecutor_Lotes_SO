#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "ejecutor_service.h"

#include "ejecutor_store.h"
#include "gesprog_store.h"
#include "protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#define FIELD_SIZE 512

static int write_json(char *response, size_t size, const char *format, ...)
{
    int written;
    va_list args;

    if (response == NULL || size == 0) {
        return 0;
    }

    va_start(args, format);
    written = vsnprintf(response, size, format, args);
    va_end(args);

    return written >= 0 && (size_t)written < size;
}

static int json_get_string(const char *json, const char *key, char *out, size_t out_size)
{
    char pattern[64];
    const char *p;
    const char *start;
    const char *end;
    size_t len;

    if (json == NULL || key == NULL || out == NULL || out_size == 0) {
        return 0;
    }
    if (snprintf(pattern, sizeof(pattern), "\"%s\"", key) >= (int)sizeof(pattern)) {
        return 0;
    }

    p = strstr(json, pattern);
    p = p == NULL ? NULL : strchr(p + strlen(pattern), ':');
    start = p == NULL ? NULL : strchr(p + 1, '"');
    if (start == NULL) {
        return 0;
    }
    start++;

    end = start;
    while (*end != '\0' && !(*end == '"' && (end == start || *(end - 1) != '\\'))) {
        end++;
    }
    if (*end != '"') {
        return 0;
    }

    len = (size_t)(end - start);
    if (len >= out_size) {
        return 0;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    return 1;
}

static int write_error(char *response, size_t size, const char *message)
{
    return write_json(response, size, "{\"estado\":\"error\",\"mensaje\":\"%s\"}", message);
}

static int build_path(char *buffer, size_t size, const char *a, const char *b,
                      const char *c)
{
    int written = snprintf(buffer, size, "%s/%s/%s", a, b, c);
    return written > 0 && (size_t)written < size;
}

static int execution_file(char *buffer, size_t size, const char *aralmac,
                          const char *id_ejecucion, const char *ext)
{
    char filename[32];
    if (snprintf(filename, sizeof(filename), "%s%s", id_ejecucion, ext) >=
        (int)sizeof(filename)) {
        return 0;
    }
    return build_path(buffer, size, aralmac, "ejecuciones", filename);
}

static const char *error_message(ejecutor_result_t result)
{
    if (result == EJECUTOR_NOT_FOUND) {
        return "ejecucion no encontrada";
    }
    if (result == EJECUTOR_INVALID_ARG) {
        return "identificador invalido";
    }
    if (result == EJECUTOR_STORAGE_ERROR) {
        return "error de almacenamiento";
    }
    return "operacion desconocida";
}

static int append_execution(char *response, size_t size, size_t *used,
                            const ejecutor_execution_t *item)
{
    int written = snprintf(response + *used, size - *used,
                           "{\"id-ejecucion\":\"%s\",\"id-programa\":\"%s\","
                           "\"proceso-estado\":\"%s\"",
                           item->id_ejecucion, item->id_programa,
                           ejecutor_estado_texto(item->estado));

    if (written < 0 || (size_t)written >= size - *used) {
        return 0;
    }
    *used += (size_t)written;

    if (item->tiene_codigo_salida) {
        written = snprintf(response + *used, size - *used, ",\"codigo-salida\":%d",
                           item->codigo_salida);
        if (written < 0 || (size_t)written >= size - *used) {
            return 0;
        }
        *used += (size_t)written;
    }

    written = snprintf(response + *used, size - *used, "}");
    if (written < 0 || (size_t)written >= size - *used) {
        return 0;
    }
    *used += (size_t)written;
    return 1;
}

static int handle_ejecutar(const char *aralmac, const char *request, char *response,
                           size_t size)
{
    char id_programa[FIELD_SIZE];
    char id_ejecucion[EJECUTOR_ID_SIZE];
    ejecutor_result_t result;

    if (!json_get_string(request, "id-programa", id_programa, sizeof(id_programa))) {
        return write_error(response, size, "falta campo: id-programa");
    }

    result = ejecutor_registrar(aralmac, id_programa, id_ejecucion);
    if (result != EJECUTOR_OK) {
        return write_error(response, size, error_message(result));
    }

    return write_json(response, size, "{\"estado\":\"ok\",\"id-ejecucion\":\"%s\"}",
                      id_ejecucion);
}

static ejecutor_process_t *find_process(ejecutor_service_t *service,
                                        const char *id_ejecucion)
{
    size_t i;
    for (i = 0; i < EJECUTOR_MAX_PROCESOS; i++) {
        if (service->processes[i].active &&
            strcmp(service->processes[i].id_ejecucion, id_ejecucion) == 0) {
            return &service->processes[i];
        }
    }
    return NULL;
}

static ejecutor_process_t *alloc_process(ejecutor_service_t *service)
{
    size_t i;
    for (i = 0; i < EJECUTOR_MAX_PROCESOS; i++) {
        if (!service->processes[i].active) {
            return &service->processes[i];
        }
    }
    return NULL;
}

static void update_process_state(const char *aralmac, ejecutor_process_t *process)
{
#ifdef _WIN32
    DWORD exit_code;
    if (process == NULL || !process->active) {
        return;
    }
    if (!GetExitCodeProcess(process->process, &exit_code) || exit_code == STILL_ACTIVE) {
        return;
    }
    ejecutor_marcar_terminada(aralmac, process->id_ejecucion, (int)exit_code);
    CloseHandle(process->process);
    CloseHandle(process->thread);
    process->active = 0;
#else
    int status;
    pid_t result;
    int code = 1;

    if (process == NULL || !process->active) {
        return;
    }
    result = waitpid(process->pid, &status, WNOHANG);
    if (result <= 0) {
        return;
    }
    if (WIFEXITED(status)) {
        code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        code = 128 + WTERMSIG(status);
    }
    ejecutor_marcar_terminada(aralmac, process->id_ejecucion, code);
    process->active = 0;
#endif
}

static void update_all_processes(ejecutor_service_t *service, const char *aralmac)
{
    size_t i;
    for (i = 0; i < EJECUTOR_MAX_PROCESOS; i++) {
        update_process_state(aralmac, &service->processes[i]);
    }
}

static int launch_program(ejecutor_service_t *service, const char *aralmac,
                          const char *id_ejecucion, const gesprog_program_t *programa)
{
    ejecutor_process_t *slot = alloc_process(service);
    char out_path[512];
    char err_path[512];

    if (slot == NULL ||
        !execution_file(out_path, sizeof(out_path), aralmac, id_ejecucion, ".out") ||
        !execution_file(err_path, sizeof(err_path), aralmac, id_ejecucion, ".err")) {
        return 0;
    }

#ifdef _WIN32
    {
        SECURITY_ATTRIBUTES sa;
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        HANDLE input_file;
        HANDLE out_file;
        HANDLE err_file;
        char command[2048];
        size_t i;
        int written;

        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;

        input_file = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL, NULL);
        out_file = CreateFileA(out_path, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
        err_file = CreateFileA(err_path, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
        if (input_file == INVALID_HANDLE_VALUE || out_file == INVALID_HANDLE_VALUE ||
            err_file == INVALID_HANDLE_VALUE) {
            if (input_file != INVALID_HANDLE_VALUE) {
                CloseHandle(input_file);
            }
            if (out_file != INVALID_HANDLE_VALUE) {
                CloseHandle(out_file);
            }
            if (err_file != INVALID_HANDLE_VALUE) {
                CloseHandle(err_file);
            }
            return 0;
        }

        written = snprintf(command, sizeof(command), "cmd.exe /C call \"%s\"",
                           programa->ejecutable);
        if (written < 0 || (size_t)written >= sizeof(command)) {
            CloseHandle(input_file);
            CloseHandle(out_file);
            CloseHandle(err_file);
            return 0;
        }
        for (i = 0; i < programa->args_count; i++) {
            written += snprintf(command + written, sizeof(command) - (size_t)written,
                                " \"%s\"", programa->args[i]);
            if (written < 0 || (size_t)written >= sizeof(command) - 1) {
                CloseHandle(input_file);
                CloseHandle(out_file);
                CloseHandle(err_file);
                return 0;
            }
        }
        memset(&si, 0, sizeof(si));
        memset(&pi, 0, sizeof(pi));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = input_file;
        si.hStdOutput = out_file;
        si.hStdError = err_file;

        if (!CreateProcessA(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL,
                            &si, &pi)) {
            CloseHandle(input_file);
            CloseHandle(out_file);
            CloseHandle(err_file);
            return 0;
        }

        CloseHandle(input_file);
        CloseHandle(out_file);
        CloseHandle(err_file);
        strcpy(slot->id_ejecucion, id_ejecucion);
        slot->process = pi.hProcess;
        slot->thread = pi.hThread;
        slot->active = 1;
        return 1;
    }
#else
    {
        pid_t pid = fork();
        if (pid < 0) {
            return 0;
        }
        if (pid == 0) {
            FILE *out = freopen(out_path, "wb", stdout);
            FILE *err = freopen(err_path, "wb", stderr);
            char **argv = calloc(programa->args_count + 3, sizeof(char *));
            size_t i;

            if (out == NULL || err == NULL || argv == NULL) {
                _exit(127);
            }
            argv[0] = "sh";
            argv[1] = programa->ejecutable;
            for (i = 0; i < programa->args_count; i++) {
                argv[i + 2] = programa->args[i];
            }
            argv[programa->args_count + 2] = NULL;
            execv("/bin/sh", argv);
            _exit(127);
        }
        strcpy(slot->id_ejecucion, id_ejecucion);
        slot->pid = pid;
        slot->active = 1;
        return 1;
    }
#endif
}

static int handle_ejecutar_real(ejecutor_service_t *service, const char *aralmac,
                                const char *request, char *response, size_t size)
{
    char id_programa[FIELD_SIZE];
    char id_ejecucion[EJECUTOR_ID_SIZE];
    gesprog_program_t programa;
    ejecutor_result_t result;

    if (!json_get_string(request, "id-programa", id_programa, sizeof(id_programa))) {
        return write_error(response, size, "falta campo: id-programa");
    }
    if (gesprog_leer(aralmac, id_programa, &programa) != GESPROG_OK) {
        return write_error(response, size, "programa no encontrado");
    }

    result = ejecutor_registrar(aralmac, id_programa, id_ejecucion);
    if (result != EJECUTOR_OK) {
        gesprog_liberar_programa(&programa);
        return write_error(response, size, error_message(result));
    }
    if (!launch_program(service, aralmac, id_ejecucion, &programa)) {
        ejecutor_marcar_terminada(aralmac, id_ejecucion, 127);
        gesprog_liberar_programa(&programa);
        return write_error(response, size, "no se pudo lanzar el programa");
    }

    gesprog_liberar_programa(&programa);
    return write_json(response, size, "{\"estado\":\"ok\",\"id-ejecucion\":\"%s\"}",
                      id_ejecucion);
}

static int handle_matar(ejecutor_service_t *service, const char *aralmac,
                        const char *request, char *response, size_t size)
{
    char id[FIELD_SIZE];
    ejecutor_process_t *process;

    if (!json_get_string(request, "id-ejecucion", id, sizeof(id))) {
        return write_error(response, size, "falta campo: id-ejecucion");
    }
    process = find_process(service, id);
    if (process == NULL) {
        return write_error(response, size, "ejecucion no encontrada");
    }

#ifdef _WIN32
    TerminateProcess(process->process, 1);
    WaitForSingleObject(process->process, INFINITE);
    CloseHandle(process->process);
    CloseHandle(process->thread);
#else
    kill(process->pid, SIGTERM);
    waitpid(process->pid, NULL, 0);
#endif
    ejecutor_marcar_terminada(aralmac, id, 1);
    process->active = 0;
    return write_json(response, size, "{\"estado\":\"ok\"}");
}

static int handle_estado(const char *aralmac, const char *request, char *response,
                         size_t size)
{
    char id[FIELD_SIZE];
    ejecutor_execution_t item;
    ejecutor_execution_t *items = NULL;
    size_t count = 0;
    size_t used;
    size_t i;
    ejecutor_result_t result;

    if (json_get_string(request, "id-ejecucion", id, sizeof(id))) {
        result = ejecutor_leer(aralmac, id, &item);
        if (result != EJECUTOR_OK) {
            return write_error(response, size, error_message(result));
        }
        used = (size_t)snprintf(response, size, "{\"estado\":\"ok\",\"ejecucion\":");
        return used < size && append_execution(response, size, &used, &item) &&
               write_json(response + used, size - used, "}");
    }

    result = ejecutor_listar(aralmac, &items, &count);
    if (result != EJECUTOR_OK) {
        return write_error(response, size, "error al listar ejecuciones");
    }

    used = (size_t)snprintf(response, size, "{\"estado\":\"ok\",\"ejecuciones\":[");
    if (used >= size) {
        ejecutor_liberar_lista(items);
        return 0;
    }

    for (i = 0; i < count; i++) {
        if (i > 0 && !write_json(response + used, size - used, ",")) {
            ejecutor_liberar_lista(items);
            return 0;
        }
        if (i > 0) {
            used++;
        }
        if (!append_execution(response, size, &used, &items[i])) {
            ejecutor_liberar_lista(items);
            return 0;
        }
    }

    ejecutor_liberar_lista(items);
    return write_json(response + used, size - used, "]}");
}

int ejecutor_handle_json(const char *aralmac, const char *request, char *response,
                         size_t response_size)
{
    char servicio[FIELD_SIZE];
    char operacion[FIELD_SIZE];

    if (response == NULL || response_size == 0) {
        return 0;
    }
    response[0] = '\0';

    if (request == NULL || !protocol_message_size_ok(strlen(request)) ||
        !json_get_string(request, "servicio", servicio, sizeof(servicio)) ||
        strcmp(servicio, "ejecutor") != 0 ||
        !json_get_string(request, "operacion", operacion, sizeof(operacion))) {
        return write_error(response, response_size, "operacion desconocida");
    }

    if (strcmp(operacion, "Ejecutar") == 0) {
        return handle_ejecutar(aralmac, request, response, response_size);
    }
    if (strcmp(operacion, "Estado") == 0) {
        return handle_estado(aralmac, request, response, response_size);
    }
    if (strcmp(operacion, "Suspender") == 0 || strcmp(operacion, "Reasumir") == 0 ||
        strcmp(operacion, "Parar") == 0) {
        return write_json(response, response_size, "{\"estado\":\"ok\"}");
    }

    return write_error(response, response_size, "operacion desconocida");
}

void ejecutor_service_init(ejecutor_service_t *service)
{
    if (service != NULL) {
        memset(service, 0, sizeof(*service));
        service->state = EJECUTOR_SERVICE_CORRIENDO;
    }
}

ejecutor_service_state_t ejecutor_service_state(const ejecutor_service_t *service)
{
    return service == NULL ? EJECUTOR_SERVICE_TERMINADO : service->state;
}

int ejecutor_service_handle_json(ejecutor_service_t *service, const char *aralmac,
                                 const char *request, char *response,
                                 size_t response_size)
{
    char operacion[FIELD_SIZE];

    if (service == NULL) {
        return write_error(response, response_size, "servicio no inicializado");
    }
    if (!json_get_string(request, "operacion", operacion, sizeof(operacion))) {
        return ejecutor_handle_json(aralmac, request, response, response_size);
    }

    update_all_processes(service, aralmac);

    if (strcmp(operacion, "Suspender") == 0) {
        service->state = EJECUTOR_SERVICE_SUSPENDIDO;
        return write_json(response, response_size, "{\"estado\":\"ok\"}");
    }
    if (strcmp(operacion, "Reasumir") == 0) {
        service->state = EJECUTOR_SERVICE_CORRIENDO;
        return write_json(response, response_size, "{\"estado\":\"ok\"}");
    }
    if (strcmp(operacion, "Parar") == 0) {
        size_t i;
        for (i = 0; i < EJECUTOR_MAX_PROCESOS; i++) {
            if (service->processes[i].active) {
#ifdef _WIN32
                TerminateProcess(service->processes[i].process, 1);
                WaitForSingleObject(service->processes[i].process, INFINITE);
                CloseHandle(service->processes[i].process);
                CloseHandle(service->processes[i].thread);
#else
                kill(service->processes[i].pid, SIGTERM);
                waitpid(service->processes[i].pid, NULL, 0);
#endif
                ejecutor_marcar_terminada(aralmac, service->processes[i].id_ejecucion, 1);
                service->processes[i].active = 0;
            }
        }
        service->state = EJECUTOR_SERVICE_TERMINADO;
        return write_json(response, response_size, "{\"estado\":\"ok\"}");
    }
    if (service->state == EJECUTOR_SERVICE_TERMINADO) {
        return write_error(response, response_size, "servicio terminado");
    }
    if (service->state == EJECUTOR_SERVICE_SUSPENDIDO &&
        strcmp(operacion, "Ejecutar") == 0) {
        return write_error(response, response_size, "servicio suspendido");
    }

    if (strcmp(operacion, "Ejecutar") == 0) {
        return handle_ejecutar_real(service, aralmac, request, response, response_size);
    }
    if (strcmp(operacion, "Matar") == 0) {
        return handle_matar(service, aralmac, request, response, response_size);
    }

    return ejecutor_handle_json(aralmac, request, response, response_size);
}
