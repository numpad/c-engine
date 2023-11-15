#include "fs.h"

#include <cJSON.h>


#if defined(__linux__) || defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int fs_readfile(const char *path, char **output, long *size) {
	if (size == NULL) {
		return FS_ERROR;
	}

	FILE *fp = fopen(path, "r");
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

int fs_writefile_json(const char *path, const cJSON *json) {
	FILE *fp = fopen(path, "w+");
	if (fp == NULL) {
		return FS_ERROR;
	}

	fseek(fp, 0, SEEK_SET);

	// convert json object to string
#if DEBUG
	char *output = cJSON_Print(json);
#else
	char *output = cJSON_PrintUnformatted(json);
#endif
	
	fwrite(output, sizeof(char), strlen(output), fp);
	free(output);
	fclose(fp);

	return FS_OK;
}

#else

#error "filesystem not supported on the current platform..."

#endif

