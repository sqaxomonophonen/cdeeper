#ifndef M_H
#define M_H

#define EPSILON (1e-4)

struct vec2 {
	float s[2];
};

void vec2_addi(struct vec2* target, struct vec2* src);
void vec2_sub(struct vec2* dst, struct vec2* a, struct vec2* b);
float vec2_dot(struct vec2* a, struct vec2* b);
float vec2_length(struct vec2* v);
float vec2_cross(struct vec2* a, struct vec2* b);
void vec2_scale(struct vec2* dst, struct vec2* src, float scalar);
void vec2_scalei(struct vec2* dst, float scalar);
void vec2_normal(struct vec2* dst, struct vec2* src);
void vec2_normalize(struct vec2* v);
void vec2_zero(struct vec2* v);
void vec2_copy(struct vec2* dst, struct vec2* src);
//float vec2_distance(struct vec2* a, struct vec2* b);
void vec2_angle(struct vec2* v, float degrees);
void vec2_add_scalei(struct vec2* dst, struct vec2* src, float scalar);
void vec2_lerp(struct vec2* dst, struct vec2* a, struct vec2* b, float t);


struct vec3 {
	float s[3];
};

void vec3_zero(struct vec3* v);
void vec3_scale(struct vec3* dst, struct vec3* src, float scalar);
void vec3_scalei(struct vec3* dst, float scalar);
void vec3_addi(struct vec3* dst, struct vec3* src);
void vec3_sub(struct vec3* dst, struct vec3* a, struct vec3* b);
float vec3_dot(struct vec3* a, struct vec3* b);
void vec3_cross(struct vec3* dst, struct vec3* a, struct vec3* b);
void vec3_copy(struct vec3* dst, struct vec3* src);
void vec3_add_scalei(struct vec3* dst, struct vec3* src, float scalar);
void vec3_normalize(struct vec3* x);
void vec3_normalized(struct vec3* dst, struct vec3* src);
void vec3_set(struct vec3* v, float r, float g, float b);
void vec3_extract(struct vec3* v, float* r, float* g, float* b);
void vec3_set8(struct vec3* v, int r, int g, int b);
void vec3_lerp(struct vec3* dst, struct vec3* a, struct vec3* b, float t);

/* transform RGB vector into the unicorns color space, suitable for measuring
 * euclidean distances */
void vec3_rgb2unicorns(struct vec3* v);

void vec3_from_vec2(struct vec3* v3, struct vec2* v2);
void vec2_from_vec3(struct vec2* v2, struct vec3* v3);

void vec2_rgbize(struct vec3* rgb, struct vec2* v);

struct mat33 {
	float s[9];
};

void mat33_apply(struct mat33* m, struct vec3* dst, struct vec3* src);
void mat33_applyi(struct mat33* m, struct vec3* v);
void mat33_mul(struct mat33* dst, struct mat33* a, struct mat33* b);
void mat33_set_rotation(struct mat33* m, float angle, struct vec3* axis);

struct mat23 {
	float s[6];
};

void mat23_apply(struct mat23* m, struct vec2* dst, struct vec2* src);
void mat23_applyi(struct mat23* m, struct vec2* v);
void mat23_set_identity(struct mat23* m);
//void mat23_translate(struct mat23* m, struct vec2* t);

float ray_zplane_intersection(struct vec3* position, struct vec3* origin, struct vec3* ray, float z);

#ifndef M_PI
#define M_PI (3.141592653589793)
#endif

#define DEG2RAD(x) (x/180.0f*M_PI)


static inline int clampi(int i, int min, int max)
{
	if (i < min) return min;
	if (i > max) return max;
	return i;
}

#endif//M_H

