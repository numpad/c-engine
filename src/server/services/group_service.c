
#include <assert.h>
#include "server/gameserver.h"
#include "net/message.h"
#include "server/errors.h"
#include <stb_ds.h>

/* Creates a new lobby and moves `requested_by` into it.
 * @param msg The requests data.
 * @param session The session which requested this. Can be NULL, if the server creates the lobby for example.
 */
void group_service_create_lobby(struct gameserver *gserver, struct lobby_create_request *msg, struct session *requested_by) {
	assert(msg != NULL);
	assert(requested_by != NULL); // TODO: also support no session
	assert(msg->lobby_id >= 0); // TODO: this is application logic...

	struct lobby_create_response response;
	message_header_init(&response.header, LOBBY_CREATE_RESPONSE);
	response.lobby_id = msg->lobby_id;
	response.create_error = SERVER_NO_ERROR;

	if (requested_by->group_id > 0) {
		//messagequeue_add("#%06d is already in lobby %d but requested to create %d", requested_by->id, requested_by->group_id, msg->lobby_id);
		response.create_error = SERVER_ERROR_ALREADY_IN_A_LOBBY;
		gameserver_send_to(gserver, &response.header, requested_by);

		return;
	}

	// Check if lobby already exists.
	for (int i = 0; i < stbds_arrlen(gserver->sessions); ++i) {
		struct session *s = gserver->sessions[i];
		if ((uint32_t)msg->lobby_id == s->group_id) {
			response.create_error = SERVER_ERROR_LOBBY_ALREADY_EXISTS;
			gameserver_send_to(gserver, &response.header, requested_by);

			return;
		}
	}

	response.create_error = 0;
	requested_by->group_id = msg->lobby_id;
	//messagequeue_add("#%06d created, and joined lobby %d!", requested_by->id, requested_by->group_id);

	gameserver_send_to(gserver, &response.header, requested_by);

	// also send join response
	struct lobby_join_response join;
	message_header_init(&join.header, LOBBY_JOIN_RESPONSE);
	join.is_other_user = 0;
	join.join_error = SERVER_NO_ERROR;
	join.lobby_id = requested_by->group_id;
	gameserver_send_to(gserver, &join.header, requested_by);

	// also notify all others
	struct lobby_create_response create;
	message_header_init(&create.header, LOBBY_CREATE_RESPONSE);
	create.create_error = SERVER_NO_ERROR;
	create.lobby_id = msg->lobby_id;
	gameserver_send_filtered(gserver, &create.header, requested_by, filter_everybody_exclude);
}

void group_service_join_lobby(struct gameserver *gserver, struct lobby_join_request *msg, struct session *requested_by) {
	struct lobby_join_response join;
	message_header_init(&join.header, LOBBY_JOIN_RESPONSE);
	join.is_other_user = 0;
	join.join_error = SERVER_NO_ERROR;
	join.lobby_id = 0;
	
	// if already in a lobby, leave.
	const int is_already_in_lobby = (requested_by->group_id > 0);
	const int is_leaving = (msg->lobby_id == 0);
	if (is_already_in_lobby && is_leaving) {
		//messagequeue_add("Cannot join %d, Already in lobby %d", msg->lobby_id, requested_by->group_id);
		join.join_error = SERVER_ERROR_ALREADY_IN_A_LOBBY; // can only LEAVE (0) while in lobby
		join.lobby_id = requested_by->group_id; // shouldn't matter, just to be sure.
		goto send_response;
	}

	// check if the lobby exists.
	int lobby_exists = (msg->lobby_id == 0); // "0" is indicates a leave and always "exists".
	if (lobby_exists == 0) {
		const size_t sessions_len = stbds_arrlen(gserver->sessions);
		for (size_t i = 0; i < sessions_len; ++i) {
			struct session *s = gserver->sessions[i];
			if ((uint32_t)msg->lobby_id == s->group_id) {
				lobby_exists = 1;
				break;
			}
		}
	}

	// lobby doesnt exist? then we cant join.
	if (lobby_exists == 0) {
		join.join_error = SERVER_ERROR_LOBBY_DOES_NOT_EXIST; // INFO: lobby does not exist.
		join.lobby_id = 0;
		goto send_response;
	}

	if (msg->lobby_id != 0) {
		requested_by->group_id = msg->lobby_id;
	}

	// lobby_id can be 0 to indicate a leave
	join.join_error = SERVER_NO_ERROR;
	join.lobby_id = msg->lobby_id;

send_response:
	gameserver_send_to(gserver, &join.header, requested_by);

	if (join.join_error == 0) {
		join.is_other_user = 1;
		//messagequeue_add("Sending to group...");
		gameserver_send_filtered(gserver, &join.header, requested_by, filter_group_exclude);
	}

	if (msg->lobby_id == 0) {
		requested_by->group_id = msg->lobby_id;
	}
}

/** Sends a list of all lobbies to `requested_by`.
 * @param msg The request data.
 * @param requested_by The client who requested the list.
 */
void group_service_list_lobbies(struct gameserver *gserver, struct lobby_list_request *msg, struct session *requested_by) {
	int sessions_len = stbds_arrlen(gserver->sessions);
	if (sessions_len > 8) sessions_len = 8;

	struct lobby_list_response res;
	message_header_init(&res.header, LOBBY_LIST_RESPONSE);
	res.ids_of_lobbies_len = 0;
	for (int i = 0; i < sessions_len; ++i) {
		int lobby_id = gserver->sessions[i]->group_id;
		
		// check if we already added lobby_id to list.
		for (int j = 0; j < res.ids_of_lobbies_len; ++j) {
			if (lobby_id == res.ids_of_lobbies[j]) {
				lobby_id = 0; // already exists
			}
		}

		if (lobby_id > 0) {
			res.ids_of_lobbies[res.ids_of_lobbies_len] = lobby_id;
			++res.ids_of_lobbies_len;
		}
	}
	gameserver_send_to(gserver, &res.header, requested_by);
}

