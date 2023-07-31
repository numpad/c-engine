#ifndef SHADER_H
#define SHADER_H

int shader_new(const char *vert_path, const char *frag_path);
void shader_delete(int program);

#endif

