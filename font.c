#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "a.h"
#include "font.h"
#include "mud.h"

#define FLOATS_PER_VERTEX (8)

#define RENDER_BUFSZ (16384)

static const char* font_shader_vertex_src =
	"#version 130\n"
	"\n"
	"attribute vec2 a_pos;\n"
	"attribute vec2 a_uv;\n"
	"attribute vec4 a_col;\n"
	"\n"
	"varying vec2 v_uv;\n"
	"varying vec4 v_col;\n"
	"\n"
	"void main()\n"
	"{\n"
	"	v_uv = a_uv;\n"
	"	v_col = a_col;\n"
	"	gl_Position = gl_ModelViewProjectionMatrix * vec4(a_pos, 0, 1);\n"
	"}\n";

static const char* font_shader_fragment_src =
	"#version 130\n"
	"\n"
	"varying vec2 v_uv;\n"
	"varying vec4 v_col;\n"
	"\n"
	"uniform sampler2D u_font_texture;\n"
	"\n"
	"void main(void)\n"
	"{\n"
	"	float sample = texture2D(u_font_texture, v_uv).r;\n"
	"	gl_FragColor = sample > 0.5 ? v_col : vec4(0,0,0,0);\n"
	"}\n";

static void font_init_font6_texture(struct font* font)
{
	uint8_t* data;
	int width = 0;
	int height = 0;
	AZ(mud_load_png_paletted("gfx/font6.png", &data, &width, &height));
	ASSERT(width == 96);
	ASSERT(height == 96);

	int level = 0;
	int border = 0;

	glGenTextures(1, &font->font6_texture); CHKGL;
	glBindTexture(GL_TEXTURE_2D, font->font6_texture); CHKGL;
	glTexImage2D(GL_TEXTURE_2D, level, 1, width, height, border, GL_RED, GL_UNSIGNED_BYTE, data); CHKGL;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); CHKGL;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); CHKGL;

	free(data);
}

static void font_init_shader(struct font* font)
{
	shader_init(&font->shader, font_shader_vertex_src, font_shader_fragment_src);
	shader_use(&font->shader);
	font->a_pos = glGetAttribLocation(font->shader.program, "a_pos"); CHKGL;
	font->a_uv = glGetAttribLocation(font->shader.program, "a_uv"); CHKGL;
	font->a_col = glGetAttribLocation(font->shader.program, "a_col"); CHKGL;

	glUniform1i(glGetUniformLocation(font->shader.program, "u_font_texture"), 0); CHKGL;
}

static void font_init_buffers(struct font* font)
{
	glGenBuffers(1, &font->vertex_buffer); CHKGL;
	glBindBuffer(GL_ARRAY_BUFFER, font->vertex_buffer); CHKGL;
	size_t vertex_data_sz = RENDER_BUFSZ * sizeof(float) * FLOATS_PER_VERTEX;
	font->vertex_data = malloc(vertex_data_sz);
	AN(font->vertex_data);
	glBufferData(GL_ARRAY_BUFFER, vertex_data_sz, font->vertex_data, GL_STREAM_DRAW); CHKGL;

	glGenBuffers(1, &font->index_buffer); CHKGL;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, font->index_buffer); CHKGL;
	size_t index_data_sz = RENDER_BUFSZ * sizeof(uint32_t);
	font->index_data = malloc(index_data_sz);
	AN(font->index_data);
	int offset = 0;
	for (int i = 0; i < (RENDER_BUFSZ-6); i += 6) {
		font->index_data[i+0] = 0 + offset;
		font->index_data[i+1] = 1 + offset;
		font->index_data[i+2] = 2 + offset;
		font->index_data[i+3] = 0 + offset;
		font->index_data[i+4] = 2 + offset;
		font->index_data[i+5] = 3 + offset;
		offset += 4;
	}
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_data_sz, font->index_data, GL_STATIC_DRAW); CHKGL;
}

void font_init(struct font* font)
{
	font_init_font6_texture(font);
	font_init_shader(font);
	font_init_buffers(font);
}

void font_begin(struct font* font, int face)
{
	ASSERT(face == 6);
	font->face = face;
	font->vertex_n = 0;
}

void font_end(struct font* font)
{
	ASSERT(font->face == 6);

	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	shader_use(&font->shader);

	glActiveTexture(GL_TEXTURE0); CHKGL;
	glEnable(GL_TEXTURE_2D); CHKGL;
	glBindTexture(GL_TEXTURE_2D, font->font6_texture); CHKGL;

	glBindBuffer(GL_ARRAY_BUFFER, font->vertex_buffer); CHKGL;
	glBufferSubData(GL_ARRAY_BUFFER, 0, font->vertex_n * sizeof(float) * FLOATS_PER_VERTEX, font->vertex_data); CHKGL;

	glEnableVertexAttribArray(font->a_pos); CHKGL;
	glVertexAttribPointer(font->a_pos, 2, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_VERTEX, 0); CHKGL;

	glEnableVertexAttribArray(font->a_uv); CHKGL;
	glVertexAttribPointer(font->a_uv, 2, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_VERTEX, (char*)(sizeof(float)*2)); CHKGL;

	glEnableVertexAttribArray(font->a_col); CHKGL;
	glVertexAttribPointer(font->a_col, 4, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_VERTEX, (char*)(sizeof(float)*4)); CHKGL;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, font->index_buffer); CHKGL;
	glDrawElements(GL_TRIANGLES, font->vertex_n/4*6, GL_UNSIGNED_INT, NULL); CHKGL;

	glDisableVertexAttribArray(font->a_col); CHKGL;
	glDisableVertexAttribArray(font->a_uv); CHKGL;
	glDisableVertexAttribArray(font->a_pos); CHKGL;

	glActiveTexture(GL_TEXTURE0); CHKGL;
	glDisable(GL_TEXTURE_2D); CHKGL;
}

void font_color(struct font* font, int color)
{
	font->color0 = font->color1 = (float)color/255.0;
}

void font_goto(struct font* font, int x, int y)
{
	font->cursor_x = x;
	font->cursor_y = y;
	font->cursor_dx = 0;
}

static int font_line_spacing(struct font* font)
{
	ASSERT(font->face == 6);
	return 6;
}

static int font_glyph_width(struct font* font)
{
	ASSERT(font->face == 6);
	return 6;
}

static int font_glyph_height(struct font* font)
{
	ASSERT(font->face == 6);
	return 6;
}

void font_printf(struct font* font, const char* fmt, ...)
{
	char buffer[16384];
	va_list args;
	va_start(args, fmt);
	int len = vsprintf(buffer, fmt, args);
	va_end(args);

	for (int i = 0; i < len; i++) {
		char ch = buffer[i];

		if (ch < 32) {
			switch (ch) {
				case '\n':
					font_goto(font, font->cursor_x, font->cursor_y + font_line_spacing(font));
					break;
				case '\r':
					font_goto(font, font->cursor_x, font->cursor_y);
					break;
			}

		} else {
			int x0 = ch & 0x0f;
			int y0 = ch >> 4;
			float u = (float)x0 / 16.0;
			float v = (float)y0 / 16.0;
			float w = 1.0 / 16.0;

			ASSERT((font->vertex_n+4) <= RENDER_BUFSZ);

			float* f = &font->vertex_data[font->vertex_n * FLOATS_PER_VERTEX];

			int i = 0;

			int x = font->cursor_x + font->cursor_dx;
			int y = font->cursor_y;

			f[i++] = x;
			f[i++] = y;
			f[i++] = u;
			f[i++] = v;
			f[i++] = font->color0; f[i++] = 0; f[i++] = 0; f[i++] = 1;

			f[i++] = x + font_glyph_width(font);
			f[i++] = y;
			f[i++] = u + w;
			f[i++] = v;
			f[i++] = font->color0; f[i++] = 0; f[i++] = 0; f[i++] = 1;

			f[i++] = x + font_glyph_width(font);
			f[i++] = y + font_glyph_height(font);
			f[i++] = u + w;
			f[i++] = v + w;
			f[i++] = font->color1; f[i++] = 0; f[i++] = 0; f[i++] = 1;

			f[i++] = x;
			f[i++] = y + font_glyph_height(font);
			f[i++] = u;
			f[i++] = v + w;
			f[i++] = font->color1; f[i++] = 0; f[i++] = 0; f[i++] = 1;

			font->cursor_dx += font_glyph_width(font);
			font->vertex_n += 4;
		}
	}
}

