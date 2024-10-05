#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <ncurses.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stb_ds.h>
#include "gameserver.h"
#include "net/message.h"
#include "services/services.h"

#define MAX_INPUT_BYTES 4096  // How many bytes the admin can write into the input box.

//
// logic
//

//
// functions
//

static void serverui_on_input(const char *cmd, size_t cmd_len);
static int serverui_is_input_command(const char *input, const char *command_name);

static void *thread_run_gameserver(void *data);

// ui
static WINDOW *create_messages_window(void);
static WINDOW *create_input_window(void);

static const char *fmt_time(time_t time);
static const char *fmt_date(time_t rawtime);

static void console_log(char *fmt, ...);

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
	console_log("Server started with PID %ld at %s. Press Ctrl-c to exit.", (long)getpid(), fmt_date(time(NULL)));

	// start bg services
	pthread_t gameserver_thread;
	pthread_create(&gameserver_thread, NULL, thread_run_gameserver, NULL);

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
		if (in_char == 127 || in_char == 8) {
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
			console_log("Quitting...");
			gameserver_shutdown(&gserver);
			running = 0;
		} else if (in_char == '\n') {
			serverui_on_input(in_command, in_command_i);
			werase(win_input);
			in_command_i = 0;
		} else if (in_char != ERR) {
			in_command[in_command_i++] = in_char;
		}
	}

	pthread_join(gameserver_thread, NULL);
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
	console_log("%s Client %d connected.", gameserver_session_connection_type(session), session->id);

	// send a welcome message
	struct welcome_response res;
	message_header_init(&res.header, WELCOME_RESPONSE);
	res._dummy = 1337;
	gameserver_send_to(gs, (struct message_header *)&res, session);
}

static void server_on_disconnect(struct gameserver *gs, struct session *session) {
	console_log("Client %d disconnected.", session->id);
}

static void server_on_message(struct gameserver *gs, struct session *session, struct message_header *message) {
	console_log("Received %s from #%06d", message_type_to_name(message->type), session->id);
	services_dispatcher(gs, message, session);
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
		console_log("%s", input);

		if (serverui_is_input_command(input, "help")) {
			console_log("- /help [cmd]    :  Get help for a specific command.");
			console_log("- /status        :  Display server status info.");
			console_log("- /clear         :  Clear the screen, same as <C-l>.");
			console_log("- /logs [on/off] :  Enable or disable writing logs to disk.");
		} else if (serverui_is_input_command(input, "status")) {
			console_log("Status: OK");
			console_log("PID: %d", getpid());
			console_log("Clients: %d", stbds_arrlen(gserver.sessions));
			for (int i = 0; i < stbds_arrlen(gserver.sessions); ++i) {
				struct session *s = gserver.sessions[i];
				console_log(" - %d <%s>", s->id, gameserver_session_connection_type(s));
			}
		} else if (serverui_is_input_command(input, "clear")) {
			console_log("Not implemented :(");
		} else {
			console_log("Unrecognized command. See available commands with /help");
		}
		/*
		if (strncmp(input, "/queue", strlen("/queue")) == 0) {
			for (int i = 0; i < stbds_arrlen(sessions); ++i) {
				struct session *s = sessions[i];
				if (s->msg_queue != NULL) {
					console_log("Outstanding messages for client %d:", s->id);
					for (int j = 0; j < stbds_arrlen(s->msg_queue); ++j) {
						console_log(" - %s", s->msg_queue[i]);
					}
				} else {
					console_log("No Message Queue for %d:", s->id);
				}
			}
		}

		if (strncmp(input, "/sessions", strlen("/sessions")) == 0) {
			const int sessions_len = stbds_arrlen(sessions);
			for (int i = 0; i < sessions_len; ++i) {
				const struct session *s = sessions[i];
				console_log("Session #%d (%d): In Lobby: %d", i, s->id, s->group_id);
			}
			console_log("Total: %d Sessions", sessions_len);
		}
		*/

	} else {
		if (input_len > 1) {
			// TODO: remove. just sends the raw input for debugging purposes.
			gameserver_send_raw(&gserver, NULL, (uint8_t *)input, input_len);
			console_log("%s", input);
		}
	}
}

static int serverui_is_input_command(const char *input, const char *cmd) {
	int cmd_len = strnlen(cmd, MAX_INPUT_BYTES);
	int input_len = strnlen(input, MAX_INPUT_BYTES);
	char *input_cmd = NULL;
	for (int i = 0; i < input_len; ++i) {
		if (input[i] == '/' && (i + 1) < input_len) {
			input_cmd = &input[i + 1];
			break;
		}
	}
	if (input_cmd == NULL) {
		return 0;
	}

	int is_cmd_equal = (strncmp(input_cmd, cmd, cmd_len) == 0);
	
	char *input_cmd_after_end = input_cmd + cmd_len;
	int is_cmd_terminated = (*input_cmd_after_end == ' ' || *input_cmd_after_end == '\0');
	return (is_cmd_equal && is_cmd_terminated);
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

static void console_log(char *fmt, ...) {
	va_list args;

	// determine message size
	va_start(args, fmt);
	int message_len = vsnprintf(NULL, 0, fmt, args);
	char *message = malloc((message_len + 1) * sizeof(char));
	message[message_len] = '\0';
	va_end(args);

	va_start(args, fmt);
	vsnprintf(message, message_len + 1, fmt, args);
	va_end(args);

	char *full_fmt = "[%s]  %s";
	int full_message_len = snprintf(NULL, 0, full_fmt, fmt_time(time(NULL)), message);
	char *full_message = malloc((full_message_len + 1) * sizeof(char));
	snprintf(full_message, full_message_len + 1, full_fmt, fmt_time(time(NULL)), message);
	full_message[full_message_len] = '\0';

	// write to queue
	pthread_mutex_lock(&messagequeue_mutex);
	assert(messagequeue_len < messagequeue_max);
	messagequeue[messagequeue_len++] = full_message;
	pthread_mutex_unlock(&messagequeue_mutex);

}

//
// server impls
//

static void *thread_run_gameserver(void *data) {
	const int port = 9124;

	// init server
	if (gameserver_init(&gserver, port)) {
		console_log("Failed starting Websocket server!");
		return (void *)1;
	}

	// register callbacks
	gserver.callback_on_connect = server_on_connect;
	gserver.callback_on_disconnect = server_on_disconnect;
	gserver.callback_on_message = server_on_message;

	// run
	console_log("Websocket server on :%d...", port);
	gameserver_listen(&gserver);

	// destroy
	gameserver_destroy(&gserver);

	return (void *)0;
}

//
// game logic
//
