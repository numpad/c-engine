#include "str.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "util/util.h"

void str_free(char *c_string) {
	free(c_string);
}

char *str_copy(char *c_string) {
	usize len = strlen(c_string);
	char *copy = malloc((len + 1) * sizeof(char));
	strncpy(copy, c_string, len);
	copy[len] = '\0';
	return copy;
}

int str_path_replace_filename(const char *path_with_filename, const char *new_filename, size_t max_output_chars, char *output) {
	assert(path_with_filename != NULL);
	assert(new_filename != NULL);
	assert(output != NULL);

	const char *last_separator = strrchr(path_with_filename, '/');
	size_t base_path_len;

	if (last_separator) {
		base_path_len = last_separator - path_with_filename + 1;
	} else {
		// No '/' found, assume current directory
		base_path_len = 0;
	}

	// Calculate the required length for output
	size_t total_length = base_path_len + strlen(new_filename);

	if (total_length >= max_output_chars) {
		return -1;
	}

	// Copy the base path to the output
	if (base_path_len > 0) {
		strncpy(output, path_with_filename, base_path_len);
	}

	// Append the new filename to the output
	strcpy(output + base_path_len, new_filename);

	return 0;
}

