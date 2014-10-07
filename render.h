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
	int flat_offset;;
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
	GLuint flat_a_light_level;

	struct shader wall_shader;
	GLuint wall_a_pos;
	GLuint wall_a_uv;
	GLuint wall_a_light_level;

	struct shader step_shader;
	GLuint step_a_pos;

	void (*begin_flat)(
		struct render* render,
		struct lvl* lvl,
		int sectori,
		int flati
	);
	void (*add_flat_vertex)(
		struct render* render,
		float x, float y, float z,
		float u, float v,
		float select_u, float select_v,
		float light_level
	);
	void (*add_flat_triangle)(
		struct render* render,
		uint32_t indices[3]
	);
	void (*end_flat)(struct render* render);

	void (*begin_wall)(
		struct render* render,
		struct lvl* lvl,
		int contouri,
		int dz
	);
	void (*add_wall_vertex)(
		struct render* render,
		float x, float y, float z,
		float u, float v,
		float light_level
	);
	void (*end_wall)(struct render* render);

	struct vec3* tags_flat_vertices;
	int tags_flat_vertex_n;
	int* tags_flat_indices;
	int tags_flat_index_n;
};


void render_init(struct render* render, SDL_Window* window);
float render_get_fovy(struct render* render);
void render_set_entity_cam(struct render* render, struct lvl_entity* entity);
void render_lvl_geom(struct render* render, struct lvl* lvl);
void render_begin2d(struct render* render);
void render_flip(struct render* render);
void render_lvl_tags(struct render* render, struct lvl* lvl);

#endif/*RENDER_H*/
