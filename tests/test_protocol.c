#include "protocol.h"

#include <assert.h>

int main(void)
{
    assert(protocol_message_size_ok(1));
    assert(protocol_message_size_ok(MSG_MAX_LEN));
    assert(!protocol_message_size_ok(0));
    assert(!protocol_message_size_ok(MSG_MAX_LEN + 1));

    assert(protocol_valid_id("f-0001", ID_FICHERO));
    assert(protocol_valid_id("p-1234", ID_PROGRAMA));
    assert(protocol_valid_id("e-9999", ID_EJECUCION));

    assert(!protocol_valid_id("l-0001", ID_EJECUCION));
    assert(!protocol_valid_id("f-001", ID_FICHERO));
    assert(!protocol_valid_id("f-00a1", ID_FICHERO));
    assert(!protocol_valid_id("p-0001", ID_FICHERO));
    assert(!protocol_valid_id(0, ID_PROGRAMA));

    return 0;
}

