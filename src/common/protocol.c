#include "protocol.h"

#include <ctype.h>
#include <string.h>

int protocol_message_size_ok(size_t len)
{
    return len > 0 && len <= MSG_MAX_LEN;
}

int protocol_valid_id(const char *value, id_kind_t kind)
{
    char prefix;
    size_t i;

    if (value == NULL || strlen(value) != 6 || value[1] != '-') {
        return 0;
    }

    switch (kind) {
    case ID_FICHERO:
        prefix = 'f';
        break;
    case ID_PROGRAMA:
        prefix = 'p';
        break;
    case ID_EJECUCION:
        prefix = 'e';
        break;
    default:
        return 0;
    }

    if (value[0] != prefix) {
        return 0;
    }

    for (i = 2; i < 6; i++) {
        if (!isdigit((unsigned char)value[i])) {
            return 0;
        }
    }

    return 1;
}

