#ifndef BACKGROUND_H
#define BACKGROUND_H

struct engine_s;

// init
void background_set_image(const char *filename);
void background_set_parallax(const char *layers_filename_fmt, int layers_count);
void background_destroy(void);

// draw
void background_draw(void);

#endif

