#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "a.h"
#include "lvl.h"
#include "magic.h"

void lvl_init(struct lvl* lvl)
{
	#if 0
	printf("sizeof(struct lvl_sector) = %zd\n", sizeof(struct lvl_sector));
	printf("sizeof(struct lvl_linedef) = %zd\n", sizeof(struct lvl_linedef));
	printf("sizeof(struct lvl_sidedef) = %zd\n", sizeof(struct lvl_sidedef));
	printf("sizeof(struct vec2) = %zd\n", sizeof(struct vec2));
	#endif

	memset(lvl, 0, sizeof(struct lvl));

	const int more_than_I_will_ever_need = 32768;

	lvl->reserved_sectors = more_than_I_will_ever_need / 2;
	lvl->sectors = calloc(lvl->reserved_sectors, sizeof(struct lvl_sector));

	lvl->reserved_linedefs = more_than_I_will_ever_need;
	lvl->linedefs = calloc(lvl->reserved_linedefs, sizeof(struct lvl_linedef));

	lvl->reserved_sidedefs = more_than_I_will_ever_need;
	lvl->sidedefs = calloc(lvl->reserved_sidedefs, sizeof(struct lvl_sidedef));

	lvl->reserved_vertices = more_than_I_will_ever_need * 2;
	lvl->vertices = calloc(lvl->reserved_vertices, sizeof(struct vec2));

	lvl->reserved_contours = more_than_I_will_ever_need;
	lvl->contours = calloc(lvl->reserved_contours, sizeof(struct lvl_contour));

	lvl->reserved_entities = more_than_I_will_ever_need;
	lvl->entities = calloc(lvl->reserved_entities, sizeof(struct lvl_entity));
}

static void lvl_entity_init(struct lvl_entity* e)
{
	memset(e, 0, sizeof(struct lvl_entity));
}


uint32_t lvl_new_entity(struct lvl* lvl)
{
	ASSERT(lvl->n_entities < lvl->reserved_entities);
	uint32_t newi = lvl->n_entities++;
	lvl_entity_init(lvl_get_entity(lvl, newi));
	return newi;
}


struct lvl_entity* lvl_get_entity(struct lvl* lvl, int32_t i)
{
	ASSERT(i >= 0 && i < lvl->n_entities);
	return &lvl->entities[i];
}

static void lvl_sector_init(struct lvl_sector* sector)
{
	sector->contour0 = -1;
	sector->usr = 0;
	//sector->floor.material = -1;
	//sector->ceiling.material = -1;
}

uint32_t lvl_new_sector(struct lvl* lvl)
{
	ASSERT(lvl->n_sectors < lvl->reserved_sectors);
	uint32_t newi = lvl->n_sectors++;
	lvl_sector_init(lvl_get_sector(lvl, newi));
	return newi;
}

struct lvl_sector* lvl_get_sector(struct lvl* lvl, int32_t i)
{
	ASSERT(i >= 0 && i < lvl->n_sectors);
	return &lvl->sectors[i];
}

static void lvl_linedef_init(struct lvl_linedef* linedef)
{
	linedef->vertex[0] = linedef->vertex[1] = -1;
	linedef->sidedef[0] = linedef->sidedef[1] = -1;
	linedef->usr = 0;
}

uint32_t lvl_new_linedef(struct lvl* lvl)
{
	ASSERT(lvl->n_linedefs < lvl->reserved_linedefs);
	uint32_t newi = lvl->n_linedefs++;
	lvl_linedef_init(lvl_get_linedef(lvl, newi));
	return newi;
}

struct lvl_linedef* lvl_get_linedef(struct lvl* lvl, int32_t i)
{
	ASSERT(i >= 0 && i < lvl->n_linedefs);
	return &lvl->linedefs[i];
}


static void lvl_sidedef_init(struct lvl_sidedef* sidedef)
{
	sidedef->sector = -1;
	//for (int i = 0; i < 3; i++) sidedef->material[i] = -1;
}

uint32_t lvl_new_sidedef(struct lvl* lvl)
{
	ASSERT(lvl->n_sidedefs < lvl->reserved_sidedefs);
	uint32_t newi = lvl->n_sidedefs++;
	lvl_sidedef_init(lvl_get_sidedef(lvl, newi));
	return newi;
}

struct lvl_sidedef* lvl_get_sidedef(struct lvl* lvl, int32_t i)
{
	ASSERT(i >= 0 && i < lvl->n_sidedefs);
	return &lvl->sidedefs[i];
}

uint32_t lvl_new_vertex(struct lvl* lvl)
{
	ASSERT(lvl->n_vertices < lvl->reserved_vertices);
	return lvl->n_vertices++;
}

uint32_t lvl_top_vertex(struct lvl* lvl)
{
	return lvl->n_vertices;
}

struct vec2* lvl_get_vertex(struct lvl* lvl, int32_t i)
{
	ASSERT(i >= 0 && i < lvl->n_vertices);
	return &lvl->vertices[i];
}

struct lvl_contour* lvl_get_contour(struct lvl* lvl, int32_t i)
{
	ASSERT(i >= 0 && i < lvl->n_contours);
	return &lvl->contours[i];
}


static void add_contour(struct lvl* lvl, uint32_t linedef, uint32_t usr)
{
	ASSERT((lvl->n_contours + 1) < lvl->reserved_contours);
	int32_t i = lvl->n_contours++;
	struct lvl_contour* contour = lvl_get_contour(lvl, i);
	contour->linedef = linedef;
	contour->usr = usr;
}

static struct lvl* GLOBAL_lvl;
static uint32_t vcntr_resolve_sector(const void* vcntr)
{
	struct lvl_contour* c = (struct lvl_contour*) vcntr;
	struct lvl_linedef* ld = lvl_get_linedef(GLOBAL_lvl, c->linedef);
	uint32_t sdi = ld->sidedef[c->usr & 1];
	ASSERT(sdi != -1);
	struct lvl_sidedef* sd = lvl_get_sidedef(GLOBAL_lvl, sdi);
	ASSERT(sd->sector != -1);
	return sd->sector;
}

static int vcntr_cmp(const void* vcntr0, const void* vcntr1)
{
	return vcntr_resolve_sector(vcntr0) - vcntr_resolve_sector(vcntr1);
}

static void lvl_swap_contours(struct lvl* lvl, int32_t i0, int32_t i1)
{
	struct lvl_contour tmp;
	memcpy(&tmp, lvl_get_contour(lvl, i0), sizeof(struct lvl_contour));
	memcpy(lvl_get_contour(lvl, i0), lvl_get_contour(lvl, i1), sizeof(struct lvl_contour));
	memcpy(lvl_get_contour(lvl, i1), &tmp, sizeof(struct lvl_contour));
}

#define MARK_CONTOUR_FIRST(c) (c->usr |= 2)
#define MARK_CONTOUR_LAST(c) (c->usr |= 4)

void lvl_build_contours(struct lvl* lvl)
{
	// clear sectors
	for (int i = 0; i < lvl->n_sectors; i++) {
		struct lvl_sector* sector = lvl_get_sector(lvl, i);
		sector->contour0 = -1;
		sector->contourn = 0;
	}

	lvl->n_contours = 0;

	for (int i = 0; i < lvl->n_linedefs; i++) {
		struct lvl_linedef* linedef = lvl_get_linedef(lvl, i);
		ASSERT(linedef->sidedef[0] != -1 || linedef->sidedef[1] != -1);
		for (int side = 0; side < 2; side++) {
			int32_t s = linedef->sidedef[side] == -1 ? -1 : lvl_get_sidedef(lvl, linedef->sidedef[side])->sector;
			if (s != -1) {
				add_contour(lvl, i, side);
			}
		}
	}

	ASSERT(lvl->n_contours >= lvl->n_linedefs);
	ASSERT(lvl->n_contours <= lvl->n_sidedefs);

	// sort contours by sector, placing LVL_CONTOUR_NON last
	GLOBAL_lvl = lvl;
	qsort(lvl->contours, lvl->n_contours, sizeof(struct lvl_contour), vcntr_cmp);

	// assign contours to sectors
	int32_t s0 = -1;
	for (int i = 0; i < lvl->n_contours; i++) {
		struct lvl_contour* c = lvl_get_contour(lvl, i);
		struct lvl_linedef* l = lvl_get_linedef(lvl, c->linedef);
		int sec = lvl_get_sidedef(lvl, l->sidedef[c->usr&1])->sector;
		ASSERT(sec != -1);
		if (sec > s0) {
			lvl_get_sector(lvl, sec)->contour0 = i;
			s0 = sec;
		}
		lvl_get_sector(lvl, s0)->contourn++;
	}

	// sort contours into .. contours! (i.e. proper loops)
	int32_t c0i = 0;
	int32_t vend = -1;
	while (1) {
		ASSERT(c0i <= lvl->n_contours);
		if (c0i == lvl->n_contours) break;

		struct lvl_contour* c0 = lvl_get_contour(lvl, c0i);
		struct lvl_linedef* l0 = lvl_get_linedef(lvl, c0->linedef);
		int32_t sec0 = lvl_get_sidedef(lvl, l0->sidedef[c0->usr&1])->sector;
		int32_t vcurrent = l0->vertex[(c0->usr&1)^1];
		if (vend == -1) {
			MARK_CONTOUR_FIRST(c0);
			vend = l0->vertex[c0->usr&1];
		}

		ASSERT(sec0 != -1);
		ASSERT(vcurrent != -1);
		ASSERT(vend != -1);

		int32_t c1i0 = c0i + 1;
		int32_t c1i = c1i0;

		while (1) {
			ASSERT(c1i <= lvl->n_contours);
			if (c1i == lvl->n_contours) break;

			struct lvl_contour* c1 = lvl_get_contour(lvl, c1i);
			struct lvl_linedef* l1 = lvl_get_linedef(lvl, c1->linedef);

			int32_t sec1 = lvl_get_sidedef(lvl, l1->sidedef[c1->usr&1])->sector;
			int32_t vjoin = l1->vertex[c1->usr&1];
			int32_t vopposite = l1->vertex[(c1->usr&1)^1];

			ASSERT(sec1 != -1);
			ASSERT(vjoin != -1);
			ASSERT(vopposite != -1);

			if (sec1 != sec0) break;

			if (vcurrent == vjoin) {
				// next in loop
				if (vopposite == vend) {
					// the loop is complete
					MARK_CONTOUR_LAST(c1);
					vend = -1;
					c0i++;
				}
				if (c1i != c1i0) {
					lvl_swap_contours(lvl, c1i, c1i0);
				}
				break;
			} else {
				c1i++;
			}
		}

		c0i++;
	}
}

int lvl_sector_inside(struct lvl* lvl, int32_t sectori, struct vec2* p)
{
	struct lvl_sector* sector = lvl_get_sector(lvl, sectori);

	struct vec2 dir;
	dir.s[0] = 0;
	dir.s[1] = 1;

	int intersections = 0;

	for (int i = 0; i < sector->contourn; i++) {
		int ci = sector->contour0 + i;

		struct lvl_contour* c = lvl_get_contour(lvl, ci);
		struct lvl_linedef* l = lvl_get_linedef(lvl, c->linedef);

		struct vec2* v0 = lvl_get_vertex(lvl, l->vertex[0]);
		struct vec2* v1 = lvl_get_vertex(lvl, l->vertex[1]);
		struct vec2 vd;
		vec2_sub(&vd, v1, v0);

		float rxs = vec2_cross(&dir, &vd);
		if (rxs == 0) continue;
		float rxs1 = 1.0f / rxs;

		struct vec2 qp;
		vec2_sub(&qp, v0, p);

		float t = vec2_cross(&qp, &vd) * rxs1;

		if (t < 0) continue;

		float u = vec2_cross(&qp, &dir) * rxs1;

		if (u < 0.0f || u > 1.0f) continue;

		intersections++;
	}

	return intersections & 1;
}

int32_t lvl_sector_find(struct lvl* lvl, struct vec2* p)
{
	// XXX TODO proper search
	for (int i = 0; i < lvl->n_sectors; i++) {
		if (lvl_sector_inside(lvl, i, p)) {
			return i;
		}
	}
	return -1;
}

void lvl_entity_update_sector(struct lvl* lvl, struct lvl_entity* entity)
{
	// XXX TODO use previous sector value as a good guess, and check neighbours
	for (int i = 0; i < lvl->n_sectors; i++) {
		if (lvl_sector_inside(lvl, i, &entity->position)) {
			entity->sector = i;
			return;
		}
	}
	entity->sector = -1;
}

float lvl_entity_radius(struct lvl_entity* entity)
{
	return 32; // XXX? TODO get from entities.inc.h
}

static void lvl_entity_vec3_position(struct lvl_entity* entity, struct vec3* position)
{
	for (int i = 0; i < 2; i++) {
		position->s[i] = entity->position.s[i];
	}
	position->s[2] = entity->z;
}

void lvl_entity_view_plane(struct lvl_entity* entity, struct vec3* pos, struct vec3* dir, struct vec3* right, struct vec3* up)
{
	lvl_entity_vec3_position(entity, pos);
	float phi = entity->yaw;
	float c = cosf(DEG2RAD(phi));
	float s = sinf(DEG2RAD(phi));

	dir->s[0] = s;
	dir->s[1] = -c;
	dir->s[2] = 0;

	up->s[0] = 0;
	up->s[1] = 0;
	up->s[2] = 1;

	vec3_cross(right, up, dir);
}

void lvl_entity_mouse(struct lvl_entity* entity, struct vec3* pos, struct vec3* mdir, float fovy, int mouse_x, int mouse_y, int width, int height)
{
	struct vec3 dir;
	struct vec3 right;
	struct vec3 up;
	lvl_entity_view_plane(entity, pos, &dir, &right, &up);

	float ys = tanf(DEG2RAD(fovy/2));
	float s = 1.0f / (float)(height>>1) * ys;
	float y = (float)(mouse_y - (height>>1)) * s;
	float x = (float)(mouse_x - (width>>1)) * s;
	vec3_copy(mdir, &dir);
	vec3_add_scalei(mdir, &right, x);
	vec3_add_scalei(mdir, &up, -y);
	vec3_normalize(mdir);
}

struct clip_result {
	// TODO what did we intersect with?
};

static void entclip_sector_contour(struct clip_result* result, struct lvl* lvl, struct lvl_entity* entity, int32_t sectori, int32_t ci)
{
	struct lvl_contour* c = lvl_get_contour(lvl, ci);
	struct lvl_linedef* l = lvl_get_linedef(lvl, c->linedef);
	struct vec2* v0 = lvl_get_vertex(lvl, l->vertex[0]);
	struct vec2* v1 = lvl_get_vertex(lvl, l->vertex[1]);
	struct vec2 vd;
	vec2_sub(&vd, v1, v0);
	struct vec2 vn;
	vec2_normal(&vn, &vd);
	vec2_normalize(&vn);

	struct vec2 dw;
	vec2_sub(&dw, &entity->position, v0);

	float d = vec2_dot(&dw, &vn);
	float dabs = fabs(d);

	float radius = lvl_entity_radius(entity);

	struct vec2 escape;
	vec2_zero(&escape);

	int intersects = 0;

	if (dabs <= radius) {
		float epsilon = 1e-3f;
		float vlength = vec2_length(&vd);
		vec2_normalize(&vd);
		float u = vec2_dot(&vd, &dw);
		if (u >= 0 && u <= vlength) {
			float push = radius - dabs + epsilon;
			float sgn = d > 0 ? 1.0f : -1.0f;
			vec2_copy(&escape, &vn);
			vec2_scalei(&escape, push * sgn);
			intersects = 1;
		} else {
			struct vec2* vcorner = u<0 ? v0 : v1;
			vec2_sub(&escape, &entity->position, vcorner);
			float ed = vec2_length(&escape);
			if (ed <= radius) {
				float push = radius - ed + epsilon;
				vec2_normalize(&escape);
				vec2_scalei(&escape, push);
				intersects = 2;
			}
		}
	}

	if (intersects) {
		int impassable = 0;

		// TODO sector height magick

		if (l->sidedef[0] == -1 || l->sidedef[1] == -1) {
			impassable = 1;
		} else {
			for (int side = 0; side < 2; side++) {
				if (l->sidedef[side] == -1) continue;
				struct lvl_sidedef* sidedef = lvl_get_sidedef(lvl, l->sidedef[side]);
				struct lvl_sector* sector = lvl_get_sector(lvl, sidedef->sector);

				// XXX FIXME calculate sloped planes
				// intersection compensated for entity height..
				// and radius? XXX currently assuming non-sloped planes

				float headroom = fabs(sector->flat[0].plane.d + sector->flat[1].plane.d);
				if (headroom < MAGIC_EVEN_MORE_MAGIC_ENTITY_HEIGHT) impassable = 1;
			}
		}

		if (impassable) {
			vec2_addi(&entity->position, &escape);
		}
	}
}

static void entclip_sector(struct clip_result* result, struct lvl* lvl, struct lvl_entity* entity, int32_t sectori)
{
	struct lvl_sector* sector = lvl_get_sector(lvl, sectori);

	for (int i = 0; i < sector->contourn; i++) {
		int32_t ci = sector->contour0 + i;
		entclip_sector_contour(result, lvl, entity, sectori, ci);
	}
}


static void entclip(struct clip_result* result, struct lvl* lvl, struct lvl_entity* entity)
{
	// TODO optimize to only consider neighbouring sectors
	for (int i = 0; i < lvl->n_sectors; i++) {
		entclip_sector(result, lvl, entity, i);
	}
}

void lvl_entity_accelerate(struct lvl* lvl, struct lvl_entity* entity, struct vec2* acceleration, float dt)
{
	struct vec2 dvel;
	vec2_copy(&dvel, acceleration);
	vec2_scalei(&dvel, dt * MAGIC_ACCELERATION_MAGNITUDE);
	vec2_addi(&entity->velocity, &dvel);
}

void lvl_entity_clipmove(struct lvl* lvl, struct lvl_entity* entity, float dt)
{
	// apply friction
	vec2_scalei(&entity->velocity, powf(MAGIC_FRICTION_MAGNITUDE, dt));
	if (vec2_dot(&entity->velocity, &entity->velocity) < MAGIC_STOP_THRESHOLD) {
		vec2_zero(&entity->velocity);
	}

	struct vec2 move;
	vec2_copy(&move, &entity->velocity);
	vec2_scalei(&move, dt);

	float move_length = vec2_length(&move);

	int nsteps = (int)ceilf(move_length/(lvl_entity_radius(entity)/64));
	if (nsteps < 1) nsteps = 1;
	float fragment = 1.0f / (float)nsteps;

	for (int i = 0; i < nsteps; i++) {
		struct vec2 move_fragment;
		vec2_scale(&move_fragment, &move, fragment);
		vec2_addi(&entity->position, &move_fragment);

		struct clip_result clip_result;
		entclip(&clip_result, lvl, entity);
	}


	lvl_entity_update_sector(lvl, entity);

	// XXX updating z here, but that's not how all entities work (or eventually any)
	if (entity->sector != -1) {
		struct lvl_sector* sector = lvl_get_sector(lvl, entity->sector);
		struct plane* plane = &sector->flat[0].plane;
		float height = MAGIC_EVEN_MORE_MAGIC_ENTITY_HEIGHT;
		entity->z = plane_z(plane, &entity->position) + height;
	}

}

int lvl_trace(
	struct lvl* lvl,
	int32_t sector,
	struct vec3* originp,
	struct vec3* ray,
	struct lvl_trace_result* result)
{
	struct vec3 origin;
	vec3_copy(&origin, originp);

	struct vec2 origin2;

	struct vec2 ray2;
	vec2_from_vec3(&ray2, ray);

	result->sector = sector;

	//int iterations = 0;

	while (1) {
		//iterations++;
		//ASSERT(iterations < 100); // XXX

		result->linedef = -1;
		result->sidedef = -1;
		result->z = 0;

		float nt = 0;

		struct lvl_sector* sector = lvl_get_sector(lvl, result->sector);
		struct lvl_contour* nc = NULL;

		int n = 0;

		for (int i = 0; i < sector->contourn; i++) {
			int32_t ci = sector->contour0 + i;
			struct lvl_contour* c = lvl_get_contour(lvl, ci);
			struct lvl_linedef* ld = lvl_get_linedef(lvl, c->linedef);
			struct vec2* v0 = lvl_get_vertex(lvl, ld->vertex[0]);
			struct vec2* v1 = lvl_get_vertex(lvl, ld->vertex[1]);
			struct vec2 vd;
			vec2_sub(&vd, v1, v0);
			if ((vec2_cross(&ray2, &vd) * ((c->usr&1) ? 1 : -1)) < 0) continue;

			float rxs = vec2_cross(&ray2, &vd);
			if (rxs == 0) continue;
			struct vec2 qp;
			vec2_from_vec3(&origin2, &origin);
			vec2_sub(&qp, v0, &origin2);
			float t = vec2_cross(&qp, &vd) / rxs;
			if (t < 0) continue;
			float u = vec2_cross(&qp, &ray2) / rxs;
			if (u >= 0 && u <= 1) {
				if (result->linedef == -1 || t < nt) {
					nt = t;
					nc = c;
					result->linedef = c->linedef;
					result->sidedef = ld->sidedef[c->usr&1];
					vec3_scale(&result->position, ray, t);
					vec3_addi(&result->position, &origin);
					n++;
				}
			}
		}

		struct vec3 plane_position;
		float pt = 0.0f;
		int z = 0;
		if (ray->s[2] > 0) {
			pt = ray_plane_intersection(&plane_position, &origin, ray, &sector->flat[1].plane);
			z = 1;
		} else if (ray->s[2] < 0) {
			pt = ray_plane_intersection(&plane_position, &origin, ray, &sector->flat[0].plane);
			z = -1;
		}
		if (z != 0 && pt > 0 && (pt < nt || n == 0)) {
			result->linedef = -1;
			result->sidedef = -1;
			nc = NULL;
			result->z = z;
			nt = pt;
			vec3_copy(&result->position, &plane_position);
			n++;
		}

		if (n == 0) return 0;

		if (result->linedef == -1) {
			// sector intersection
			return 1;
		}

		ASSERT(result->linedef >= 0);
		AN(nc);

		struct lvl_linedef* ld = lvl_get_linedef(lvl, nc->linedef);

		if (ld->sidedef[(nc->usr&1)^1] == -1) {
			return 1;
		}

		uint32_t oppositei = lvl_get_sidedef(lvl, ld->sidedef[(nc->usr&1)^1])->sector;
		struct lvl_sector* opposite = lvl_get_sector(lvl, oppositei);
		struct vec2 hit;
		vec2_from_vec3(&hit, &result->position);

		float pz = result->position.s[2];
		float z0 = plane_z(&opposite->flat[0].plane, &hit);
		float z1 = plane_z(&opposite->flat[1].plane, &hit);

		if (pz < z0) {
			result->z = -1;
			return 1;
		} else if (pz > z1) {
			result->z = 1;
			return 1;
		}

		// repeat
		vec3_copy(&origin, &result->position);
		result->sector = oppositei;
	}

	return 0;
}

static void lvl_tag_apply_mask(struct lvl* lvl, int32_t mask)
{
	for (int i = 0; i < lvl->n_sectors; i++) {
		struct lvl_sector* sector = lvl_get_sector(lvl, i);
		sector->usr &= mask;
		for (int j = 0; j < sector->contourn; j++) {
			int32_t ci = sector->contour0 + j;
			struct lvl_contour* c = lvl_get_contour(lvl, ci);
			struct lvl_linedef* ld = lvl_get_linedef(lvl, c->linedef);
			uint32_t sdi = ld->sidedef[c->usr&1];
			ASSERT(sdi != -1);
			struct lvl_sidedef* sd = lvl_get_sidedef(lvl, sdi);
			sd->usr &= mask;
		}
	}
}

void lvl_tag_clear_highlights(struct lvl* lvl)
{
	lvl_tag_apply_mask(lvl, ~(LVL_HIGHLIGHTED_ZPLUS | LVL_HIGHLIGHTED_ZMINUS));
}

void lvl_tag_clear_all(struct lvl* lvl)
{
	lvl_tag_apply_mask(lvl, ~(LVL_HIGHLIGHTED_ZPLUS | LVL_HIGHLIGHTED_ZMINUS | LVL_SELECTED_ZMINUS | LVL_SELECTED_ZPLUS));
}

void lvl_tag_flats(struct lvl* lvl, struct lvl_trace_result* trace, int clicked)
{
	for (int i = 0; i < lvl->n_sectors; i++) {
		struct lvl_sector* sector = lvl_get_sector(lvl, i);

		sector->usr &= ~(LVL_HIGHLIGHTED_ZMINUS | LVL_HIGHLIGHTED_ZPLUS);
		if (i == trace->sector && trace->linedef == -1) {
			if (trace->z > 0) {
				sector->usr |= LVL_HIGHLIGHTED_ZPLUS;
				if (clicked) {
					sector->usr ^= LVL_SELECTED_ZPLUS;
				}
			}

			if (trace->z < 0) {
				sector->usr |= LVL_HIGHLIGHTED_ZMINUS;
				if (clicked) {
					sector->usr ^= LVL_SELECTED_ZMINUS;
				}
			}
		}
	}
}

void lvl_tag_sectors(struct lvl* lvl, struct lvl_trace_result* trace, int clicked)
{
	int selected = LVL_SELECTED_ZPLUS | LVL_SELECTED_ZMINUS;
	int highlighted = LVL_HIGHLIGHTED_ZPLUS | LVL_HIGHLIGHTED_ZMINUS;

	for (int i = 0; i < lvl->n_sectors; i++) {
		struct lvl_sector* sector = lvl_get_sector(lvl, i);

		if (i == trace->sector) {
			sector->usr |= highlighted;
			if (clicked) {
				sector->usr ^= selected;
			}
		} else {
			sector->usr &= ~highlighted;
		}

		for (int j = 0; j < sector->contourn; j++) {
			int32_t ci = sector->contour0 + j;
			struct lvl_contour* c = lvl_get_contour(lvl, ci);
			struct lvl_linedef* ld = lvl_get_linedef(lvl, c->linedef);
			uint32_t sdi = ld->sidedef[c->usr&1];
			ASSERT(sdi != -1);

			struct lvl_sidedef* sd = lvl_get_sidedef(lvl, sdi);

			if (i == trace->sector) {
				sd->usr |= highlighted;
				if (clicked) {
					sd->usr ^= selected;
				}
			} else {
				sd->usr &= ~highlighted;
			}
		}
	}
}

void lvl_tag_sidedefs(struct lvl* lvl, struct lvl_trace_result* trace, int clicked)
{
	for (int i = 0; i < lvl->n_sectors; i++) {
		struct lvl_sector* sector = lvl_get_sector(lvl, i);

		for (int j = 0; j < sector->contourn; j++) {
			int32_t ci = sector->contour0 + j;
			struct lvl_contour* c = lvl_get_contour(lvl, ci);
			struct lvl_linedef* ld = lvl_get_linedef(lvl, c->linedef);
			uint32_t sdi = ld->sidedef[c->usr&1];
			ASSERT(sdi != -1);

			struct lvl_sidedef* sd = lvl_get_sidedef(lvl, sdi);

			sd->usr &= ~(LVL_HIGHLIGHTED_ZMINUS | LVL_HIGHLIGHTED_ZPLUS);

			if (trace->sidedef == sdi) {
				if (trace->z >= 0) {
					sd->usr |= LVL_HIGHLIGHTED_ZPLUS;
					if (clicked) {
						sd->usr ^= LVL_SELECTED_ZPLUS;
					}
				}

				if (trace->z <= 0) {
					sd->usr |= LVL_HIGHLIGHTED_ZMINUS;
					if (clicked) {
						sd->usr ^= LVL_SELECTED_ZMINUS;
					}
				}
			}
		}
	}
}
