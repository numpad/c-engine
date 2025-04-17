#ifndef CAMERA_H
#define CAMERA_H

#include <cglm/cglm.h>
#include <cglm/types-struct.h>

struct camera {
	mat4 projection;
	mat4 view;
	vec3s position;
	float z_near;
	float z_far;
	vec3s view_offset;
};

void camera_init_default(struct camera *, int width, int height);
void camera_resize_projection(struct camera *, int width, int height);

/** Updates view matrix after changing position/rotation */
void camera_transform_changed(struct camera *);

#endif

