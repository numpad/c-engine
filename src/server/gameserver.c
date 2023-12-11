#include "gameserver.h"

#include <assert.h>
#include <string.h>
#include <libwebsockets.h>
#include <cJSON.h>
#include <stb_ds.h>
#include "net/message.h"

//
// private api
//

static int callback_rawtcp(struct lws *, enum lws_callback_reasons, void *, void *, size_t);
static int callback_ws    (struct lws *, enum lws_callback_reasons, void *, void *, size_t);

static void gameserver_on_connect   (struct gameserver *, struct session *, struct lws *wsi);
static void gameserver_on_disconnect(struct gameserver *, struct session *);
static void gameserver_on_message   (struct gameserver *, struct session *, void *, size_t);
static void gameserver_on_writable  (struct gameserver *, struct session *);

//
// private variables
//

static struct lws_protocols protocols[] = {
	{"",       callback_rawtcp, sizeof(struct session), 512, 0, NULL, 0},
	{"binary", callback_ws,     sizeof(struct session), 512, 0, NULL, 0}, // needed for SDLNet-to-WebSocket translation.
	{NULL,     NULL,            0,                      0,   0, NULL, 0},
};

//
// api
//

int filter_group            (struct session *o, struct session *t) { return (t->group_id == o->group_id); }
int filter_group_exclude    (struct session *o, struct session *t) { return (o->id != t->id && t->group_id == o->group_id); }
int filter_everybody        (struct session *o, struct session *t) { return 1; }
int filter_everybody_exclude(struct session *o, struct session *t) { return (o->id != t->id); }

// initialization
int gameserver_init(struct gameserver *server, uint16_t port) {
	memset(server, 0, sizeof(*server));

	// init libwebsockets
	lws_set_log_level(0, NULL);

	struct lws_context_creation_info info = {0};
	info.port = port;
	info.iface = NULL;
	info.protocols = protocols;
	info.gid = -1;
	info.uid = -1;
	info.options = LWS_SERVER_OPTION_FALLBACK_TO_RAW;
	info.user = (void *)server;

	server->lws = lws_create_context(&info);
	return (server->lws == NULL);
}

void gameserver_destroy(struct gameserver *server) {
	lws_context_destroy(server->lws);
}

// main loop
void gameserver_listen(struct gameserver *server) {
	while (1) {
		lws_service(server->lws, 100);
	}
}

// send raw bytes
void gameserver_send_raw(struct gameserver *gserver, struct session *receiver, uint8_t *data, size_t data_len) {
	assert(gserver != NULL);
	assert(data != NULL);
	assert(data_len > 0);

	// build message for lws
	uint8_t message[LWS_PRE + data_len];
	memcpy(&message[LWS_PRE], data, data_len);
	
	// if the receiver is NULL, we send to everybody
	// TODO: use receiver->message_queue and callback_on_writable()
	if (receiver == NULL) {
		const size_t sessions_len = stbds_arrlen(gserver->sessions);
		for (size_t i = 0; i < sessions_len; ++i) {
			lws_write(gserver->sessions[i]->wsi, &message[LWS_PRE], data_len, LWS_WRITE_TEXT);
		}
	} else {
		lws_write(receiver->wsi, &message[LWS_PRE], data_len, LWS_WRITE_TEXT);
	}
}

void gameserver_send_to(struct gameserver *gserver, struct message_header *message, struct session *receiver) {
	// serialize message
	cJSON *json = pack_message(message);
	char *json_str = cJSON_PrintUnformatted(json); // TODO: maybe we can implement this directly with LWS_PRE padding?
	const size_t json_str_len = strlen(json_str);
	cJSON_Delete(json);
	
	// TODO: validation
	assert(json_str != NULL);
	assert(json_str_len > 0);

	// build lws message
	uint8_t *data = malloc(LWS_PRE + json_str_len + 1);
	memcpy(&data[LWS_PRE], json_str, json_str_len + 1);
	free(json_str);

	// enqueue message
	stbds_arrpush(receiver->message_queue, data);
	lws_callback_on_writable(receiver->wsi);
}

void gameserver_send_filtered(struct gameserver *gserver, struct message_header *message, struct session *master, session_filter_fn filter) {
	assert(gserver != NULL);
	assert(message != NULL);
	assert(filter != NULL);

	// serialize message
	cJSON *json = pack_message(message);
	char *json_str = cJSON_PrintUnformatted(json); // TODO: maybe we can implement this directly with LWS_PRE padding?
	const size_t json_str_len = strlen(json_str);
	cJSON_Delete(json);
	
	// TODO: validation
	assert(json_str != NULL);
	assert(json_str_len > 0);

	// build lws message
	const size_t sessions_len = stbds_arrlen(gserver->sessions);
	for (size_t i = 0; i < sessions_len; ++i) {
		struct session *tested = gserver->sessions[i];
		if (filter(master, tested)) {
			// TODO: refcount message instead of `malloc`ing for every session
			unsigned char *data = malloc(LWS_PRE + json_str_len + 1);
			memcpy(&data[LWS_PRE], json_str, json_str_len + 1);

			stbds_arrpush(tested->message_queue, data);
			lws_callback_on_writable(tested->wsi);
		}
	}

	free(json_str);
}

//
// private implementation
//

static int callback_ws(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *data, size_t data_len) {
	struct session *session = user;
	struct gameserver *server = lws_context_user(lws_get_context(wsi));

	switch ((int)reason) {
	case LWS_CALLBACK_ESTABLISHED:
		gameserver_on_connect(server, session, wsi);
		//messagequeue_add("WebSocket #%06d connected!", session->id);
		break;
	case LWS_CALLBACK_RECEIVE: {
		gameserver_on_message(server, session, data, data_len);
		break;
	}
	case LWS_CALLBACK_CLOSED:
		//messagequeue_add("WebSocket #%06d disconnected!", session->id);
		gameserver_on_disconnect(server, session);
		break;
	case LWS_CALLBACK_SERVER_WRITEABLE:
		gameserver_on_writable(server, session);
		break;
	default: break;
	}

	return 0;
}

static int callback_rawtcp(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *data, size_t data_len) {
	struct session *session = user;
	struct gameserver *server = lws_context_user(lws_get_context(wsi));

	switch ((int)reason) {
	case LWS_CALLBACK_RAW_ADOPT: {
		gameserver_on_connect(server, session, wsi);
		//messagequeue_add("TCP-Client #%06d connected!", session->id);
		break;
	}
	case LWS_CALLBACK_RAW_RX: {
		gameserver_on_message(server, session, data, data_len);
		break;
	}
	case LWS_CALLBACK_RAW_CLOSE: {
		//messagequeue_add("TCP-Client #%06d disconnected!", session->id);
		gameserver_on_disconnect(server, session);
		break;
	}
	case LWS_CALLBACK_SERVER_WRITEABLE:
		gameserver_on_writable(server, session);
		break;
	case LWS_CALLBACK_RAW_WRITEABLE:
		gameserver_on_writable(server, session);
		break;
	case LWS_CALLBACK_ESTABLISHED:
		gameserver_on_connect(server, session, wsi);
		//messagequeue_add("Client %06d connected!", session->id);
		break;
	case LWS_CALLBACK_RECEIVE: {
		gameserver_on_message(server, session, data, data_len);
		break;
	}
	case LWS_CALLBACK_CLOSED:
		//messagequeue_add("Client %06d disconnected!", session->id);
		gameserver_on_disconnect(server, session);
		break;
	default: break;
	}

	return 0;
}

static void gameserver_on_connect(struct gameserver *server, struct session *session, struct lws *wsi) {
	memset(session, 0, sizeof(*session));

	// initialize
	session->wsi = wsi;
	session->id = (uint32_t)((rand() / (float)RAND_MAX) * 999999.0f + 100000.0f);
	session->message_queue = NULL;
	session->group_id = 0;

	// store session
	size_t sessions_count = stbds_arrlen(server->sessions);
	for (size_t i = 0; i < sessions_count; ++i) {
		assert(session->id != server->sessions[i]->id);
	}

	stbds_arrpush(server->sessions, session);

	// propagate
	if (server->callback_on_connect != NULL) {
		server->callback_on_connect(server, session);
	}
}

static void gameserver_on_disconnect(struct gameserver *server, struct session *session) {
	// cleanup
	stbds_arrfree(session->message_queue);

	// remove from sessions
	size_t sessions_count = stbds_arrlen(server->sessions);
	for (size_t i = 0; i < sessions_count; ++i) {
		if (server->sessions[i]->id == session->id) {
			stbds_arrdel(server->sessions, i);
			break;
		}
	}

	// propagate
	if (server->callback_on_disconnect != NULL) {
		server->callback_on_disconnect(server, session);
	}
}

static void gameserver_on_message(struct gameserver *server, struct session *session, void *data, size_t data_len) {
	if (data_len == 0) {
		return;
	}

	// parse as json
	// TODO: handle multiple json objects in data. (see engine.c)
	cJSON *data_json = cJSON_ParseWithLength(data, data_len);
	if (data_json == NULL) {
		return;
	}

	// TODO: this can easily occur, but message.h doesnt validate the data yet.
	//       we just fail here for visibility, these asserts can be removed later.
	assert(cJSON_GetObjectItem(data_json, "header") != NULL);
	assert(cJSON_GetObjectItem(cJSON_GetObjectItem(data_json, "header"), "type") != NULL);
	assert(cJSON_IsNumber(cJSON_GetObjectItem(cJSON_GetObjectItem(data_json, "header"), "type")));

	// unpack
	struct message_header *message = unpack_message(data_json);
	if (message == NULL) {
		cJSON_Delete(data_json);
		return;
	}

	assert(message->type != MSG_TYPE_UNKNOWN);
	assert(message->type != MSG_TYPE_MAX);

	// propagate
	if (server->callback_on_message != NULL) {
		server->callback_on_message(server, session, message);
	}

	free_message(data_json, message);
}

static void gameserver_on_writable(struct gameserver *server, struct session *session) {
	// write all queued messages
	size_t message_queue_len = stbds_arrlen(session->message_queue);
	for (size_t i = 0; i < message_queue_len; ++i) {
		char *msg = session->message_queue[i];

		lws_write(session->wsi, (unsigned char *)&msg[LWS_PRE], strlen(&msg[LWS_PRE]), LWS_WRITE_TEXT);
		free(msg);
	}
	stbds_arrfree(session->message_queue);
	session->message_queue = NULL;
}


