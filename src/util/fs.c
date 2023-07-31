#include "fs.h"

#if defined(__linux__) || defined(__EMSCRIPTEN__)

#include <stdio.h>
#include <stdlib.h>

int fs_readfile(const char *path, char **output, long *size) {
	if (size == NULL) {
		return FS_ERROR;
	}

	FILE* fp = fopen(path, "r");
	if (fp == NULL) {
		return FS_ERROR;
	}

	fseek(fp, 0, SEEK_END);
	*size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	*output = malloc(*size * sizeof(char) + 1);
	fread(*output, sizeof(char), *size, fp);
	(*output)[*size] = '\0';

	fclose(fp);

	return FS_OK;
}

#else

#error "filesystem not supported on the current platform..."

#endif

