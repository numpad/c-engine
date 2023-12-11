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
#include "gameserver.h"
#include "net/message.h"
#include "services/services.h"

//
// logic
//

//
// functions
//

static void serverui_on_input(const char *cmd, size_t cmd_len);

void *wsserver_thread(void *data);

// ui
static WINDOW *create_messages_window(void);
static WINDOW *create_input_window(void);

static const char *fmt_time(time_t time);
static const char *fmt_date(time_t rawtime);

static void messagequeue_add(char *fmt, ...);

//
// vars
//

static pthread_mutex_t messagequeue_mutex = PTHREAD_MUTEX_INITIALIZER;
const size_t messagequeue_max = 128;
static char *messagequeue[128];
static size_t messagequeue_len = 0;
static struct gameserver gserver;

int main(int argc, char **argv) {
	initscr();
	cbreak();
	noecho();
	//keypad(stdscr, TRUE);

	WINDOW *win_messages = create_messages_window();
	WINDOW *win_input = create_input_window();

	wtimeout(win_input, 100);
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

static void server_on_connect(struct gameserver *gs, struct session *session) {
	messagequeue_add("Client %d connected.", session->id);
}

static void server_on_disconnect(struct gameserver *gs, struct session *session) {
	messagequeue_add("Client %d disconnected.", session->id);
}

static void server_on_message(struct gameserver *gs, struct session *session, struct message_header *msg_header) {
	messagequeue_add("Received %s from #%06d", message_type_to_name(msg_header->type), session->id);
	switch (msg_header->type) {
		case LOBBY_CREATE_REQUEST:
			create_lobby(gs, (struct lobby_create_request *)msg_header, session);
			break;
		case LOBBY_JOIN_REQUEST:
			join_lobby(gs, (struct lobby_join_request *)msg_header, session);
			break;
		case LOBBY_LIST_REQUEST: {
			list_lobbies(gs, (struct lobby_list_request *)msg_header, session);
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
		/*
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
				messagequeue_add("Session #%d (%d): In Lobby: %d", i, s->id, s->group_id);
			}
			messagequeue_add("Total: %d Sessions", sessions_len);
		}
		*/

	} else {
		// TODO: remove. just sends the raw input for debugging purposes.
		gameserver_send_raw(&gserver, NULL, (uint8_t *)input, input_len);
		messagequeue_add("[%s] %s", fmt_time(time(NULL)), input);
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

void *wsserver_thread(void *data) {
	const int port = 9124;

	// init server
	if (gameserver_init(&gserver, port)) {
		messagequeue_add("Failed starting Websocket server!");
		return (void *)1;
	}

	// register callbacks
	gserver.callback_on_connect = server_on_connect;
	gserver.callback_on_disconnect = server_on_disconnect;
	gserver.callback_on_message = server_on_message;

	messagequeue_add("Starting Websocket server on :%d...", port);
	
	// run
	gameserver_listen(&gserver);

	// destroy
	gameserver_destroy(&gserver);

	return (void *)0;
}

//
// game logic
//
