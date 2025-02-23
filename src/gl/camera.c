#include "gl/camera.h"

void camera_init_default(struct camera *camera, int width, int height) {
	camera->z_near = 1.0f;
	camera->z_far  = 50.0f;
	camera->position.x = 0.0f;
	camera->position.y = 0.0f;
	camera->position.z = -25.0f;
	glm_mat4_identity(camera->view);
	glm_translate(camera->view, camera->position.raw);
	glm_rotate_x(camera->view, glm_rad(35.0f), camera->view);

	camera_resize_projection(camera, width, height);
}

void camera_resize_projection(struct camera *camera, int width, int height) {
	float aspect = (float)height / width;
	float size = width / 40.0f;
	glm_ortho(-size, size, -size * aspect, size * aspect, camera->z_near, camera->z_far, camera->projection);
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

