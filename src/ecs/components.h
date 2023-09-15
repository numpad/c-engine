#ifndef COMPONENTS_H
#define COMPONENTS_H

// general information about a card
typedef struct {
	char *name;
	int image_id;
} c_card ;

// a cards state when held in hand
typedef struct {
	int selected;
	float drag_x, drag_y;
} c_handcard;

// position on the isoterrain grid
typedef struct {
	int x, y, z;
} c_blockpos;

#endif

