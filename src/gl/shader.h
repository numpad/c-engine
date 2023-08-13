#ifndef SHADER_H
#define SHADER_H

int shader_new(const char *vert_path, const char *frag_path);

/**
 * Load a shader program from a directory.
 *
 * The directory structure expects at least the files "vertex.glsl" and "fragment.glsl".
 */
int shader_from_directory(const char *directory);

void shader_delete(int program);

#endif

