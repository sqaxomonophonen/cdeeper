#include <math.h>
#include "m.h"

void plane_floor(struct plane* p, float z)
{
	p->s[0] = 0;
	p->s[1] = 0;
	p->s[2] = 1;
	p->s[3] = -z;
}

void plane_ceiling(struct plane* p, float z)
{
	p->s[0] = 0;
	p->s[1] = 0;
	p->s[2] = -1;
	p->s[3] = z;
}

void vec2_addi(struct vec2* target, struct vec2* src)
{
	for (int i = 0; i < 2; i++) {
		target->s[i] += src->s[i];
	}
}

void vec2_sub(struct vec2* dst, struct vec2* a, struct vec2* b)
{
	for (int i = 0; i < 2; i++) {
		dst->s[i] = a->s[i] - b->s[i];
	}
}

float vec2_cross(struct vec2* a, struct vec2* b)
{
	return a->s[0]*b->s[1] - a->s[1]*b->s[0];
}

float plane_z(struct plane* p, struct vec2* v)
{
	// ax + by + cz + d = 0
	// so, z = -(ax + by + d) / c
	return -((p->s[0]*v->s[0] + p->s[1]*v->s[1] + p->s[3]) / p->s[2]);
}

float vec2_dot(struct vec2* a, struct vec2* b)
{
	return a->s[0]*b->s[0] + a->s[1]*b->s[1];
}

float vec2_length(struct vec2* v)
{
	return sqrtf(vec2_dot(v, v));
}

void vec2_scale(struct vec2* dst, struct vec2* src, float scalar)
{
	for (int i = 0; i < 2; i++) {
		dst->s[i] = src->s[i] * scalar;
	}
}

void vec2_scalei(struct vec2* dst, float scalar)
{
	for (int i = 0; i < 2; i++) {
		dst->s[i] *= scalar;
	}
}

void vec2_normal(struct vec2* dst, struct vec2* src)
{
	dst->s[0] = src->s[1];
	dst->s[1] = -src->s[0];
}

void vec2_normalize(struct vec2* v)
{
	vec2_scalei(v, 1.0f/vec2_length(v));
}

void vec2_zero(struct vec2* v)
{
	for (int i = 0; i < 2; i++) {
		v->s[i] = 0.0f;
	}
}

void vec2_copy(struct vec2* dst, struct vec2* src)
{
	for (int i = 0; i < 2; i++) {
		dst->s[i] = src->s[i];
	}
}

void vec3_to_vec2(struct vec3* v3, struct vec2* v2)
{
	for (int i = 0; i < 2; i++) {
		v2->s[i] = v3->s[i];
	}
}

void vec3_scale(struct vec3* dst, struct vec3* src, float scalar)
{
	for (int i = 0; i < 3; i++) {
		dst->s[i] = src->s[i] * scalar;
	}
}

void vec3_addi(struct vec3* dst, struct vec3* src)
{
	for (int i = 0; i < 3; i++) {
		dst->s[i] += src->s[i];
	}
}

float vec3_dot(struct vec3* a, struct vec3* b)
{
	return a->s[0]*b->s[0] + a->s[1]*b->s[1] + a->s[2]*b->s[2];
}

void vec3_cross(struct vec3* dst, struct vec3* a, struct vec3* b)
{
	for (int i = 0; i < 3; i++) {
		int i1 = (i+1)%3;
		int i2 = (i+2)%3;
		dst->s[i] = a->s[i1]*b->s[i2] - a->s[i2]*b->s[i1];
	}
}

void vec3_copy(struct vec3* dst, struct vec3* src)
{
	for (int i = 0; i < 3; i++) {
		dst->s[i] = src->s[i];
	}
}

void vec3_add_scalei(struct vec3* dst, struct vec3* src, float scalar)
{
	for (int i = 0; i < 3; i++) {
		dst->s[i] += src->s[i] * scalar;
	}
}

void vec3_normalize(struct vec3* x)
{
	float r = sqrtf(vec3_dot(x, x));
	for (int i = 0; i < 3; i++) {
		x->s[i] /= r;
	}
}

void vec3_set(struct vec3* v, float r, float g, float b)
{
	v->s[0] = r;
	v->s[1] = g;
	v->s[2] = b;
}

void vec3_lerp(struct vec3* dst, struct vec3* a, struct vec3* b, float t)
{
	for (int i = 0; i < 3; i++) {
		dst->s[i] = a->s[i] + (b->s[i] - a->s[i]) * t;
	}
}

void vec2_rgbize(struct vec3* rgb, struct vec2* v)
{
	float s = (atan2f(v->s[0], v->s[1]) / M_PI + 1.0f) * 1.5f;
	if (s < 1) {
		float up = s;
		float down = 1-s;
		rgb->s[0] = up;
		rgb->s[1] = 0;
		rgb->s[2] = down;
	} else if (s < 2) {
		float up = s-1;
		float down = 2-s;
		rgb->s[0] = down;
		rgb->s[1] = up;
		rgb->s[2] = 0;
	} else {
		float up = s-2;
		float down = 3-s;
		rgb->s[0] = 0;
		rgb->s[1] = down;
		rgb->s[2] = up;
	}
}

void plane_normal(struct vec3* normal, struct plane* plane)
{
	for (int i = 0; i < 3; i++) {
		normal->s[i] = plane->s[i];
	}
}

float ray_plane_intersection(struct vec3* position, struct vec3* origin, struct vec3* ray, struct plane* plane)
{
	struct vec3 normal;
	plane_normal(&normal, plane);
	float t = -(vec3_dot(&normal, origin) + plane->s[3]) / vec3_dot(&normal, ray);
	if (t > 0) {
		vec3_scale(position, ray, t);
		vec3_addi(position, origin);
		return t;
	} else {
		return 0.0f;
	}
}

