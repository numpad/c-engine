#ifndef GL_TEXT_H
#define GL_TEXT_H

typedef struct texture_s texture_t;

void text_global_init(void);
void text_global_destroy(void);

void text_render(void);
texture_t *text_get_atlas(void);
void text_get_charsize(int *w, int *h);

#endif

