#ifndef BACKGROUND_H
#define BACKGROUND_H

struct engine;

// init
void background_set_parallax(const char *layers_filename_fmt, int layers_count);
void background_destroy(void);

// draw
void background_draw(struct engine *);

// api
void background_set_parallax_offset(float y);

#endif

