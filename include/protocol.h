#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>

#define MSG_MAX_LEN 4096

typedef enum {
    ID_FICHERO,
    ID_PROGRAMA,
    ID_EJECUCION
} id_kind_t;

int protocol_message_size_ok(size_t len);
int protocol_valid_id(const char *value, id_kind_t kind);

#endif

