#ifndef RENDER_H
#define RENDER_H

#include <SDL.h>

#include "shader.h"
#include "lvl.h"

#define MAX_WALLS (1024)
#define MAX_SPRITES (4096)

struct render_texture {
	GLuint texture;
	int width;
	int height;
};

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
	float current_select_u, current_select_v, current_light_level;

	struct render_texture walls[MAX_WALLS];
	struct render_texture sprites[MAX_SPRITES];

	GLuint type0_vertex_buffer;
	GLuint type0_index_buffer;
	float* type0_vertex_data;
	int32_t* type0_index_data;
	int type0_vertex_n;
	int type0_index_n;

	int wall_current_texture, wall_next_texture;
	int wall_enable;

	struct lvl_entity* entity_cam;

	GLuint palette_lookup_texture;
	GLuint flatlas_texture;

	struct shader flat_shader;
	GLuint flat_a_pos;
	GLuint flat_a_uv;
	GLuint flat_a_selector;
	GLuint flat_a_light_level;

	struct shader type0_shader;
	GLuint type0_a_pos;
	GLuint type0_a_uv;
	GLuint type0_a_light_level;

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
		float u, float v
	);
	void (*add_flat_triangle)(
		struct render* render,
		uint32_t indices[3]
	);
	void (*end_flat)(struct render* render);

	void (*begin_wall)(
		struct render* render,
		struct lvl* lvl,
		int sectori,
		int contouri,
		int dz,
		int nvertices
	);
	void (*add_wall_vertex)(
		struct render* render,
		float x, float y, float z,
		float u, float v
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
void render_lvl(struct render* render, struct lvl* lvl);
void render_begin2d(struct render* render);
void render_flip(struct render* render);
void render_lvl_tags(struct render* render, struct lvl* lvl);

#endif/*RENDER_H*/
