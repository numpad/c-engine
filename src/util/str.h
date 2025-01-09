#ifndef CENGINE_STR_H
#define CENGINE_STR_H

#include <stddef.h>

void str_free(char *c_string);
char *str_copy(char *c_string);

int str_path_replace_filename(const char *path_with_filename, const char *new_filename, size_t max_output_chars, char *output);

#endif

