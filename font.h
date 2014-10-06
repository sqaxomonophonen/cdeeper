#ifndef FONT_H
#define FONT_H

#include <GL/glew.h>
#include "shader.h"

struct font {
	GLuint font6_texture;

	struct shader shader;
	GLuint a_pos;
	GLuint a_uv;
	GLuint a_col;

	GLuint vertex_buffer;
	float* vertex_data;
	int vertex_n;

	GLuint index_buffer;
	int32_t* index_data;

	int face;
	float color0, color1;
	int cursor_x;
	int cursor_y;
	int cursor_dx;
};

void font_init(struct font* font);
void font_begin(struct font* font, int face);
void font_end(struct font* font);

void font_color(struct font* font, int color);
void font_goto(struct font* font, int x, int y);
void font_printf(struct font* font, const char* fmt, ...) __attribute__((format (printf, 2, 3)));

#endif/*FONT_H*/
