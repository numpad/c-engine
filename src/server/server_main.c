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
	struct lws *wsi;
	int id; // unique until disconnected
	
	int is_in_lobby_with_id;
};

void session_init(struct session *, struct lws *wsi);

//
// functions
//

void send_json_message(struct lws *wsi, cJSON *message);

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

//
// vars
//

static pthread_mutex_t messagequeue_mutex = PTHREAD_MUTEX_INITIALIZER;
const size_t messagequeue_max = 128;
static char *messagequeue[128];
static size_t messagequeue_len = 0;

static struct session *sessions = NULL;

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
	messagequeue_add("[%s]  PID : %ld", fmt_time(time(NULL)), (long)getpid());
	messagequeue_add("[%s]  ", fmt_time(time(NULL)));

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
	session->is_in_lobby_with_id = -1;
	
	// id needs to be unique
	size_t sessions_count = stbds_arrlen(sessions);
	for (size_t i = 0; i < sessions_count; ++i) {
		assert(session->id != sessions[i].id);
	}
}

void send_json_message(struct lws *wsi, cJSON *message) {
	assert(wsi != NULL);
	assert(message != NULL);

	// serialize json to string
	char *json_str = cJSON_PrintUnformatted(message);
	size_t json_str_len = strlen(json_str);

	// prepare lws message
	unsigned char data[LWS_PRE + json_str_len + 1];
	data[json_str_len] = '\0';
	memcpy(&data[LWS_PRE], json_str, json_str_len);

	// send
	//lws_write(wsi, data, json_str_len, LWS_WRITE_TEXT);
}

void server_on_message(struct session *session, void *data, size_t data_len) {
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
			messagequeue_add("TODO: implement logic for \"join lobby\"...");
			break;
		case MSG_UNKNOWN:
		case LOBBY_CREATE_RESPONSE:
		case LOBBY_JOIN_RESPONSE:
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
		if (strncmp(input, "/newlobby", 13) == 0) {
		}

	} else {
		// TODO: remove. just sends the raw input for debugging purposes.
		char ws_command[LWS_PRE + input_len + 1];
		snprintf(&ws_command[LWS_PRE], input_len + 1, "%.*s", (int)cmd_len, cmd);

		size_t session_len = stbds_arrlen(sessions);
		for (size_t i = 0; i < session_len; ++i) {
			lws_write(sessions[i].wsi, (unsigned char *)&ws_command[LWS_PRE], input_len + 1, LWS_WRITE_TEXT);
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
		messagequeue_add("WebSocket connected!");
		session_init(session, wsi);
		stbds_arrput(sessions, *session);
		break;
	case LWS_CALLBACK_RECEIVE: {
		server_on_message(session, data, data_len);
		break;
	}
	case LWS_CALLBACK_CLOSED:
		messagequeue_add("WebSocket disconnected!");
		for (int i = 0; i < stbds_arrlen(sessions); ++i) {
			if (sessions[i].id == session->id) {
				stbds_arrdel(sessions, i);
				messagequeue_add("Client session destroyed!");
				break;
			}
		}
		break;
	case LWS_CALLBACK_SERVER_WRITEABLE:
		messagequeue_add("WebSocket writable!");
		break;
	default: break;
	}

	return 0;
}

static int rawtcp_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *data, size_t data_len) {
	struct session *session = user;

	switch ((int)reason) {
	case LWS_CALLBACK_RAW_ADOPT: {
		messagequeue_add("TCP-Client connected!");
		session_init(session, wsi);
		stbds_arrput(sessions, *session);
		break;
	}
	case LWS_CALLBACK_RAW_RX: {
		server_on_message(session, data, data_len);
		break;
	}
	case LWS_CALLBACK_RAW_CLOSE: {
		messagequeue_add("TCP-Client disconnected!");
		for (int i = 0; i < stbds_arrlen(sessions); ++i) {
			if (sessions[i].id == session->id) {
				stbds_arrdel(sessions, i);
				messagequeue_add("TCP-Client session destroyed!");
				break;
			}
		}
		break;
	}
	case LWS_CALLBACK_RAW_WRITEABLE:
		messagequeue_add("RAW_WRITEABLE");
		break;
	case LWS_CALLBACK_ESTABLISHED:
		messagequeue_add("Client connected!");
		session_init(session, wsi);
		stbds_arrput(sessions, *session);
		break;
	case LWS_CALLBACK_RECEIVE: {
		server_on_message(session, data, data_len);
		//lws_write(wsi, &data[LWS_PRE], data_len, LWS_WRITE_TEXT);
		break;
	}
	case LWS_CALLBACK_CLOSED:
		messagequeue_add("Client disconnected!");
		for (int i = 0; i < stbds_arrlen(sessions); ++i) {
			if (sessions[i].id == session->id) {
				stbds_arrdel(sessions, i);
				messagequeue_add("Client session destroyed!");
				break;
			}
		}
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

	// TODO:
	// - Create vhost to use ws & raw sockets together. [1][2]
	// - Enable Keepalive. [1]
	//
	// [0]: https://libwebsockets.org/lws-api-doc-v2.3-stable/html/md_README_8coding.html#:~:text=enable%20your%20vhost%20to%20accept%20RAW%20socket%20connections
	// [1]: https://android.googlesource.com/platform/external/libwebsockets/+/refs/tags/android-s-beta-1/minimal-examples/raw/
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

/* Creates a new lobby.
 * @param The requests data.
 * @param session The session which requested this. Can be NULL, if the server creates the lobby for example.
 */
static void create_lobby(struct lobby_create_request *msg, struct session *requested_by) {
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

		send_json_message(requested_by->wsi, json);

		cJSON_Delete(json);
		return;
	}

	// TODO: check if lobby exists

	response.create_error = 0;
	requested_by->is_in_lobby_with_id = msg->lobby_id;
	messagequeue_add("#%06d created lobby %d!", requested_by->id, msg->lobby_id);


	cJSON *json = cJSON_CreateObject();
	pack_lobby_create_response(&response, json);

	send_json_message(requested_by->wsi, json);

	cJSON_Delete(json);
}

