#ifndef LVL_H
#define LVL_H

#include <stdint.h>
#include "m.h"

#define LVL_HIGHLIGHTED_ZPLUS (1<<0)
#define LVL_SELECTED_ZPLUS (1<<1)
#define LVL_HIGHLIGHTED_ZMINUS (1<<2)
#define LVL_SELECTED_ZMINUS (1<<3)

#define LVL_CONTOUR_IS_FIRST(c) (c->usr & 2)
#define LVL_CONTOUR_IS_LAST(c) (c->usr & 4)

struct lvl_flat {
	struct plane plane; // local to vertex 0
	int32_t texture;
	struct mat23 tx;
};

struct lvl_sector {
	struct lvl_flat flat[2];

	float light_level;
	uint32_t usr;
	int32_t contour0, contourn; // (derived)
};

struct lvl_linedef {
	int32_t vertex[2];
	int32_t sidedef[2];
	uint32_t usr;
	//uint32_t flags;
};

struct lvl_sidedef {
	int32_t sector;
	uint32_t usr;
	int32_t texture[2];
	struct mat23 tx[2];
};

struct lvl_contour {
	uint32_t linedef;
	uint32_t usr;
};

#define ENTITY_DELETED (-1)

struct lvl_entity {
	int32_t type;
	struct vec2 position;
	float z;
	float yaw;
	float pitch;
	int32_t sector;
};

struct lvl {
	uint32_t n_sectors, reserved_sectors;
	struct lvl_sector* sectors;

	uint32_t n_linedefs, reserved_linedefs;
	struct lvl_linedef* linedefs;

	uint32_t n_sidedefs, reserved_sidedefs;
	struct lvl_sidedef* sidedefs;

	uint32_t n_vertices, reserved_vertices;
	struct vec2* vertices;

	uint32_t n_contours, reserved_contours;
	struct lvl_contour* contours;

	uint32_t n_entities, reserved_entities;
	struct lvl_entity* entities;
};

void lvl_init(struct lvl*);

uint32_t lvl_new_entity(struct lvl*);
struct lvl_entity* lvl_get_entity(struct lvl*, int32_t i);

uint32_t lvl_new_sector(struct lvl*);
struct lvl_sector* lvl_get_sector(struct lvl*, int32_t i);

uint32_t lvl_new_linedef(struct lvl* lvl);
struct lvl_linedef* lvl_get_linedef(struct lvl* lvl, int32_t i);

uint32_t lvl_new_sidedef(struct lvl* lvl);
struct lvl_sidedef* lvl_get_sidedef(struct lvl* lvl, int32_t i);

uint32_t lvl_new_vertex(struct lvl*);
uint32_t lvl_top_vertex(struct lvl* lvl);
struct vec2* lvl_get_vertex(struct lvl*, int32_t i);

struct lvl_contour* lvl_get_contour(struct lvl* lvl, int32_t i);


void lvl_entity_update_sector(struct lvl* lvl, struct lvl_entity* entity);

void lvl_entity_clipmove(struct lvl* lvl, struct lvl_entity* entity, struct vec2* move);

void lvl_build_contours(struct lvl* lvl);

float lvl_entity_radius(struct lvl_entity* entity);

void lvl_entity_view_plane(struct lvl_entity* entity, struct vec3* pos, struct vec3* dir, struct vec3* right, struct vec3* up);
void lvl_entity_mouse(struct lvl_entity* entity, struct vec3* pos, struct vec3* mdir, float fovy, int mouse_x, int mouse_y, int width, int height);

struct lvl_trace_result {
	struct vec3 position;
	int z;
	int32_t linedef;
	int32_t sidedef;
	int32_t sector;
};

int lvl_trace(
	struct lvl* lvl,
	int32_t sector,
	struct vec3* origin,
	struct vec3* ray,
	struct lvl_trace_result* result);

int lvl_sector_inside(struct lvl* lvl, int32_t sectori, struct vec2* p);
int32_t lvl_sector_find(struct lvl* lvl, struct vec2* p);

void lvl_tag_clear_highlights(struct lvl* lvl);
void lvl_tag_flats(struct lvl* lvl, struct lvl_trace_result* trace, int clicked);
void lvl_tag_sectors(struct lvl* lvl, struct lvl_trace_result* trace, int clicked);
void lvl_tag_sidedefs(struct lvl* lvl, struct lvl_trace_result* trace, int clicked);

#endif//LVL_H
