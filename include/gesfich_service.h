#ifndef GESFICH_SERVICE_H
#define GESFICH_SERVICE_H

#include <stddef.h>

int gesfich_handle_json(const char *aralmac, const char *request, char *response,
                        size_t response_size);

#endif

