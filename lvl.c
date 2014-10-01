#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <tesselator.h>

#include <GL/glew.h>

#include "a.h"
#include "lvl.h"

static void* my_malloc(void* usr, unsigned int sz)
{
	//printf("my_malloc(%u)\n", sz);
	return malloc(sz);
}

static void* my_realloc(void* usr, void* ptr, unsigned int sz)
{
	//printf("my_realloc(%u)\n", sz);
	return realloc(ptr, sz);
}

static void my_free(void* usr, void* ptr)
{
	//printf("my_free()\n");
	return free(ptr);
}

void lvl_init(struct lvl* lvl)
{
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
	linedef->vertex0 = linedef->vertex1 = -1;
	linedef->sidedef_left = linedef->sidedef_right = -1;
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

static struct lvl_contour* lvl_get_contour(struct lvl* lvl, int32_t i)
{
	ASSERT(i >= 0 && i < lvl->n_contours);
	return &lvl->contours[i];
}


static void add_contour(struct lvl* lvl, uint32_t linedef, uint32_t data)
{
	ASSERT((lvl->n_contours + 1) < lvl->reserved_contours);
	int32_t i = lvl->n_contours++;
	struct lvl_contour* contour = lvl_get_contour(lvl, i);
	contour->linedef = linedef;
	contour->data = data;
}

#define LVL_CONTOUR_LEFT (1)
#define LVL_CONTOUR_RIGHT (2)
#define LVL_CONTOUR_NON (3)

#define LVL_CONTOUR_IS_LEFT(c) ((c->data & 3) == 1)
#define LVL_CONTOUR_IS_RIGHT(c) ((c->data & 3) == 2)
#define LVL_CONTOUR_IS_NON(c) ((c->data & 3) == 3)

static struct lvl* GLOBAL_lvl;
static uint32_t vcntr_resolve_sector(const void* vcntr)
{
	struct lvl_contour* cntr = (struct lvl_contour*) vcntr;
	struct lvl_linedef* ld = lvl_get_linedef(GLOBAL_lvl, cntr->linedef);
	uint32_t sdi = (LVL_CONTOUR_IS_LEFT(cntr) || LVL_CONTOUR_IS_NON(cntr)) ? ld->sidedef_left : LVL_CONTOUR_IS_RIGHT(cntr) ? ld->sidedef_right : -1;
	ASSERT(sdi != -1);
	struct lvl_sidedef* sd = lvl_get_sidedef(GLOBAL_lvl, sdi);
	ASSERT(sd->sector != -1);
	return sd->sector;
}

static int vcntr_is_non(const void* vcntr)
{
	struct lvl_contour* cntr = (struct lvl_contour*) vcntr;
	return LVL_CONTOUR_IS_NON(cntr) ? 1 : 0;
}

static int vcntr_cmp(const void* vcntr0, const void* vcntr1)
{
	int cmp = vcntr_resolve_sector(vcntr0) - vcntr_resolve_sector(vcntr1);
	if (cmp != 0) return cmp;
	return vcntr_is_non(vcntr0) - vcntr_is_non(vcntr1);
}

static void lvl_swap_contours(struct lvl* lvl, int32_t i0, int32_t i1)
{
	struct lvl_contour tmp;
	memcpy(&tmp, lvl_get_contour(lvl, i0), sizeof(struct lvl_contour));
	memcpy(lvl_get_contour(lvl, i0), lvl_get_contour(lvl, i1), sizeof(struct lvl_contour));
	memcpy(lvl_get_contour(lvl, i1), &tmp, sizeof(struct lvl_contour));
}

#define CONTOURS_CW (1)
#define SELECT_VERTEX(linedef, p) ((p) ? (linedef)->vertex1 : (linedef)->vertex0)
#define MARK_CONTOUR_FIRST(c) (c->data |= 4)
#define CONTOUR_IS_FIRST(c) (c->data & 4)
#define MARK_CONTOUR_LAST(c) (c->data |= 8)
#define CONTOUR_IS_LAST(c) (c->data & 8)

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
		ASSERT(linedef->sidedef_left != -1 || linedef->sidedef_right != -1);
		int32_t sleft = (linedef->sidedef_left == -1) ? -1 : lvl_get_sidedef(lvl, linedef->sidedef_left)->sector;
		int32_t sright = (linedef->sidedef_right == -1) ? -1 : lvl_get_sidedef(lvl, linedef->sidedef_right)->sector;
		ASSERT(sleft != -1 || sright != -1);
		if (sleft == sright) {
			add_contour(lvl, i, LVL_CONTOUR_NON);
		} else {
			if (sleft != -1) add_contour(lvl, i, LVL_CONTOUR_LEFT);
			if (sright != -1) add_contour(lvl, i, LVL_CONTOUR_RIGHT);
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
		if (LVL_CONTOUR_IS_NON(c)) continue;
		struct lvl_linedef* l = lvl_get_linedef(lvl, c->linedef);
		int sec = -1;
		if (LVL_CONTOUR_IS_LEFT(c)) {
			sec = lvl_get_sidedef(lvl, l->sidedef_left)->sector;
		} else if(LVL_CONTOUR_IS_RIGHT(c)) {
			sec = lvl_get_sidedef(lvl, l->sidedef_right)->sector;
		}
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
		if (LVL_CONTOUR_IS_NON(c0)) {
			c0i++;
			continue;
		}

		struct lvl_linedef* l0 = lvl_get_linedef(lvl, c0->linedef);
		int32_t sec0 = -1;
		int32_t vcurrent = -1;
		if (vend == -1) {
			MARK_CONTOUR_FIRST(c0);
		}
		if (LVL_CONTOUR_IS_LEFT(c0)) {
			sec0 = lvl_get_sidedef(lvl, l0->sidedef_left)->sector;
			vcurrent = SELECT_VERTEX(l0, CONTOURS_CW);
			if (vend == -1) vend = SELECT_VERTEX(l0, !CONTOURS_CW);
		} else if (LVL_CONTOUR_IS_RIGHT(c0)) {
			sec0 = lvl_get_sidedef(lvl, l0->sidedef_right)->sector;
			vcurrent = SELECT_VERTEX(l0, !CONTOURS_CW);
			if (vend == -1) vend = SELECT_VERTEX(l0, CONTOURS_CW);
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
			if (LVL_CONTOUR_IS_NON(c1)) break;

			struct lvl_linedef* l1 = lvl_get_linedef(lvl, c1->linedef);
			int32_t sec1 = -1;
			int32_t vjoin = -1;
			int32_t vopposite = -1;
			if (LVL_CONTOUR_IS_LEFT(c1)) {
				sec1 = lvl_get_sidedef(lvl, l1->sidedef_left)->sector;
				vjoin = SELECT_VERTEX(l1, !CONTOURS_CW);
				vopposite = SELECT_VERTEX(l1, CONTOURS_CW);
			} else if (LVL_CONTOUR_IS_RIGHT(c1)) {
				sec1 = lvl_get_sidedef(lvl, l1->sidedef_right)->sector;
				vjoin = SELECT_VERTEX(l1, CONTOURS_CW);
				vopposite = SELECT_VERTEX(l1, !CONTOURS_CW);
			}
			ASSERT(sec1 != -1);
			ASSERT(vjoin != -1);
			ASSERT(vopposite != -1);

			if (sec1 != sec0) break;

			if (vcurrent == vjoin) {
				// next in loop
				// XXX printf("%d <=> %d (vend=%d)\n", c1i, c1i0, vend);
				if (vopposite == vend) {
					// the loop is complete
					// XXX printf("LOOPSIE\n");
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

/*
static void lvl_sector_draw_lines_partial(struct lvl* lvl, int32_t sectori, int top)
{
	struct lvl_sector* sector = lvl_get_sector(lvl, sectori);
	int32_t ci = sector->contour0;

	float y = top ? sector->ceiling.plane.s[3] : sector->floor.plane.s[3];

	int begun = 0;
	while (1) {
		if (ci >= lvl->n_contours) break;
		struct lvl_contour* c = lvl_get_contour(lvl, ci);

		struct lvl_linedef* l = lvl_get_linedef(lvl, c->linedef);
		struct vec2* v = NULL;
		int32_t s1 = -1;
		if (LVL_CONTOUR_IS_LEFT(c)) {
			v = lvl_get_vertex(lvl, l->vertex0);
			s1 = lvl_get_sidedef(lvl, l->sidedef_left)->sector;
		} else if (LVL_CONTOUR_IS_RIGHT(c)) {
			v = lvl_get_vertex(lvl, l->vertex1);
			s1 = lvl_get_sidedef(lvl, l->sidedef_right)->sector;
		}
		AN(v);
		ASSERT(s1 != -1);
		if (s1 != sectori) break;

		if (CONTOUR_IS_FIRST(c)) {
			//printf("BEGIN\n");
			glBegin(GL_LINE_LOOP);
			AZ(begun);
			begun = 1;
		}

		glVertex3f(v->s[0], y, v->s[1]);
		//printf("   VERTEX\n");

		if (CONTOUR_IS_LAST(c)) {
			//printf("END\n");
			glEnd();
			AN(begun);
			begun = 0;
		}


		ci++;
	}
}
*/

static void lvl_sector_draw_lines_partial(struct lvl* lvl, int32_t sectori, int top)
{
	struct lvl_sector* sector = lvl_get_sector(lvl, sectori);
	struct vec2* vlast = NULL;
	int begun = 0;

	for (int i = 0; i < sector->contourn; i++) {
		int ci = sector->contour0 + i;

		struct lvl_contour* c = lvl_get_contour(lvl, ci);

		struct lvl_linedef* l = lvl_get_linedef(lvl, c->linedef);
		struct vec2* v = NULL;
		if (LVL_CONTOUR_IS_LEFT(c)) {
			v = lvl_get_vertex(lvl, l->vertex0);
		} else if (LVL_CONTOUR_IS_RIGHT(c)) {
			v = lvl_get_vertex(lvl, l->vertex1);
		}
		AN(v);

		if (vlast != NULL) {
			struct plane* plane = top ? &sector->ceiling.plane : &sector->floor.plane;
			float zlast = plane_z(plane, vlast);
			float z = plane_z(plane, v);
			glBegin(GL_LINES);
			glColor4f(0,0,0,1);
			glVertex3f(vlast->s[0], zlast, vlast->s[1]);
			glColor4f(1,1,0,1);
			glVertex3f(v->s[0], z, v->s[1]);
			glEnd();
		}

		vlast = v;

		if (CONTOUR_IS_FIRST(c)) {
			AZ(begun);
			begun = 1;
		}

		if (CONTOUR_IS_LAST(c)) {
			AN(begun);
			begun = 0;
			vlast = NULL;
		}
	}
}

void lvl_sector_draw_lines(struct lvl* lvl, int32_t sectori)
{
	//printf("==%d==\n", sectori);
	lvl_sector_draw_lines_partial(lvl, sectori, 0);
	lvl_sector_draw_lines_partial(lvl, sectori, 1);
	//printf("=====\n");
}

static void lvl_sector_draw_flat_partial(struct lvl* lvl, int32_t sectori, int top)
{
	struct lvl_sector* sector = lvl_get_sector(lvl, sectori);

	TESSalloc ta;
	TESStesselator* t;

	ta.memalloc = my_malloc;
	ta.memrealloc = my_realloc;
	ta.memfree = my_free;
	ta.userData = NULL;
	ta.meshEdgeBucketSize = 64;
	ta.meshVertexBucketSize = 64;
	ta.meshFaceBucketSize = 32;
	ta.dictNodeBucketSize = 64;
	ta.regionBucketSize = 32;
	ta.extraVertices = 0;

	t = tessNewTess(&ta);

	struct vec2 points[16384];

	int32_t p = 0;
	int32_t p0 = -1;

	int begun = 0;
	for (int i = 0; i < sector->contourn; i++) {
		int ci = sector->contour0 + i;

		if (p0 == -1) p0 = p;

		struct lvl_contour* c = lvl_get_contour(lvl, ci);

		struct lvl_linedef* l = lvl_get_linedef(lvl, c->linedef);
		struct vec2* v = NULL;
		if (LVL_CONTOUR_IS_LEFT(c)) {
			v = lvl_get_vertex(lvl, SELECT_VERTEX(l, !CONTOURS_CW));
		} else if (LVL_CONTOUR_IS_RIGHT(c)) {
			v = lvl_get_vertex(lvl, SELECT_VERTEX(l, CONTOURS_CW));
		}
		AN(v);

		if (CONTOUR_IS_FIRST(c)) {
			p0 = p;
			AZ(begun);
			begun = 1;
		}

		memcpy(&points[p], v, sizeof(struct vec2));

		if (CONTOUR_IS_LAST(c)) {
			ASSERT(p > p0);
			tessAddContour(t, 2, &points[p0], sizeof(struct vec2), p - p0 + 1);
			p0 = -1;
			AN(begun);
			begun = 0;
		}

		p++;
	}

	int nvp = 3;
	int tess_result = tessTesselate(t, TESS_WINDING_POSITIVE, TESS_POLYGONS, nvp, 2, NULL);
	//int tess_result = tessTesselate(t, TESS_WINDING_ODD, TESS_POLYGONS, nvp, 2, NULL);
	//int tess_result = tessTesselate(t, TESS_WINDING_NONZERO, TESS_POLYGONS, nvp, 2, NULL);
	ASSERT(tess_result == 1);

	const float* verts = tessGetVertices(t);
	int nelems = tessGetElementCount(t);
	const int* elems = tessGetElements(t);

	for (int i = 0; i < nelems; i++) {
		glBegin(GL_TRIANGLES);
		//glBegin(GL_LINE_STRIP);
		const int* p = &elems[i * nvp];
		for (int j = 0; j < nvp; j++) {
			ASSERT(p[j] != TESS_UNDEF);
			int k = top ? nvp - 1 - j : j;
			struct vec2 v;
			struct plane* plane = top ? &sector->ceiling.plane : &sector->floor.plane;
			v.s[0] = verts[p[k]*2];
			v.s[1] = verts[p[k]*2+1];
			float z = plane_z(plane, &v);
			glVertex3f(v.s[0], z, v.s[1]);
		}
		glEnd();
		//glEnd();
	}

	tessDeleteTess(t);
}

void lvl_sector_draw_flats(struct lvl* lvl, int32_t sectori)
{
	struct lvl_sector* sector = lvl_get_sector(lvl, sectori);

	float r,g,b;

	r = 0; g = 0.1; b = 0.2;
	if (sector->usr & LVL_HIGHLIGHTED_ZMINUS) {
		r += 0.1f;
		g += 0.1f;
		b += 0.1f;
	}
	if (sector->usr & LVL_SELECTED_ZMINUS) {
		r += 1.0f;
		g += 1.0f;
	}

	glColor4f(r,g,b,1);
	lvl_sector_draw_flat_partial(lvl, sectori, 0);

	r = 0.1; g = 0.2; b = 0;
	if (sector->usr & LVL_HIGHLIGHTED_ZPLUS) {
		r += 0.1f;
		g += 0.1f;
		b += 0.1f;
	}
	if (sector->usr & LVL_SELECTED_ZPLUS) {
		r += 1.0f;
		g += 1.0f;
	}
	glColor4f(r,g,b,1);
	lvl_sector_draw_flat_partial(lvl, sectori, 1);
}

static void lvl_sector_draw_walls(struct lvl* lvl, int32_t sectori)
{
	struct lvl_sector* sector = lvl_get_sector(lvl, sectori);

	for (int i = 0; i < sector->contourn; i++) {
		int ci = sector->contour0 + i;

		struct lvl_contour* c = lvl_get_contour(lvl, ci);
		struct lvl_linedef* l = lvl_get_linedef(lvl, c->linedef);

		struct lvl_sidedef* sd = NULL;
		struct lvl_sidedef* sopp = NULL;
		struct vec2* v0 = NULL;
		struct vec2* v1 = NULL;
		if (LVL_CONTOUR_IS_LEFT(c)) {
			v0 = lvl_get_vertex(lvl, l->vertex0);
			v1 = lvl_get_vertex(lvl, l->vertex1);
			sd = l->sidedef_left == -1 ? NULL : lvl_get_sidedef(lvl, l->sidedef_left);
			sopp = l->sidedef_right == -1 ? NULL : lvl_get_sidedef(lvl, l->sidedef_right);
		} else if (LVL_CONTOUR_IS_RIGHT(c)) {
			v0 = lvl_get_vertex(lvl, l->vertex1);
			v1 = lvl_get_vertex(lvl, l->vertex0);
			sd = l->sidedef_right == -1 ? NULL : lvl_get_sidedef(lvl, l->sidedef_right);
			sopp = l->sidedef_left == -1 ? NULL : lvl_get_sidedef(lvl, l->sidedef_left);
		} else {
			continue; // TODO NONs?
		}
		AN(sd);
		AN(v0);
		AN(v1);

		struct vec2 vd;
		vec2_sub(&vd, v1, v0);
		struct vec3 stdcolor;
		vec2_rgbize(&stdcolor, &vd);
		struct vec3 mid;
		vec3_set(&mid, 0.3, 0.3, 0.3);
		vec3_lerp(&stdcolor, &stdcolor, &mid, 0.8f);

		struct vec3 color;

		if (sopp == NULL) {
			// one-sided
			float z0 = plane_z(&sector->ceiling.plane, v0);
			float z1 = plane_z(&sector->ceiling.plane, v1);
			float z2 = plane_z(&sector->floor.plane, v1);
			float z3 = plane_z(&sector->floor.plane, v0);

			vec3_copy(&color, &stdcolor);
			if ((sd->usr & LVL_HIGHLIGHTED_ZMINUS) || (sd->usr & LVL_HIGHLIGHTED_ZPLUS)) {
				struct vec3 a;
				vec3_set(&a, 0.1, 0.1, 0.1);
				vec3_addi(&color, &a);
			}
			if ((sd->usr & LVL_SELECTED_ZMINUS) || (sd->usr & LVL_SELECTED_ZPLUS)) {
				struct vec3 a;
				vec3_set(&a, 0.7, 0.7, 0);
				vec3_addi(&color, &a);
			}
			glColor4f(color.s[0], color.s[1], color.s[2], 1);

			glBegin(GL_QUADS);
			glVertex3f(v0->s[0], z0, v0->s[1]);
			glVertex3f(v1->s[0], z1, v1->s[1]);
			glVertex3f(v1->s[0], z2, v1->s[1]);
			glVertex3f(v0->s[0], z3, v0->s[1]);
			glEnd();
		} else {
			// two-sided
			struct lvl_sector* sector1 = lvl_get_sector(lvl, sopp->sector);
			{
				float z0 = plane_z(&sector1->floor.plane, v0);
				float z1 = plane_z(&sector1->floor.plane, v1);
				float z2 = plane_z(&sector->floor.plane, v1);
				float z3 = plane_z(&sector->floor.plane, v0);

				if (z1 > z2 && z0 > z3) {
					vec3_copy(&color, &stdcolor);
					if (sd->usr & LVL_HIGHLIGHTED_ZMINUS) {
						struct vec3 a;
						vec3_set(&a, 0.1, 0.1, 0.1);
						vec3_addi(&color, &a);
					}
					if (sd->usr & LVL_SELECTED_ZMINUS) {
						struct vec3 a;
						vec3_set(&a, 0.7, 0.7, 0);
						vec3_addi(&color, &a);
					}
					glColor4f(color.s[0], color.s[1], color.s[2], 1);

					glBegin(GL_QUADS);
					glVertex3f(v0->s[0], z0, v0->s[1]);
					glVertex3f(v1->s[0], z1, v1->s[1]);
					glVertex3f(v1->s[0], z2, v1->s[1]);
					glVertex3f(v0->s[0], z3, v0->s[1]);
					glEnd();
				} // XXX else: crossing?
			}
			{
				float z0 = plane_z(&sector1->ceiling.plane, v0);
				float z1 = plane_z(&sector1->ceiling.plane, v1);
				float z2 = plane_z(&sector->ceiling.plane, v1);
				float z3 = plane_z(&sector->ceiling.plane, v0);

				if (z1 < z2 && z0 < z3) {
					vec3_copy(&color, &stdcolor);
					if (sd->usr & LVL_HIGHLIGHTED_ZPLUS) {
						struct vec3 a;
						vec3_set(&a, 0.1, 0.1, 0.1);
						vec3_addi(&color, &a);
					}
					if (sd->usr & LVL_SELECTED_ZPLUS) {
						struct vec3 a;
						vec3_set(&a, 0.7, 0.7, 0);
						vec3_addi(&color, &a);
					}
					glColor4f(color.s[0], color.s[1], color.s[2], 1);

					glBegin(GL_QUADS);
					glVertex3f(v0->s[0], z3, v0->s[1]);
					glVertex3f(v1->s[0], z2, v1->s[1]);
					glVertex3f(v1->s[0], z1, v1->s[1]);
					glVertex3f(v0->s[0], z0, v0->s[1]);
					glEnd();
				} // XXX else: crossing?
			}
		}
	}
}

void lvl_draw_sector(struct lvl* lvl, int32_t sectori)
{
	lvl_sector_draw_flats(lvl, sectori);
	lvl_sector_draw_walls(lvl, sectori);
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
		if (LVL_CONTOUR_IS_NON(c)) continue;

		struct lvl_linedef* l = lvl_get_linedef(lvl, c->linedef);

		struct vec2* v0 = lvl_get_vertex(lvl, l->vertex0);
		struct vec2* v1 = lvl_get_vertex(lvl, l->vertex1);
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
	// XXX use previous sector value as a good guess, and check neighbours
	for (int i = 0; i < lvl->n_sectors; i++) {
		if (lvl_sector_inside(lvl, i, &entity->p)) {
			entity->sector = i;
			return;
		}
	}
	entity->sector = -1;
}

float lvl_entity_radius(struct lvl_entity* entity)
{
	return 32; // XXX?
}

static void lvl_entity_vec3_position(struct lvl_entity* entity, struct vec3* position)
{
	for (int i = 0; i < 2; i++) {
		position->s[i] = entity->p.s[i];
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

static void entclip_sector_contour(struct lvl* lvl, struct lvl_entity* entity, int32_t sectori, int32_t ci)
{
	//struct lvl_sector* sector = lvl_get_sector(lvl, sectori);
	struct lvl_contour* c = lvl_get_contour(lvl, ci);

	if (LVL_CONTOUR_IS_NON(c)) return;

	struct lvl_linedef* l = lvl_get_linedef(lvl, c->linedef);
	struct vec2* v0 = lvl_get_vertex(lvl, l->vertex0);
	struct vec2* v1 = lvl_get_vertex(lvl, l->vertex1);
	struct vec2 vd;
	vec2_sub(&vd, v1, v0);
	struct vec2 vn;
	vec2_normal(&vn, &vd);
	vec2_normalize(&vn);

	struct vec2 dw;
	vec2_sub(&dw, &entity->p, v0);

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
		//float u = vec2_dot(&dw, &vd);
		float u = vec2_dot(&vd, &dw);
		if (u >= 0 && u <= vlength) {
			float push = radius - dabs + epsilon;
			float sgn = d > 0 ? 1.0f : -1.0f;
			vec2_copy(&escape, &vn);
			vec2_scalei(&escape, push * sgn);
			intersects = 1;
		} else {
			struct vec2* vcorner = u<0 ? v0 : v1;
			vec2_sub(&escape, &entity->p, vcorner);
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

		if (l->sidedef_left == -1 || l->sidedef_right == -1) {
			impassable = 1;
		}

		if (impassable) {
			vec2_addi(&entity->p, &escape);
		}
	}
}

static void entclip_sector(struct lvl* lvl, struct lvl_entity* entity, int32_t sectori)
{
	struct lvl_sector* sector = lvl_get_sector(lvl, sectori);

	for (int i = 0; i < sector->contourn; i++) {
		int32_t ci = sector->contour0 + i;
		entclip_sector_contour(lvl, entity, sectori, ci);
	}
}


static void entclip(struct lvl* lvl, struct lvl_entity* entity)
{
	//if (entity->sector == -1) return;

	for (int i = 0; i < lvl->n_sectors; i++) {
		entclip_sector(lvl, entity, i);
	}
}

void lvl_entity_clipmove(struct lvl* lvl, struct lvl_entity* entity, struct vec2* move)
{
	//printf("\n\n\nCLIPMOVE\n");
	float move_length = vec2_length(move);

	int nsteps = (int)ceilf(move_length/(lvl_entity_radius(entity)/64));
	if (nsteps < 1) nsteps = 1;
	float fragment = 1.0f / (float)nsteps;

	for (int i = 0; i < nsteps; i++) {
		struct vec2 move_fragment;
		vec2_scale(&move_fragment, move, fragment);
		vec2_addi(&entity->p, &move_fragment);
		entclip(lvl, entity);
	}

	lvl_entity_update_sector(lvl, entity);
	// XXX updating z here, but that's not how all entities work (or eventually any)
	if (entity->sector != -1) {
		struct lvl_sector* sector = lvl_get_sector(lvl, entity->sector);
		struct plane* plane = &sector->floor.plane;
		float height = 48;
		entity->z = plane_z(plane, &entity->p) + height;
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
	vec3_to_vec2(ray, &ray2);

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
			struct vec2* v0 = lvl_get_vertex(lvl, ld->vertex0);
			struct vec2* v1 = lvl_get_vertex(lvl, ld->vertex1);
			struct vec2 vd;
			vec2_sub(&vd, v1, v0);
			if ((vec2_cross(&ray2, &vd) * (LVL_CONTOUR_IS_LEFT(c) ? -1 : LVL_CONTOUR_IS_RIGHT(c) ? 1 : 0)) < 0) continue;
			float rxs = vec2_cross(&ray2, &vd);
			if (rxs == 0) continue;
			struct vec2 qp;
			vec3_to_vec2(&origin, &origin2);
			vec2_sub(&qp, v0, &origin2);
			float t = vec2_cross(&qp, &vd) / rxs;
			if (t < 0) continue;
			float u = vec2_cross(&qp, &ray2) / rxs;
			if (u >= 0 && u <= 1) {
				if (result->linedef == -1 || t < nt) {
					nt = t;
					nc = c;
					result->linedef = c->linedef;
					result->sidedef = LVL_CONTOUR_IS_LEFT(c) ? ld->sidedef_left : LVL_CONTOUR_IS_RIGHT(c) ? ld->sidedef_right : -1;
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
			pt = ray_plane_intersection(&plane_position, &origin, ray, &sector->ceiling.plane);
			z = 1;
		} else if (ray->s[2] < 0) {
			pt = ray_plane_intersection(&plane_position, &origin, ray, &sector->floor.plane);
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

		uint32_t oppositei;
		if (LVL_CONTOUR_IS_LEFT(nc)) {
			if (ld->sidedef_right == -1) {
				return 1;
			} else {
				struct lvl_sidedef* sd = lvl_get_sidedef(lvl, ld->sidedef_right);
				oppositei = sd->sector;
			}
		} else if (LVL_CONTOUR_IS_RIGHT(nc)) {
			if (ld->sidedef_left == -1) {
				return 1;
			} else {
				struct lvl_sidedef* sd = lvl_get_sidedef(lvl, ld->sidedef_left);
				oppositei = sd->sector;
			}
		} else {
			AZ(1);
		}

		struct lvl_sector* opposite = lvl_get_sector(lvl, oppositei);
		struct vec2 hit;
		vec3_to_vec2(&result->position, &hit);
		float pz = result->position.s[2];
		float z0 = plane_z(&opposite->floor.plane, &hit);
		float z1 = plane_z(&opposite->ceiling.plane, &hit);

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

void lvl_tag(struct lvl* lvl, struct lvl_trace_result* trace, int clicked)
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

		for (int j = 0; j < sector->contourn; j++) {
			int32_t ci = sector->contour0 + j;
			struct lvl_contour* c = lvl_get_contour(lvl, ci);
			struct lvl_linedef* ld = lvl_get_linedef(lvl, c->linedef);
			uint32_t sdi = LVL_CONTOUR_IS_LEFT(c) ? ld->sidedef_left : LVL_CONTOUR_IS_RIGHT(c) ? ld->sidedef_right : -1;
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

void lvl_draw(struct lvl* lvl)
{
	for (int i = 0; i < lvl->n_sectors; i++) {
		//if (i != 0) continue;
		lvl_draw_sector(lvl, i);
		lvl_sector_draw_lines(lvl, i);
	}
}
