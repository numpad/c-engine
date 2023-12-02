#include "message.h"

#include <stddef.h>

struct message_s message_new(uint16_t type, uint16_t size) {
	struct message_s msg;
	msg.header.type = type;
	msg.header.size = size;
	msg.body = NULL;
	return msg;
}

