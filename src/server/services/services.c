#include "services.h"

#include "server/gameserver.h"
#include "net/message.h"

// include all sub-services
#include "group_service.c"

void services_dispatcher(struct gameserver *gs, struct message_header *message, struct session *session) {
	switch (message->type) {
		case LOBBY_CREATE_REQUEST:
			group_service_create_lobby(gs, (struct lobby_create_request *)message, session);
			break;
		case LOBBY_JOIN_REQUEST:
			group_service_join_lobby(gs, (struct lobby_join_request *)message, session);
			break;
		case LOBBY_LIST_REQUEST: {
			group_service_list_lobbies(gs, (struct lobby_list_request *)message, session);
			break;
		}
		case LOBBY_CREATE_RESPONSE:
		case LOBBY_JOIN_RESPONSE:
		case LOBBY_LIST_RESPONSE:
			// these messages we cant handle
		case MSG_TYPE_UNKNOWN:
		case MSG_TYPE_MAX:
			// these messages are invalid
			break;
	}
}

