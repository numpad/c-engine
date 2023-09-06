#ifndef COMPONENTS_H
#define COMPONENTS_H

typedef struct {
	char *name;
	int image_id;
} c_card ;

typedef struct {
	int selected;
	float drag_x, drag_y;
} c_handcard;

#endif

