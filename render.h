#ifndef RENDER_H
#define RENDER_H

#include <SDL.h>

#include "shader.h"
#include "lvl.h"

struct render {
	SDL_Window* window;

	GLuint screen_framebuffer;
	GLuint screen_texture;
	GLuint screen_depthbuffer;

	GLuint step_framebuffer;
	GLuint step_texture;

	GLuint step_vertex_buffer;
	GLuint step_index_buffer;

	GLuint flat_vertex_buffer;
	GLuint flat_index_buffer;
	float* flat_vertex_data;
	int32_t* flat_index_data;
	int flat_vertex_n;
	int flat_index_n;

	GLuint wall0_texture;

	GLuint wall_vertex_buffer;
	GLuint wall_index_buffer;
	float* wall_vertex_data;
	int32_t* wall_index_data;
	int wall_vertex_n;

	struct lvl_entity* entity_cam;

	GLuint palette_lookup_texture;
	GLuint flatlas_texture;

	struct shader flat_shader;
	GLuint flat_a_pos;
	GLuint flat_a_uv;
	GLuint flat_a_selector;

	struct shader wall_shader;
	GLuint wall_a_pos;
	GLuint wall_a_uv;

	struct shader step_shader;
	GLuint step_a_pos;
};


void render_init(struct render* render, SDL_Window* window);
float render_get_fovy(struct render* render);
void render_set_entity_cam(struct render* render, struct lvl_entity* entity);
void render_lvl(struct render* render, struct lvl* lvl);

#endif/*RENDER_H*/
