#ifndef INPUT_H
#define INPUT_H

#define INPUT_DRAG_IS_DOWN(input_drag) \
	(input_drag.state == INPUT_DRAG_BEGIN || input_drag.state == INPUT_DRAG_IN_PROGRESS)

#define INPUT_DRAG_IS_UP(input_drag) \
	(input_drag.state == INPUT_DRAG_END || input_drag.state == INPUT_DRAG_NONE)

enum INPUT_DRAG_STATE {
	INPUT_DRAG_NONE = 0,
	INPUT_DRAG_BEGIN,
	INPUT_DRAG_IN_PROGRESS,
	INPUT_DRAG_END,
};

struct input_drag_s {
	enum INPUT_DRAG_STATE state;

	float begin_x, begin_y;
	float x, y;
	float end_x, end_y;
};

#endif

