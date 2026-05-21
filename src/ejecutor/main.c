#include "ejecutor_server.h"

#include <stdio.h>
#include <string.h>

static void usage(const char *program)
{
    fprintf(stderr, "Uso: %s -e <tuberia-nombrada> [-d <tuberia-nombrada>] "
                    "-x <info-aralmac>\n",
            program);
}

static int parse_args(int argc, char **argv, ejecutor_server_config_t *config)
{
    int i;

    config->request_pipe = NULL;
    config->response_pipe = NULL;
    config->aralmac = NULL;

    for (i = 1; i < argc; i++) {
        const char **target = NULL;

        if (strcmp(argv[i], "-e") == 0) {
            target = &config->request_pipe;
        } else if (strcmp(argv[i], "-d") == 0) {
            target = &config->response_pipe;
        } else if (strcmp(argv[i], "-x") == 0) {
            target = &config->aralmac;
        } else {
            return 0;
        }

        if (i + 1 >= argc || argv[i + 1][0] == '-' || *target != NULL) {
            return 0;
        }
        *target = argv[++i];
    }

    return config->request_pipe != NULL && config->aralmac != NULL;
}

int main(int argc, char **argv)
{
    ejecutor_server_config_t config;

    if (!parse_args(argc, argv, &config)) {
        usage(argc > 0 ? argv[0] : "ejecutor");
        return 1;
    }

#ifndef _WIN32
    if (config.response_pipe == NULL) {
        fprintf(stderr, "Error: en POSIX se requiere -d <tuberia-respuesta>.\n");
        usage(argc > 0 ? argv[0] : "ejecutor");
        return 1;
    }
#endif

    return ejecutor_server_run(&config) ? 0 : 1;
}
