#ifndef M_H
#define M_H

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



struct vec3 {
	float s[3];
};

void vec3_scale(struct vec3* dst, struct vec3* src, float scalar);
void vec3_scalei(struct vec3* dst, float scalar);
void vec3_addi(struct vec3* dst, struct vec3* src);
void vec3_sub(struct vec3* dst, struct vec3* a, struct vec3* b);
float vec3_dot(struct vec3* a, struct vec3* b);
void vec3_cross(struct vec3* dst, struct vec3* a, struct vec3* b);
void vec3_copy(struct vec3* dst, struct vec3* src);
void vec3_add_scalei(struct vec3* dst, struct vec3* src, float scalar);
void vec3_normalize(struct vec3* x);
void vec3_set(struct vec3* v, float r, float g, float b);
void vec3_set8(struct vec3* v, int r, int g, int b);
void vec3_lerp(struct vec3* dst, struct vec3* a, struct vec3* b, float t);

/* transform RGB vector into the unicorns color space, suitable for measuring
 * euclidean distances */
void vec3_rgb2unicorns(struct vec3* v);


void vec3_to_vec2(struct vec3* v3, struct vec2* v2);

void vec2_rgbize(struct vec3* rgb, struct vec2* v);

struct mat33 {
	float s[9];
};

void mat33_apply(struct mat33* m, struct vec3* dst, struct vec3* src);
void mat33_applyi(struct mat33* m, struct vec3* v);

/*
struct mat23 {
	float s[6];
};
*/

struct plane {
	float s[4];
};

void plane_normal(struct vec3* normal, struct plane* plane);

void plane_floor(struct plane* p, float z);
void plane_ceiling(struct plane* p, float z);
float plane_z(struct plane* p, struct vec2* v);



float ray_plane_intersection(struct vec3* position, struct vec3* origin, struct vec3* ray, struct plane* plane);

#ifndef M_PI
#define M_PI (3.141592653589793)
#endif

#define DEG2RAD(x) (x/180.0f*M_PI)

#endif//M_H

