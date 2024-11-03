#include "server/errors.h"
#include <assert.h>

const char *server_error_description(enum server_error error) {
	assert(error >= SERVER_NO_ERROR && error <= SERVER_ERROR_UNKNOWN);

	switch (error) {
	case SERVER_NO_ERROR                  : return "Success";
	// lobbies
	case SERVER_ERROR_ALREADY_IN_A_LOBBY  : return "Already in a lobby.";
	case SERVER_ERROR_LOBBY_ALREADY_EXISTS: return "Lobby already exists.";
	case SERVER_ERROR_LOBBY_DOES_NOT_EXIST: return "Lobby does not exist.";
	// ...
	case SERVER_ERROR_UNKNOWN:
	default:
		return "Unknown error";
	};

	return "UNEXPECTED ERROR";
}

