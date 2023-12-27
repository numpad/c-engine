#ifndef SHADER_H
#define SHADER_H

//
// typedefs
//

// TODO: impl shader struct

//
// public api
//

// init & destroy
int shader_new(const char *vert_path, const char *frag_path);
int shader_from_dir(const char *dir_path);
void shader_delete(int program);

// uniform setters
void shader_set_uniform_mat4(int shader, const char *attribname, float[16]);

#endif

