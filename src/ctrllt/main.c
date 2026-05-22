#include "ctrllt_server.h"

#include <stdio.h>
#include <string.h>

static void usage(const char *program)
{
    fprintf(stderr,
            "Uso: %s -c <tuberia-cliente> [-a <tuberia-respuesta>] "
            "-f <tuberia-gesfich> [-b <respuesta-gesfich>] "
            "-p <tuberia-gesprog> [-g <respuesta-gesprog>] "
            "-e <tuberia-ejecutor> [-d <respuesta-ejecutor>]\n",
            program);
}

static int set_option(const char **target, int argc, char **argv, int *index)
{
    if (*index + 1 >= argc || argv[*index + 1][0] == '-' || *target != NULL) {
        return 0;
    }
    *target = argv[++(*index)];
    return 1;
}

static int parse_args(int argc, char **argv, ctrllt_server_config_t *config)
{
    int i;

    memset(config, 0, sizeof(*config));

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            if (!set_option(&config->client_request_pipe, argc, argv, &i)) {
                return 0;
            }
        } else if (strcmp(argv[i], "-a") == 0) {
            if (!set_option(&config->client_response_pipe, argc, argv, &i)) {
                return 0;
            }
        } else if (strcmp(argv[i], "-f") == 0) {
            if (!set_option(&config->gesfich_request_pipe, argc, argv, &i)) {
                return 0;
            }
        } else if (strcmp(argv[i], "-b") == 0) {
            if (!set_option(&config->gesfich_response_pipe, argc, argv, &i)) {
                return 0;
            }
        } else if (strcmp(argv[i], "-p") == 0) {
            if (!set_option(&config->gesprog_request_pipe, argc, argv, &i)) {
                return 0;
            }
        } else if (strcmp(argv[i], "-g") == 0) {
            if (!set_option(&config->gesprog_response_pipe, argc, argv, &i)) {
                return 0;
            }
        } else if (strcmp(argv[i], "-e") == 0) {
            if (!set_option(&config->ejecutor_request_pipe, argc, argv, &i)) {
                return 0;
            }
        } else if (strcmp(argv[i], "-d") == 0) {
            if (!set_option(&config->ejecutor_response_pipe, argc, argv, &i)) {
                return 0;
            }
        } else {
            return 0;
        }
    }

    return config->client_request_pipe != NULL && config->gesfich_request_pipe != NULL &&
           config->gesprog_request_pipe != NULL && config->ejecutor_request_pipe != NULL;
}

int main(int argc, char **argv)
{
    ctrllt_server_config_t config;

    if (!parse_args(argc, argv, &config)) {
        usage(argc > 0 ? argv[0] : "ctrllt");
        return 1;
    }

#ifndef _WIN32
    if (config.client_response_pipe == NULL || config.gesfich_response_pipe == NULL ||
        config.gesprog_response_pipe == NULL || config.ejecutor_response_pipe == NULL) {
        fprintf(stderr, "Error: en POSIX se requieren tuberias de respuesta.\n");
        usage(argc > 0 ? argv[0] : "ctrllt");
        return 1;
    }
#endif

    return ctrllt_server_run(&config) ? 0 : 1;
}
