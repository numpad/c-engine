#include "message.h"

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <cJSON.h>

const message_function_info_t message_function_infos[MSG_TYPE_MAX] = {
	[MSG_UNKNOWN] = (message_function_info_t){
		.name="MSG_UNKNOWN",
		.pack_fn=NULL,
		.unpack_fn=NULL,
		.struct_size=0,
	},
	[LOBBY_CREATE_REQUEST] = (message_function_info_t){
		.name        = "LOBBY_CREATE_REQUEST",
		.pack_fn     = (message_pack_fn)pack_lobby_create_request,
		.unpack_fn   = (message_unpack_fn)unpack_lobby_create_request,
		.struct_size = sizeof(struct lobby_create_request),
	},
	[LOBBY_CREATE_RESPONSE] = (message_function_info_t){
		.name        = "LOBBY_CREATE_RESPONSE",
		.pack_fn     = (message_pack_fn)pack_lobby_create_response,
		.unpack_fn   = (message_unpack_fn)unpack_lobby_create_response,
		.struct_size = sizeof(struct lobby_create_response),
	},
	[LOBBY_JOIN_REQUEST] = (message_function_info_t){
		.name        = "LOBBY_JOIN_REQUEST",
		.pack_fn     = (message_pack_fn)pack_lobby_join_request,
		.unpack_fn   = (message_unpack_fn)unpack_lobby_join_request,
		.struct_size = sizeof(struct lobby_join_request),
	},
	[LOBBY_JOIN_RESPONSE] = (message_function_info_t){
		.name        = "LOBBY_JOIN_RESPONSE",
		.pack_fn     = (message_pack_fn)pack_lobby_join_response,
		.unpack_fn   = (message_unpack_fn)unpack_lobby_join_response,
		.struct_size = sizeof(struct lobby_join_response),
	},
	[LOBBY_LIST_REQUEST] = (message_function_info_t){
		.name        = "LOBBY_LIST_REQUEST",
		.pack_fn     = (message_pack_fn)pack_lobby_list_request,
		.unpack_fn   = (message_unpack_fn)unpack_lobby_list_request,
		.struct_size = sizeof(struct lobby_list_request),
	},
	[LOBBY_LIST_RESPONSE] = (message_function_info_t){
		.name        = "LOBBY_LIST_RESPONSE",
		.pack_fn     = (message_pack_fn)pack_lobby_list_response,
		.unpack_fn   = (message_unpack_fn)unpack_lobby_list_response,
		.struct_size = sizeof(struct lobby_list_response),
	},
};

// Message Header
const char *message_type_to_name(enum message_type type) {
	return message_function_infos[type].name;
}

void message_header_init(struct message_header *msg, int16_t type) {
	msg->type = type;
}

void pack_message_header(struct message_header *msg, cJSON *json) {
	cJSON *header = cJSON_AddObjectToObject(json, "header");
	cJSON_AddNumberToObject(header, "type", msg->type);
}

void unpack_message_header(cJSON *json, struct message_header *msg) {
	cJSON *header = cJSON_GetObjectItem(json, "header");
	if (header == NULL || !cJSON_IsObject(header)) {
		msg->type = MSG_UNKNOWN;
		return;
	}

	cJSON *header_type = cJSON_GetObjectItem(header, "type");
	if (header_type == NULL || !cJSON_IsNumber(header_type)) {
		msg->type = MSG_UNKNOWN;
		return;
	}

	msg->type = header_type->valueint;
}

cJSON *pack_message(struct message_header *msg) {
	assert(msg != NULL);
	assert(msg->type < MSG_TYPE_MAX);
	if (msg->type == MSG_UNKNOWN) {
		return NULL;
	}

	message_pack_fn pack_fn = message_function_infos[msg->type].pack_fn;
	assert(pack_fn != NULL);

	cJSON *json = cJSON_CreateObject();
	pack_fn(msg, json);

	return json;
}

struct message_header *unpack_message(cJSON *json) {
	if (json == NULL) {
		return NULL;
	}

	struct message_header header;
	unpack_message_header(json, &header);
	if (header.type == 0) {
		return NULL;
	}

	const message_function_info_t *info = &message_function_infos[header.type];
	const message_unpack_fn unpack_fn = info->unpack_fn;
	const size_t struct_size = info->struct_size;
	assert(unpack_fn != NULL);
	assert(struct_size > 0);

	struct message_header *msg = malloc(struct_size);
	unpack_fn(json, msg);
	assert(msg->type != MSG_UNKNOWN);
	return msg;
}

void free_message(cJSON *json, struct message_header *msg) {
	free(msg);
	cJSON_Delete(json);
}


// LOBBY_CREATE_REQUEST
void pack_lobby_create_request(struct lobby_create_request *msg, cJSON *json) {
	assert(msg->header.type == LOBBY_CREATE_REQUEST);
	pack_message_header(&msg->header, json);

	cJSON_AddNumberToObject(json, "lobby_id", msg->lobby_id);
	cJSON_AddStringToObject(json, "lobby_name", msg->lobby_name);
}

void unpack_lobby_create_request(cJSON *json, struct lobby_create_request *msg) {
	unpack_message_header(json, &msg->header);
	assert(msg->header.type == LOBBY_CREATE_REQUEST);

	msg->lobby_id = cJSON_GetObjectItem(json, "lobby_id")->valueint;
	msg->lobby_name = cJSON_GetObjectItem(json, "lobby_name")->valuestring;
}


// LOBBY_CREATE_RESPONSE
void pack_lobby_create_response(struct lobby_create_response *msg, cJSON *json) {
	assert(msg->header.type == LOBBY_CREATE_RESPONSE);
	pack_message_header(&msg->header, json);

	cJSON_AddNumberToObject(json, "lobby_id", msg->lobby_id);
	cJSON_AddNumberToObject(json, "create_error", msg->create_error);
}

void unpack_lobby_create_response(cJSON *json, struct lobby_create_response *msg) {
	unpack_message_header(json, &msg->header);
	assert(msg->header.type == LOBBY_CREATE_RESPONSE);
	
	msg->lobby_id = cJSON_GetObjectItem(json, "lobby_id")->valueint;
	msg->create_error = cJSON_GetObjectItem(json, "create_error")->valueint;
}


// LOBBY_JOIN_REQUEST
void pack_lobby_join_request(struct lobby_join_request *msg, cJSON *json) {
	assert(msg->header.type == LOBBY_JOIN_REQUEST);
	pack_message_header(&msg->header, json);

	cJSON_AddNumberToObject(json, "lobby_id", msg->lobby_id);
}

void unpack_lobby_join_request(cJSON *json, struct lobby_join_request *msg) {
	unpack_message_header(json, &msg->header);
	assert(msg->header.type == LOBBY_JOIN_REQUEST);

	msg->lobby_id = cJSON_GetObjectItem(json, "lobby_id")->valueint;
}


// LOBBY_JOIN_RESPONSE
void pack_lobby_join_response(struct lobby_join_response *msg, cJSON *json) {
	assert(msg->header.type == LOBBY_JOIN_RESPONSE);
	pack_message_header(&msg->header, json);

	cJSON_AddNumberToObject(json, "lobby_id", msg->lobby_id);
	cJSON_AddNumberToObject(json, "join_error", msg->join_error);
}

void unpack_lobby_join_response(cJSON *json, struct lobby_join_response *msg) {
	unpack_message_header(json, &msg->header);
	assert(msg->header.type == LOBBY_JOIN_RESPONSE);

	msg->lobby_id = cJSON_GetObjectItem(json, "lobby_id")->valueint;
	msg->join_error = cJSON_GetObjectItem(json, "join_error")->valueint;
}

// LOBBY_LIST_REQUEST
void pack_lobby_list_request(struct lobby_list_request *msg, cJSON *json) {
	assert(msg->header.type == LOBBY_LIST_REQUEST);
	pack_message_header(&msg->header, json);

	// no fields to store
}

void unpack_lobby_list_request(cJSON *json, struct lobby_list_request *msg) {
	unpack_message_header(json, &msg->header);
	assert(msg->header.type == LOBBY_LIST_REQUEST);
	
	// no fields to retrieve
}

// LOBBY_LIST_RESPONSE
void pack_lobby_list_response(struct lobby_list_response *msg, cJSON *json) {
	assert(msg->header.type == LOBBY_LIST_RESPONSE);
	pack_message_header(&msg->header, json);
	assert(msg->ids_of_lobbies_len <= 8); // TODO: fix this.

	cJSON *ids = cJSON_CreateIntArray(msg->ids_of_lobbies, msg->ids_of_lobbies_len);
	cJSON_AddItemToObject(json, "ids_of_lobbies", ids);
}

void unpack_lobby_list_response(cJSON *json, struct lobby_list_response *msg) {
	unpack_message_header(json, &msg->header);
	assert(msg->header.type == LOBBY_LIST_RESPONSE);
	
	cJSON *ids = cJSON_GetObjectItem(json, "ids_of_lobbies");
	if (ids == NULL || !cJSON_IsArray(ids)) {
		msg->header.type = MSG_UNKNOWN;
		return;
	}

	msg->ids_of_lobbies_len = cJSON_GetArraySize(ids);
	assert(msg->ids_of_lobbies_len <= 8); // TODO: fix this.
	for (int i = 0; i < msg->ids_of_lobbies_len; ++i) {
		cJSON *id = cJSON_GetArrayItem(ids, i);
		assert(id != NULL);
		assert(cJSON_IsNumber(id));
		msg->ids_of_lobbies[i] = id->valueint;
	}
}

