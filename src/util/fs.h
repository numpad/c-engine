#ifndef FS_H
#define FS_H

#define FS_OK    0
#define FS_ERROR 1

int fs_readfile(const char *path, char **output, long *size);

#endif

