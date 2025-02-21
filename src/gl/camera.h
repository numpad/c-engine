#ifndef CAMERA_H
#define CAMERA_H

#include <cglm/mat4.h>

struct camera {
	mat4 projection;
	mat4 view;
	float z_near;
	float z_far;
};

void camera_init_default(struct camera *, int width, int height);
void camera_resize_projection(struct camera *, int width, int height);

#endif

