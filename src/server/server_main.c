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
	int random;
};

//
// server code
//

void *wsserver_thread(void *data);

static WINDOW *create_messages_window(void);
static WINDOW *create_input_window(void);

static const char *fmt_time(time_t time);
static const char *fmt_date(time_t rawtime);

static void messagequeue_add(char *fmt, ...);

struct session *add_session(struct lws *wsi);

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
			// build command for raw tcp...
			int command_len = snprintf(NULL, 0, "%.*s", (int)in_command_i, in_command);
			char command[command_len + 1];
			snprintf(command, command_len + 1, "%.*s", (int)in_command_i, in_command);
			// ...and for websockets
			char ws_command[LWS_PRE + command_len + 1];
			snprintf(&ws_command[LWS_PRE], command_len + 1, "%.*s", (int)in_command_i, in_command);

			size_t session_len = stbds_arrlen(sessions);
			for (size_t i = 0; i < session_len; ++i) {
				lws_write(sessions[i].wsi, (unsigned char *)&ws_command[LWS_PRE], command_len + 1, LWS_WRITE_TEXT);
			}

			messagequeue_add("[%s]  %s  (seen by %zu)", fmt_time(time(NULL)), command, session_len);
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
		session->wsi = wsi;
		session->random = rand() % 999;
		stbds_arrput(sessions, *session);
		break;
	case LWS_CALLBACK_RECEIVE:
		messagequeue_add("WebSocket Userdata: %d", session->random);
		messagequeue_add("WebSocket said: '%.*s'", data_len, (char *)data);
		//lws_write(wsi, data, data_len, LWS_WRITE_TEXT);
		break;
	case LWS_CALLBACK_CLOSED:
		messagequeue_add("WebSocket disconnected!");
		for (int i = 0; i < stbds_arrlen(sessions); ++i) {
			if (sessions[i].random == session->random) {
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
		session->wsi = wsi;
		session->random = rand() % 999;
		stbds_arrput(sessions, *session);
		break;
	}
	case LWS_CALLBACK_RAW_RX: {
		messagequeue_add("TCP-Client Userdata: %d", session->random);
		messagequeue_add("TCP-Client said: '%.*s'", (int)data_len - 1, (char *)data);
		break;
	}
	case LWS_CALLBACK_RAW_CLOSE: {
		messagequeue_add("TCP-Client disconnected!");
		for (int i = 0; i < stbds_arrlen(sessions); ++i) {
			if (sessions[i].random == session->random) {
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
		session->wsi = wsi;
		session->random = rand() % 999;
		stbds_arrput(sessions, *session);
		break;
	case LWS_CALLBACK_RECEIVE:
		messagequeue_add("Client Userdata: %d", session->random);
		messagequeue_add("Client said: '%.*s'", data_len, (char *)data);
		//lws_write(wsi, data, data_len, LWS_WRITE_TEXT);
		break;
	case LWS_CALLBACK_CLOSED:
		messagequeue_add("Client disconnected!");
		for (int i = 0; i < stbds_arrlen(sessions); ++i) {
			if (sessions[i].random == session->random) {
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

