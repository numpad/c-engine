#ifndef FS_H
#define FS_H

#define FS_OK    0
#define FS_ERROR 1

typedef struct cJSON cJSON;

int fs_readfile(const char *path, char **output, long *size);

int fs_writefile_json(const char *path, const cJSON *json);

#endif

