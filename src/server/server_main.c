#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <ncurses.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <libwebsockets.h>
#include <stb_ds.h>
#include "net/message.h"

//
// logic
//

struct session {
	// connection
	struct lws *wsi;
	int id; // unique until disconnected

	// message queue
	char **msg_queue;

	// game logic
	int is_in_lobby_with_id;

};

void session_init(struct session *, struct lws *wsi);
void session_destroy(struct session *s);
void session_send_message(struct session *, cJSON *message);
void session_on_writable(struct session *);

//
// functions
//


void server_on_message(struct session *session, void *data, size_t data_len);
static void serverui_on_input(const char *cmd, size_t cmd_len);

void *wsserver_thread(void *data);

// ui
static WINDOW *create_messages_window(void);
static WINDOW *create_input_window(void);

static const char *fmt_time(time_t time);
static const char *fmt_date(time_t rawtime);

static void messagequeue_add(char *fmt, ...);

struct session *add_session(struct lws *wsi);

// game logic
static void create_lobby(struct lobby_create_request *msg, struct session *);
static void join_lobby(struct lobby_join_request *msg, struct session *session);
static void list_lobbies(struct lobby_list_request *msg, struct session *requested_by);

//
// vars
//

static pthread_mutex_t messagequeue_mutex = PTHREAD_MUTEX_INITIALIZER;
const size_t messagequeue_max = 128;
static char *messagequeue[128];
static size_t messagequeue_len = 0;

static struct session **sessions = NULL;

int main(int argc, char **argv) {
	initscr();
	cbreak();
	noecho();
	//keypad(stdscr, TRUE);

	WINDOW *win_messages = create_messages_window();
	WINDOW *win_input = create_input_window();

	wtimeout(win_input, 500);
	//nodelay(stdscr, TRUE);
	//wtimeout(win_messages, 100);
	wmove(win_messages, 0, 0);
	messagequeue_add("[%s]  Server started on %s", fmt_time(time(NULL)), fmt_date(time(NULL)));
	messagequeue_add("[%s]  PID : %ld\n", fmt_time(time(NULL)), (long)getpid());

	// start bg services
	pthread_t ws_thread;
	pthread_create(&ws_thread, NULL, wsserver_thread, NULL);

	int running = 1;
	while (running) {
		static char in_command[256] = {0};
		static size_t in_command_i = 0;

		// logic
		{
			pthread_mutex_lock(&messagequeue_mutex);

			size_t i = 0;
			while (i < messagequeue_len) {
				wprintw(win_messages, "%s\n", messagequeue[i]);
				free(messagequeue[i]);
				++i;
			}
			messagequeue_len = 0;

			pthread_mutex_unlock(&messagequeue_mutex);
		}

		// draw
		wattron(win_input, A_BOLD);
		mvwprintw(win_input, 0, 0, "command: ");
		wattroff(win_input, A_BOLD);
		wprintw(win_input, "%.*s", (int)in_command_i, in_command);
		refresh();
		wrefresh(win_messages);
		wrefresh(win_input);

		// read input
		int in_char = wgetch(win_input);
		if (in_char == 127) {
			if (in_command_i > 0) {
				--in_command_i;
			}
			werase(win_input);
		} else if (in_char == '') {
			werase(win_messages);
		} else if (in_char == 23 /* ^W */) {
			in_command_i = 0;
			werase(win_input);
		} else if (in_char == 4 /* ^D */) {
			running = 0;
		} else if (in_char == '\n') {
			serverui_on_input(in_command, in_command_i);
			werase(win_input);
			in_command_i = 0;
		} else if (in_char != ERR) {
			in_command[in_command_i++] = in_char;
		}
	}

	pthread_join(ws_thread, NULL);
	pthread_mutex_destroy(&messagequeue_mutex);

	delwin(win_messages);
	delwin(win_input);
	endwin();

	return 0;
}

//
// server logic
//

void session_init(struct session *session, struct lws *wsi) {
	session->wsi = wsi;
	session->id = (int)((rand() / (float)RAND_MAX) * 999999.0f + 100000.0f);
	session->msg_queue = NULL;
	session->is_in_lobby_with_id = -1;
	
	// id needs to be unique
	size_t sessions_count = stbds_arrlen(sessions);
	for (size_t i = 0; i < sessions_count; ++i) {
		assert(session->id != sessions[i]->id);
	}

	// store in sessions
	stbds_arrput(sessions, session);

}

void session_destroy(struct session *session) {
	stbds_arrfree(session->msg_queue);

	// remove from session
	for (int i = 0; i < stbds_arrlen(sessions); ++i) {
		if (sessions[i]->id == session->id) {
			stbds_arrdel(sessions, i);
			break;
		}
	}
}

void session_on_writable(struct session *session) {
	size_t msg_queue_len = stbds_arrlen(session->msg_queue);
	for (size_t i = 0; i < msg_queue_len; ++i) {
		char *msg = session->msg_queue[i];
		lws_write(session->wsi, (unsigned char *)&msg[LWS_PRE], strlen(&msg[LWS_PRE]), LWS_WRITE_TEXT);
		free(msg);
	}
	stbds_arrfree(session->msg_queue);
	session->msg_queue = NULL;
}

void session_send_message(struct session *session, cJSON *message) {
	assert(session != NULL);
	assert(session->wsi != NULL);
	assert(message != NULL);

	// serialize json to string
	char *json_str = cJSON_PrintUnformatted(message);
	size_t json_str_len = strlen(json_str);

	// dont send nothing
	assert(json_str_len > 0);
	if (json_str_len == 0) {
		return;
	}

	// prepare lws message
	char *data = malloc(LWS_PRE + json_str_len + 1);
	memcpy(&data[LWS_PRE], json_str, json_str_len);
	data[LWS_PRE + json_str_len] = '\0';
	free(json_str);

	// enqueue message and request writing
	stbds_arrpush(session->msg_queue, data);
	lws_callback_on_writable(session->wsi);
}

void server_on_message(struct session *session, void *data, size_t data_len) {
	if (data_len == 0) {
		messagequeue_add("Got empty message from %06d!", session->id);
		return;
	}

	cJSON *data_json = cJSON_ParseWithLength(data, data_len);
	if (data_json == NULL) {
		// just a raw message, no json.
		messagequeue_add("--- Raw Message from #%06d ---\n%.*s\n---     Raw Message End      ---", session->id, (int)data_len, (char *)data);
		return;
	}

	// TODO: this can easily occur, but message.h doesnt validate the data yet.
	//       we just fail here for visibility, these asserts can be removed later.
	assert(cJSON_GetObjectItem(data_json, "header") != NULL);
	assert(cJSON_GetObjectItem(cJSON_GetObjectItem(data_json, "header"), "type") != NULL);
	assert(cJSON_IsNumber(cJSON_GetObjectItem(cJSON_GetObjectItem(data_json, "header"), "type")));


	struct message_header *msg_header = unpack_message(data_json);
	// could not unpack, maybe validation failed?
	if (msg_header == NULL) {
		messagequeue_add("--- Invalid JSON from #%06d ---\n%.*s\n---     Invalid JSON End      ---", session->id, (int)data_len, (char *)data);
		cJSON_Delete(data_json);
		return;
	}

	// we got a fully valid message.
	messagequeue_add("Received %s from #%06d", message_type_to_name(msg_header->type), session->id);
	switch ((enum message_type)msg_header->type) {
		case LOBBY_CREATE_REQUEST:
			create_lobby((struct lobby_create_request *)msg_header, session);
			break;
		case LOBBY_JOIN_REQUEST:
			join_lobby((struct lobby_join_request *)msg_header, session);
			break;
		case LOBBY_LIST_REQUEST: {
			list_lobbies((struct lobby_list_request *)msg_header, session);
			break;
		}
		case MSG_UNKNOWN:
		case LOBBY_CREATE_RESPONSE:
		case LOBBY_JOIN_RESPONSE:
		case LOBBY_LIST_RESPONSE:
			// these messages we cant handle
			break;
	}
	
	free_message(data_json, msg_header);
}

//
// server ui
//

static void serverui_on_input(const char *cmd, size_t cmd_len) {
	// input to c string
	int input_len = snprintf(NULL, 0, "%.*s", (int)cmd_len, cmd);
	char input[input_len + 1];
	snprintf(input, input_len + 1, "%.*s", (int)cmd_len, cmd);
	
	int is_command = (input[0] == '/');
	if (is_command) {
		messagequeue_add("%s", input);
		if (strncmp(input, "/queue", strlen("/queue")) == 0) {
			for (int i = 0; i < stbds_arrlen(sessions); ++i) {
				struct session *s = sessions[i];
				if (s->msg_queue != NULL) {
					messagequeue_add("Outstanding messages for client %d:", s->id);
					for (int j = 0; j < stbds_arrlen(s->msg_queue); ++j) {
						messagequeue_add(" - %s", s->msg_queue[i]);
					}
				} else {
					messagequeue_add("No Message Queue for %d:", s->id);
				}
			}
		}

		if (strncmp(input, "/sessions", strlen("/sessions")) == 0) {
			const int sessions_len = stbds_arrlen(sessions);
			for (int i = 0; i < sessions_len; ++i) {
				const struct session *s = sessions[i];
				messagequeue_add("Session #%d (%d): In Lobby: %d", i, s->id, s->is_in_lobby_with_id);
			}
			messagequeue_add("Total: %d Sessions", sessions_len);
		}

	} else {
		// TODO: remove. just sends the raw input for debugging purposes.
		char ws_command[LWS_PRE + input_len + 1];
		snprintf(&ws_command[LWS_PRE], input_len + 1, "%.*s", (int)cmd_len, cmd);

		size_t session_len = stbds_arrlen(sessions);
		for (size_t i = 0; i < session_len; ++i) {
			lws_write(sessions[i]->wsi, (unsigned char *)&ws_command[LWS_PRE], input_len + 1, LWS_WRITE_TEXT);
		}

		messagequeue_add("[%s]  %s  (seen by %zu)", fmt_time(time(NULL)), input, session_len);
	}
}

static WINDOW *create_messages_window(void) {
	WINDOW *win = newwin(LINES, COLS, 0, 0);
	scrollok(win, TRUE);

	return win;
}

static WINDOW *create_input_window(void) {
	WINDOW *win = newwin(1, COLS, LINES - 1, 0);
	return win;
}

static const char *fmt_time(time_t rawtime) {
	// Get current time
	time(&rawtime);
	struct tm *timeinfo = localtime(&rawtime);

	static char buffer[9]; // HH:MM:SS\0
	strftime(buffer, sizeof(buffer), "%H:%M:%S", timeinfo);
	return buffer;
}

static const char *fmt_date(time_t rawtime) {
	// Get current time
	time(&rawtime);
	struct tm *timeinfo = localtime(&rawtime);

	static char buffer[11]; // YYYY-MM-DD\0
	strftime(buffer, sizeof(buffer), "%Y-%m-%d", timeinfo);
	return buffer;
}

static void messagequeue_add(char *fmt, ...) {
	va_list args;
	
	// determine size
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	char *buf = malloc((len + 1) * sizeof(char));
	buf[len] = '\0';
	va_end(args);

	va_start(args, fmt);
	vsnprintf(buf, len + 1, fmt, args);
	va_end(args);

	// write to queue
	pthread_mutex_lock(&messagequeue_mutex);
	assert(messagequeue_len < messagequeue_max);
	messagequeue[messagequeue_len++] = buf;
	pthread_mutex_unlock(&messagequeue_mutex);

}

//
// server impls
//

static int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *data, size_t data_len) {
	struct session *session = user;

	switch ((int)reason) {
	case LWS_CALLBACK_ESTABLISHED:
		session_init(session, wsi);
		messagequeue_add("WebSocket #%06d connected!", session->id);
		break;
	case LWS_CALLBACK_RECEIVE: {
		server_on_message(session, data, data_len);
		break;
	}
	case LWS_CALLBACK_CLOSED:
		messagequeue_add("WebSocket #%06d disconnected!", session->id);
		session_destroy(session);
		break;
	case LWS_CALLBACK_SERVER_WRITEABLE:
		session_on_writable(session);
		break;
	default: break;
	}

	return 0;
}

static int rawtcp_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *data, size_t data_len) {
	struct session *session = user;

	switch ((int)reason) {
	case LWS_CALLBACK_RAW_ADOPT: {
		session_init(session, wsi);
		messagequeue_add("TCP-Client #%06d connected!", session->id);
		break;
	}
	case LWS_CALLBACK_RAW_RX: {
		server_on_message(session, data, data_len);
		break;
	}
	case LWS_CALLBACK_RAW_CLOSE: {
		messagequeue_add("TCP-Client #%06d disconnected!", session->id);
		session_destroy(session);
		break;
	}
	case LWS_CALLBACK_SERVER_WRITEABLE:
		session_on_writable(session);
		break;
	case LWS_CALLBACK_RAW_WRITEABLE:
		// raw tcp client is writable
		session_on_writable(session);
		break;
	case LWS_CALLBACK_ESTABLISHED:
		session_init(session, wsi);
		messagequeue_add("Client %06d connected!", session->id);
		break;
	case LWS_CALLBACK_RECEIVE: {
		server_on_message(session, data, data_len);
		break;
	}
	case LWS_CALLBACK_CLOSED:
		messagequeue_add("Client %06d disconnected!", session->id);
		session_destroy(session);
		break;
	default: break;
	}

	return 0;
}

// Browser/SDLNet → binary/ws_callback()
// Native/SDLNet → rawtcp_callback()
static struct lws_protocols protocols[] = {
	{""      , rawtcp_callback, sizeof(struct session), 512, 0, NULL, 0},
	{"binary", ws_callback    , sizeof(struct session), 512, 0, NULL, 0}, // needed for SDLNet-to-WebSocket translation.
	{NULL    , NULL           , 0,   0, 0, NULL, 0},
};
void *wsserver_thread(void *data) {
	lws_set_log_level(0, NULL);

	struct lws_context_creation_info info = {0};
	info.port = 9124;
	info.iface = NULL;
	info.protocols = protocols;
	info.gid = -1;
	info.uid = -1;
	info.options = LWS_SERVER_OPTION_FALLBACK_TO_RAW;

	struct lws_context *ctx = lws_create_context(&info);
	if (ctx == NULL) {
		return (void *)1;
	}

	messagequeue_add("WebSocket listening on :%d...", info.port);
	
	while (1) {
		lws_service(ctx, 100);
	}
	
	lws_context_destroy(ctx);

	return (void *)0;
}

//
// game logic
//

/* Creates a new lobby and moves `requested_by` into it.
 * @param msg The requests data.
 * @param session The session which requested this. Can be NULL, if the server creates the lobby for example.
 */
static void create_lobby(struct lobby_create_request *msg, struct session *requested_by) {
	messagequeue_add("[create_lobby] Session Ptr: %p", requested_by);
	assert(msg != NULL);
	assert(requested_by != NULL); // TODO: also support no session
	assert(msg->lobby_id >= 0); // TODO: this is application logic...

	struct lobby_create_response response;
	message_header_init(&response.header, LOBBY_CREATE_RESPONSE);
	response.lobby_id = msg->lobby_id;
	response.create_error = 0;

	if (requested_by->is_in_lobby_with_id >= 0) {
		messagequeue_add("#%06d is already in lobby %d but requested to create %d", requested_by->id, requested_by->is_in_lobby_with_id, msg->lobby_id);
		response.create_error = 1;

		cJSON *json = cJSON_CreateObject();
		pack_lobby_create_response(&response, json);

		session_send_message(requested_by, json);

		cJSON_Delete(json);
		return;
	}

	// TODO: check if lobby exists

	response.create_error = 0;
	requested_by->is_in_lobby_with_id = msg->lobby_id;
	messagequeue_add("#%06d created, and joined lobby %d!", requested_by->id, requested_by->is_in_lobby_with_id);


	cJSON *json = cJSON_CreateObject();
	pack_lobby_create_response(&response, json);

	session_send_message(requested_by, json);

	cJSON_Delete(json);

	// also send join response
	struct lobby_join_response join;
	message_header_init(&join.header, LOBBY_JOIN_RESPONSE);
	join.join_error = 0;
	join.lobby_id = requested_by->is_in_lobby_with_id;
	json = cJSON_CreateObject();
	pack_lobby_join_response(&join, json);
	session_send_message(requested_by, json);
	cJSON_Delete(json);
}

static void join_lobby(struct lobby_join_request *msg, struct session *requested_by) {
	
	struct lobby_join_response join;
	message_header_init(&join.header, LOBBY_JOIN_RESPONSE);
	
	// if already in a lobby, leave.
	const int is_already_in_lobby = (requested_by->is_in_lobby_with_id >= 0);
	if (is_already_in_lobby && msg->lobby_id != -1) {
		messagequeue_add("Cannot join %d, Already in lobby %d", msg->lobby_id, requested_by->is_in_lobby_with_id);
		join.join_error = 1; // can only LEAVE (-1) while in lobby
		join.lobby_id = requested_by->is_in_lobby_with_id; // shouldn't matter, just to be sure.
		goto send_response;
	}

	// apply data
	requested_by->is_in_lobby_with_id = msg->lobby_id;

	// lobby_id can be -1 to indicate a leave
	join.join_error = 0;
	join.lobby_id = msg->lobby_id;

send_response:
	{
	// send response
	cJSON *json = cJSON_CreateObject();
	pack_lobby_join_response(&join, json);
	session_send_message(requested_by, json);
	cJSON_Delete(json);
	}
}

/** Sends a list of all lobbies to `requested_by`.
 * @param msg The request data.
 * @param requested_by The client who requested the list.
 */
static void list_lobbies(struct lobby_list_request *msg, struct session *requested_by) {
	int sessions_len = stbds_arrlen(sessions);
	if (sessions_len > 8) sessions_len = 8;

	struct lobby_list_response res;
	message_header_init(&res.header, LOBBY_LIST_RESPONSE);
	res.ids_of_lobbies_len = 0;
	for (int i = 0; i < sessions_len; ++i) {
		const int lobby_id = sessions[i]->is_in_lobby_with_id;
		if (lobby_id >= 0) {
			res.ids_of_lobbies[res.ids_of_lobbies_len] = lobby_id;
			++res.ids_of_lobbies_len;
		}
	}
	cJSON *json = cJSON_CreateObject();
	pack_lobby_list_response(&res, json);
	session_send_message(requested_by, json);
	cJSON_Delete(json);
}

