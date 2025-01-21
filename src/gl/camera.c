#include "gl/camera.h"

#include <cglm/cglm.h>

void camera_init_default(struct camera *camera, int width, int height) {
	glm_mat4_identity(camera->view);
	glm_translate(camera->view, (vec3){ 0.0f, 0.0f, -1000.0f });
	glm_rotate_x(camera->view, glm_rad(35.0f), camera->view);

	camera_resize_projection(camera, width, height);
}

void camera_resize_projection(struct camera *camera, int width, int height) {
	float aspect = (float)height / width;
	glm_ortho(-width, width, -width * aspect, width * aspect, 1.0f, 2000.0f, camera->projection);
}

// Perspective camera experiment:
/*
void camera_init_default(struct camera *camera, int width, int height) {
	glm_mat4_identity(camera->view);
	glm_translate(camera->view, (vec3){ 0.0f, -100.0f, -2600.0f });
	glm_rotate_x(camera->view, glm_rad(30.0f), camera->view);
	glm_translate(camera->view, (vec3){ 0.0f, 500.0f, 0.0f});

	camera_resize_projection(camera, width, height);
}

void camera_resize_projection(struct camera *camera, int width, int height) {
	float aspect = (float)height / width;
	glm_perspective(glm_rad(40.0f), (float)width / height, 0.1f, 4000.0f, camera->projection);
}
*/

