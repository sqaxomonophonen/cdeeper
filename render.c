#include "names.h"
#include "magic.h"
#include "render.h"
#include "mud.h"
#include "runtime.h"
#include "a.h"

#include <tesselator.h>
#include <GL/glew.h>


#define FLOATS_PER_FLAT_VERTEX (8)
#define FLOATS_PER_TYPE0_VERTEX (6)

// XXX TODO should roll my own matrix stack. gl_ModelViewProjectionMatrix,
// glLoadIdentity() and so on are all deprecated


#define DEF_LIGHT_FALLOFF_FN	\
	"float light_falloff_fn (float light_level, float z)\n" \
	"{\n" \
	"	float ib = 1.0 - light_level;\n" \
	"	ib = ib*ib*ib*ib*ib*ib;\n" \
	"	float v = (light_level + 1.0) / (length(z) * ib + light_level + 1);\n" \
	"	return 1-v;\n" \
	"}\n"

// flat shader source
static const char* flat_shader_vertex_src =
	"#version 130\n"
	"\n"
	"attribute vec3 a_pos;\n"
	"attribute vec2 a_uv;\n"
	"attribute vec2 a_selector;\n"
	"attribute float a_light_level;\n"
	"\n"
	"varying vec2 v_uv;\n"
	"varying vec2 v_selector;\n"
	"varying float v_light_level;\n"
	"varying float v_z;\n"
	"\n"
	"void main()\n"
	"{\n"
	"	vec4 pos = gl_ModelViewMatrix * vec4(a_pos, 1);\n"
	"	v_z = pos.z;\n"
	"	v_uv = a_uv;\n"
	"	v_selector = a_selector;\n"
	"	v_light_level = a_light_level;\n"
	"	gl_Position = gl_ProjectionMatrix * pos;\n"
	"}\n";

static const char* flat_shader_fragment_src =
	"#version 130\n"
	"\n"
	"varying vec2 v_uv;\n"
	"varying vec2 v_selector;\n"
	"varying float v_light_level;\n"
	"varying float v_z;\n"
	"\n"
	"uniform sampler2D u_flatlas;\n"
	"\n"
	DEF_LIGHT_FALLOFF_FN
	"\n"
	"void main(void)\n"
	"{\n"
	"	float flat_size = " STR_VALUE(MAGIC_FLAT_SIZE) ";\n"
	"	float atlas_size = " STR_VALUE(MAGIC_FLAT_ATLAS_SIZE) ";\n"
	"	vec2 uv = (clamp(fract(v_uv / flat_size) * flat_size, 0.5, flat_size - 0.5) + v_selector) / atlas_size;\n"
	"	float index = texture2D(u_flatlas, uv).r;\n"
	"	gl_FragColor = vec4(index, light_falloff_fn(v_light_level, v_z), 0, 1);\n"
	"}\n";

// type0 shader source
static const char* type0_shader_vertex_src =
	"#version 130\n"
	"\n"
	"attribute vec3 a_pos;\n"
	"attribute vec2 a_uv;\n"
	"attribute float a_light_level;\n"
	"\n"
	"varying vec2 v_uv;\n"
	"varying float v_z;\n"
	"varying float v_light_level;\n"
	"\n"
	"void main()\n"
	"{\n"
	"	vec4 pos = gl_ModelViewMatrix * vec4(a_pos, 1);\n"
	"	v_z = pos.z;\n"
	"	v_uv = a_uv;\n"
	"	v_light_level = a_light_level;\n"
	"	gl_Position = gl_ProjectionMatrix * pos;\n"
	"}\n";

static const char* type0_shader_fragment_src =
	"#version 130\n"
	"\n"
	"varying vec2 v_uv;\n"
	"varying float v_light_level;\n"
	"varying float v_z;\n"
	"\n"
	"uniform sampler2D u_texture;\n"
	"\n"
	DEF_LIGHT_FALLOFF_FN
	"\n"
	"void main(void)\n"
	"{\n"
	"	float index = texture2D(u_texture, v_uv).r;\n"
	"	if (index == 0) discard;\n" // transparency
	"	gl_FragColor = vec4(index, light_falloff_fn(v_light_level, v_z), 0, 1);\n"
	"}\n";


// step shader source
static const char* step_shader_vertex_src =
	"#version 130\n"
	"\n"
	"attribute vec2 a_pos;\n"
	"\n"
	"varying vec2 v_uv;\n"
	"\n"
	"void main()\n"
	"{\n"
	"	v_uv = a_pos;\n"
	"	gl_Position = gl_ModelViewProjectionMatrix * vec4(a_pos, 0, 1);\n"
	"}\n";

static const char* step_shader_fragment_src =
	"#version 130\n"
	"\n"
	"varying vec2 v_uv;\n"
	"\n"
	"uniform sampler2D u_screen;\n"
	"uniform sampler2D u_palette_lookup;\n"
	"\n"
	"void main(void)\n"
	"{\n"
	"	float width = " STR_VALUE(MAGIC_RWIDTH) ";\n"
	"	float height = " STR_VALUE(MAGIC_RHEIGHT) ";\n"
	"	vec2 nuv = vec2(v_uv.x / width, v_uv.y / height);\n"
	"	vec4 sample = texture2D(u_screen, nuv);\n"
	"	float index = sample.r;\n"
	"	float brite = 1-sample.g;\n"
	"	int d = int(v_uv.x) + int(v_uv.y);\n"
	"	if ((d&1) == 0) {\n"
	"		float halfstep = 1.0f / (float(" STR_VALUE(MAGIC_NUM_LIGHT_LEVELS) ")*2.0);\n"
	"		brite += halfstep;\n"
	"	}\n"
	"	gl_FragColor = texture2D(u_palette_lookup, vec2(index, brite));\n"
	"}\n";



static void render_init_palette_table(struct render* render)
{
	int width = 0;
	int height = 0;
	uint8_t* palette_table;
	AZ(mud_load_png_rgb("dgfx/palette_table.png", &palette_table, &width, &height));

	ASSERT(width == 256);
	ASSERT(height == MAGIC_NUM_LIGHT_LEVELS);

	int level = 0;
	int border = 0;

	glGenTextures(1, &render->palette_lookup_texture); CHKGL;
	glBindTexture(GL_TEXTURE_2D, render->palette_lookup_texture); CHKGL;
	glTexImage2D(GL_TEXTURE_2D, level, 3, width, height, border, GL_RGB, GL_UNSIGNED_BYTE, palette_table); CHKGL;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); CHKGL;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); CHKGL;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); CHKGL;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); CHKGL;

	free(palette_table);
}

static void render_init_flats(struct render* render)
{
	static char path[1024];

	size_t atlas_sz = MAGIC_FLAT_ATLAS_SIZE * MAGIC_FLAT_ATLAS_SIZE;
	uint8_t* atlas = malloc(atlas_sz);
	AN(atlas);

	int flats_per_row_exp = MAGIC_FLAT_ATLAS_SIZE_EXP - MAGIC_FLAT_SIZE_EXP;

	int slot = 0;
	for (const char** flat = names_flats; *flat; flat++) {
		uint8_t* data;
		int width = 0;
		int height = 0;

		strcpy(path, "gfx/");
		strcat(path, *flat);
		strcat(path, ".png");

		AZ(mud_load_png_paletted(path, &data, &width, &height));
		ASSERT(width == MAGIC_FLAT_SIZE);
		ASSERT(height == MAGIC_FLAT_SIZE);

		ASSERT(slot < (1 << (flats_per_row_exp << 1)));
		int x0 = (slot << MAGIC_FLAT_SIZE_EXP) & (MAGIC_FLAT_ATLAS_SIZE - 1);
		int y0 = (slot >> flats_per_row_exp) << MAGIC_FLAT_SIZE_EXP;
		ASSERT(x0 <= (MAGIC_FLAT_ATLAS_SIZE - width));
		ASSERT(y0 <= (MAGIC_FLAT_ATLAS_SIZE - height));
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				int atlasi = x0+x + ((y0+y) << MAGIC_FLAT_ATLAS_SIZE_EXP);
				ASSERT(atlasi >= 0 && atlasi < atlas_sz);
				int datai = x + (y << MAGIC_FLAT_SIZE_EXP);
				atlas[atlasi] = data[datai];
			}
		}

		free(data);

		slot++;
	}

	int level = 0;
	int border = 0;

	glGenTextures(1, &render->flatlas_texture); CHKGL;
	glBindTexture(GL_TEXTURE_2D, render->flatlas_texture); CHKGL;
	glTexImage2D(GL_TEXTURE_2D, level, 1, MAGIC_FLAT_ATLAS_SIZE, MAGIC_FLAT_ATLAS_SIZE, border, GL_RED, GL_UNSIGNED_BYTE, atlas); CHKGL;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); CHKGL;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); CHKGL;

	free(atlas);
}

static void render_load_texture_list(struct render_texture* textures, int max, const char* names[])
{
	static char path[1024];
	int index = 0;
	for (const char** name = names; *name; name++) {
		ASSERT(index <= max);
		struct render_texture* texture = &textures[index];

		uint8_t* data;

		strcpy(path, "gfx/");
		strcat(path, *name);
		strcat(path, ".png");

		AZ(mud_load_png_paletted(path, &data, &texture->width, &texture->height));

		int level = 0;
		int border = 0;

		glGenTextures(1, &texture->texture); CHKGL;
		glBindTexture(GL_TEXTURE_2D, texture->texture); CHKGL;
		glTexImage2D(GL_TEXTURE_2D, level, 1, texture->width, texture->height, border, GL_RED, GL_UNSIGNED_BYTE, data); CHKGL;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); CHKGL;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); CHKGL;

		free(data);

		index++;
	}
}

static void render_init_walls(struct render* render)
{
	render_load_texture_list(render->walls, MAX_WALLS, names_walls);
}

static void render_init_sprites(struct render* render)
{
	render_load_texture_list(render->sprites, MAX_SPRITES, names_sprites);
}

static void render_init_framebuffers(struct render* render)
{
	int width = MAGIC_RWIDTH;
	int height = MAGIC_RHEIGHT;
	int level = 0;
	int border = 0;

	// screen framebuffer
	glGenFramebuffers(1, &render->screen_framebuffer); CHKGL;
	glBindFramebuffer(GL_FRAMEBUFFER, render->screen_framebuffer); CHKGL;

	glGenTextures(1, &render->screen_texture); CHKGL;
	glBindTexture(GL_TEXTURE_2D, render->screen_texture); CHKGL;

	glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, width, height, border, GL_RGBA, GL_UNSIGNED_BYTE, NULL); CHKGL;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); CHKGL;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); CHKGL;

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render->screen_texture, level); CHKGL;

	// a renderbuffer is almost the same as a texture;
	// glRenderbufferStorage is similar to glTexImage2D, and
	// glFramebufferRenderbuffer is similar to glFramebufferTexture2D.
	// HOWEVER, a renderbuffer cannot be used as a texture (so it cannot be
	// sampled, or blitted to a screen). so it only makes sense e.g. for a
	// zbuffer like used here.
	// SEE ~/priv/git/deeper for an example of using a texture instead.
	glGenRenderbuffers(1, &render->screen_depthbuffer); CHKGL;
	glBindRenderbuffer(GL_RENDERBUFFER, render->screen_depthbuffer); CHKGL;
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, width, height); CHKGL;
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, render->screen_depthbuffer); CHKGL;

	ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);


	// step framebuffer
	glGenFramebuffers(1, &render->step_framebuffer); CHKGL;
	glBindFramebuffer(GL_FRAMEBUFFER, render->step_framebuffer); CHKGL;

	glGenTextures(1, &render->step_texture); CHKGL;
	glBindTexture(GL_TEXTURE_2D, render->step_texture); CHKGL;

	glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, width, height, border, GL_RGBA, GL_UNSIGNED_BYTE, NULL); CHKGL;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); CHKGL;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); CHKGL;

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render->step_texture, level); CHKGL;

	ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);


	// bind default framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0); CHKGL;
}

static void render_init_shaders(struct render* render)
{
	// flat shader
	shader_init(&render->flat_shader, flat_shader_vertex_src, flat_shader_fragment_src);
	shader_use(&render->flat_shader);
	render->flat_a_pos = glGetAttribLocation(render->flat_shader.program, "a_pos"); CHKGL;
	render->flat_a_uv = glGetAttribLocation(render->flat_shader.program, "a_uv"); CHKGL;
	render->flat_a_selector = glGetAttribLocation(render->flat_shader.program, "a_selector"); CHKGL;
	render->flat_a_light_level = glGetAttribLocation(render->flat_shader.program, "a_light_level"); CHKGL;
	glUniform1i(glGetUniformLocation(render->flat_shader.program, "u_flatlas"), 0); CHKGL;

	// wall shader
	shader_init(&render->type0_shader, type0_shader_vertex_src, type0_shader_fragment_src);
	shader_use(&render->type0_shader);
	render->type0_a_pos = glGetAttribLocation(render->type0_shader.program, "a_pos"); CHKGL;
	render->type0_a_uv = glGetAttribLocation(render->type0_shader.program, "a_uv"); CHKGL;
	render->type0_a_light_level = glGetAttribLocation(render->type0_shader.program, "a_light_level"); CHKGL;
	glUniform1i(glGetUniformLocation(render->type0_shader.program, "u_texture"), 0); CHKGL;

	// step shader
	shader_init(&render->step_shader, step_shader_vertex_src, step_shader_fragment_src);
	shader_use(&render->step_shader);
	render->step_a_pos = glGetAttribLocation(render->step_shader.program, "a_pos"); CHKGL;
	glUniform1i(glGetUniformLocation(render->step_shader.program, "u_screen"), 0); CHKGL;
	glUniform1i(glGetUniformLocation(render->step_shader.program, "u_palette_lookup"), 1); CHKGL;

	glUseProgram(0);
}

#define RENDER_BUFSZ (16384)

static void static_quad_buffers(GLuint* vertex_buffer, GLuint* index_buffer, int width, int height)
{
	glGenBuffers(1, vertex_buffer); CHKGL;
	glBindBuffer(GL_ARRAY_BUFFER, *vertex_buffer); CHKGL;
	size_t vertex_sz = sizeof(float) * 2 * 4;
	float* vertex_data = malloc(vertex_sz);
	AN(vertex_data);

	vertex_data[0] = 0;
	vertex_data[1] = 0;

	vertex_data[2] = width;
	vertex_data[3] = 0;

	vertex_data[4] = width;
	vertex_data[5] = height;

	vertex_data[6] = 0;
	vertex_data[7] = height;

	glBufferData(GL_ARRAY_BUFFER, vertex_sz, vertex_data, GL_STATIC_DRAW); CHKGL;

	glGenBuffers(1, index_buffer); CHKGL;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *index_buffer); CHKGL;
	size_t index_sz = 6;
	uint8_t* index_data = malloc(index_sz);
	AN(index_data);

	index_data[0] = 0;
	index_data[1] = 1;
	index_data[2] = 2;

	index_data[3] = 0;
	index_data[4] = 2;
	index_data[5] = 3;

	glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_sz, index_data, GL_STATIC_DRAW); CHKGL;
}

static void render_init_buffers(struct render* render)
{
	// flat buffers
	glGenBuffers(1, &render->flat_vertex_buffer); CHKGL;
	glBindBuffer(GL_ARRAY_BUFFER, render->flat_vertex_buffer); CHKGL;
	size_t flat_vertex_data_sz = RENDER_BUFSZ * sizeof(float) * FLOATS_PER_FLAT_VERTEX;
	render->flat_vertex_data = malloc(flat_vertex_data_sz);
	AN(render->flat_vertex_data);
	glBufferData(GL_ARRAY_BUFFER, flat_vertex_data_sz, render->flat_vertex_data, GL_STREAM_DRAW); CHKGL;

	glGenBuffers(1, &render->flat_index_buffer); CHKGL;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render->flat_index_buffer); CHKGL;
	size_t flat_index_data_sz = RENDER_BUFSZ * sizeof(uint32_t);
	render->flat_index_data = malloc(flat_index_data_sz);
	AN(render->flat_index_data);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, flat_index_data_sz, render->flat_index_data, GL_STREAM_DRAW); CHKGL;

	// wall buffers
	glGenBuffers(1, &render->type0_vertex_buffer); CHKGL;
	glBindBuffer(GL_ARRAY_BUFFER, render->type0_vertex_buffer); CHKGL;
	size_t wall_vertex_data_sz = RENDER_BUFSZ * sizeof(float) * FLOATS_PER_TYPE0_VERTEX;
	render->type0_vertex_data = malloc(wall_vertex_data_sz);
	AN(render->type0_vertex_data);
	glBufferData(GL_ARRAY_BUFFER, wall_vertex_data_sz, render->type0_vertex_data, GL_STREAM_DRAW); CHKGL;

	glGenBuffers(1, &render->type0_index_buffer); CHKGL;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render->type0_index_buffer); CHKGL;
	size_t wall_index_data_sz = RENDER_BUFSZ * sizeof(uint32_t);
	render->type0_index_data = malloc(wall_index_data_sz);
	AN(render->type0_index_data);
	int offset = 0;
	for (int i = 0; i < (RENDER_BUFSZ-6); i += 6) {
		render->type0_index_data[i+0] = 0 + offset;
		render->type0_index_data[i+1] = 1 + offset;
		render->type0_index_data[i+2] = 2 + offset;
		render->type0_index_data[i+3] = 0 + offset;
		render->type0_index_data[i+4] = 2 + offset;
		render->type0_index_data[i+5] = 3 + offset;
		offset += 4;
	}
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, wall_index_data_sz, render->type0_index_data, GL_STATIC_DRAW); CHKGL;

	// step buffers
	static_quad_buffers(&render->step_vertex_buffer, &render->step_index_buffer, MAGIC_RWIDTH, MAGIC_RHEIGHT);
}

static void render_init_tagstuff(struct render* render)
{
	render->tags_flat_vertices = malloc(sizeof(struct vec3) * RENDER_BUFSZ);
	AN(render->tags_flat_vertices);

	render->tags_flat_indices = malloc(sizeof(int) * RENDER_BUFSZ);
	AN(render->tags_flat_indices);
}

void render_init(struct render* render, SDL_Window* window)
{
	glew_init();

	render->window = window;

	render_init_palette_table(render);
	render_init_flats(render);
	render_init_walls(render);
	render_init_sprites(render);
	render_init_framebuffers(render);
	render_init_shaders(render);
	render_init_buffers(render);
	render_init_tagstuff(render);
}

void render_set_entity_cam(struct render* render, struct lvl_entity* entity)
{
	render->entity_cam = entity;
}

float render_get_fovy(struct render* render)
{
	AN(render);
	return 65;
}


static void* tess_alloc(void* usr, unsigned int sz)
{
	//printf("my_malloc(%u)\n", sz);
	return malloc(sz);
}

static void* tess_realloc(void* usr, void* ptr, unsigned int sz)
{
	//printf("my_realloc(%u)\n", sz);
	return realloc(ptr, sz);
}

static void tess_free(void* usr, void* ptr)
{
	//printf("my_free()\n");
	return free(ptr);
}


static void renderctx_add_flat_vertex(struct render* render, float x, float y, float z, float u, float v)
{
	ASSERT(render->flat_vertex_n < RENDER_BUFSZ);
	float* data = &render->flat_vertex_data[render->flat_vertex_n * FLOATS_PER_FLAT_VERTEX];
	int i = 0;
	data[i++] = x;
	data[i++] = y;
	data[i++] = z;
	data[i++] = u;
	data[i++] = v;
	data[i++] = render->current_select_u;
	data[i++] = render->current_select_v;
	data[i++] = render->current_light_level;
	//printf("%f %f %f\n", x, y, z);
	render->flat_vertex_n++;
}

static void resolve_select(int i, float* select_u, float* select_v)
{
	ASSERT(i >= 0 && i < 256);
	*select_u = (i << MAGIC_FLAT_SIZE_EXP) & (MAGIC_FLAT_ATLAS_SIZE - 1);
	*select_v = (i >> (MAGIC_FLAT_ATLAS_SIZE_EXP - MAGIC_FLAT_SIZE_EXP)) << MAGIC_FLAT_SIZE_EXP;
}

static void renderctx_begin_flat(struct render* render, struct lvl* lvl, int sectori, int flati)
{
	render->flat_offset = render->flat_vertex_n;
	struct lvl_sector* sector = lvl_get_sector(lvl, sectori);
	struct lvl_flat* flat = &sector->flat[flati];

	resolve_select(flat->texture, &render->current_select_u, &render->current_select_v);
	render->current_light_level = sector->light_level;
}

static void renderctx_add_flat_triangle(struct render* render, uint32_t indices[3])
{
	ASSERT((render->flat_index_n+3) <= RENDER_BUFSZ);
	for (int i = 0; i < 3; i++) {
		render->flat_index_data[render->flat_index_n++] = render->flat_offset + indices[i];
	}
}

static void yield_flat_partial(struct render* render, struct lvl* lvl, int sectori, int flati)
{
	struct lvl_sector* sector = lvl_get_sector(lvl, sectori);
	struct lvl_flat* flat = &sector->flat[flati];

	TESSalloc ta;

	ta.memalloc = tess_alloc;
	ta.memrealloc = tess_realloc;
	ta.memfree = tess_free;
	ta.userData = NULL;
	ta.meshEdgeBucketSize = 64;
	ta.meshVertexBucketSize = 64;
	ta.meshFaceBucketSize = 32;
	ta.dictNodeBucketSize = 64;
	ta.regionBucketSize = 32;
	ta.extraVertices = 0;

	TESStesselator* t = tessNewTess(&ta);

	struct vec2 points[16384];

	int32_t p = 0;
	int32_t p0 = -1;

	int begun = 0;
	ASSERT(sector->contourn > 0);
	for (int i = 0; i < sector->contourn; i++) {
		int ci = sector->contour0 + i;

		if (p0 == -1) p0 = p;

		struct lvl_contour* c = lvl_get_contour(lvl, ci);

		struct lvl_linedef* l = lvl_get_linedef(lvl, c->linedef);
		struct vec2* v = lvl_get_vertex(lvl, l->vertex[c->usr&1]);
		AN(v);

		if (LVL_CONTOUR_IS_FIRST(c)) {
			p0 = p;
			AZ(begun);
			begun = 1;
		}

		memcpy(&points[p], v, sizeof(struct vec2));

		if (LVL_CONTOUR_IS_LAST(c)) {
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

	render->begin_flat(render, lvl, sectori, flati);

	int nverts = tessGetVertexCount(t);
	const float* verts = tessGetVertices(t);
	for (int i = 0; i < nverts; i++) {
		struct plane* plane = &flat->plane;
		struct vec2 v;
		v.s[0] = verts[i*2];
		v.s[1] = verts[i*2+1];
		float z = plane_z(plane, &v);
		struct vec2 uv;
		vec2_copy(&uv, &v);
		mat23_applyi(&flat->tx, &uv);
		AN(render->add_flat_vertex);
		render->add_flat_vertex(
			render,
			v.s[0], z, v.s[1], // XYZ
			uv.s[0], uv.s[1]
		);
	}

	int nelems = tessGetElementCount(t);
	const int* elems = tessGetElements(t);

	for (int i = 0; i < nelems; i++) {
		const int* p = &elems[i * nvp];
		uint32_t indices[3];
		for (int j = 0; j < nvp; j++) {
			ASSERT(p[j] != TESS_UNDEF);
			int k = flati ? nvp - 1 - j : j;
			indices[j] = p[k];
		}
		AN(render->add_flat_triangle);
		render->add_flat_triangle(render, indices);
	}

	if (render->end_flat) render->end_flat(render);

	tessDeleteTess(t);
}

static void yield_flats(struct render* render, struct lvl* lvl)
{
	for (int i = 0; i < lvl->n_sectors; i++) {
		yield_flat_partial(render, lvl, i, 0);
		yield_flat_partial(render, lvl, i, 1);
	}

}
static void renderctx_begin_wall(struct render* render, struct lvl* lvl, int sectori, int contouri, int dz)
{
	struct lvl_sector* sector = lvl_get_sector(lvl, sectori);
	render->current_light_level = sector->light_level;

	struct lvl_contour* contour = lvl_get_contour(lvl, contouri);
	struct lvl_linedef* ld = lvl_get_linedef(lvl, contour->linedef);
	uint32_t sdi = ld->sidedef[contour->usr&1];
	ASSERT(sdi != -1);
	struct lvl_sidedef* sd = lvl_get_sidedef(lvl, sdi);

	int texture = sd->texture[dz <= 0 ? 0 : 1];
	if (texture == render->wall_current_texture) {
		render->wall_enable = 1;
	} else {
		render->wall_enable = 0;
	}
	if (texture > render->wall_current_texture && (render->wall_next_texture == -1 || texture < render->wall_next_texture)) {
		render->wall_next_texture = texture;
	}
}

static void render_add_type0_vertex(struct render* render, float x, float y, float z, float u, float v, float ll)
{
	ASSERT(render->type0_vertex_n < RENDER_BUFSZ);
	float* data = &render->type0_vertex_data[render->type0_vertex_n * FLOATS_PER_TYPE0_VERTEX];
	int i = 0;
	data[i++] = x;
	data[i++] = y;
	data[i++] = z;
	data[i++] = u;
	data[i++] = v;
	data[i++] = ll;
	render->type0_vertex_n++;
}

static void renderctx_add_wall_vertex(struct render* render, float x, float y, float z, float u, float v)
{
	if (!render->wall_enable) return;
	ASSERT(render->type0_vertex_n < RENDER_BUFSZ);
	struct render_texture* texture = &render->walls[render->wall_current_texture];
	render_add_type0_vertex(render, x, y, z, u / (float)texture->width, v / (float)texture->height, render->current_light_level);
}

static void yield_walls(struct render* render, struct lvl* lvl)
{
	for (int sectori = 0; sectori < lvl->n_sectors; sectori++) {
		struct lvl_sector* sector = lvl_get_sector(lvl, sectori);

		for (int cdi = 0; cdi < sector->contourn; cdi++) {
			int ci = sector->contour0 + cdi;

			struct lvl_contour* c = lvl_get_contour(lvl, ci);
			struct lvl_linedef* l = lvl_get_linedef(lvl, c->linedef);

			struct vec2* v0 = lvl_get_vertex(lvl, l->vertex[c->usr&1]);
			struct vec2* v1 = lvl_get_vertex(lvl, l->vertex[(c->usr&1)^1]);
			struct lvl_sidedef* sd = l->sidedef[c->usr&1] == -1 ? NULL : lvl_get_sidedef(lvl, l->sidedef[c->usr&1]);
			struct lvl_sidedef* sopp = l->sidedef[(c->usr&1)^1] == -1 ? NULL : lvl_get_sidedef(lvl, l->sidedef[(c->usr&1)^1]);

			AN(v0);
			AN(v1);
			AN(sd);

			struct vec2 vd;
			vec2_sub(&vd, v1, v0);

			float vd_length = vec2_length(&vd);

			for (int zd = -1; zd <= 1; zd++) {
				if (sopp == NULL && zd != 0) continue;
				if (sopp != NULL && zd == 0) continue;

				float u0 = 0;
				float u1 = vd_length;

				float z0,z1,z2,z3;

				if (zd == 0) {
					z0 = plane_z(&sector->flat[1].plane, v0);
					z1 = plane_z(&sector->flat[1].plane, v1);
					z2 = plane_z(&sector->flat[0].plane, v1);
					z3 = plane_z(&sector->flat[0].plane, v0);
				} else {
					struct lvl_sector* sector1 = lvl_get_sector(lvl, sopp->sector);
					if (zd == -1) {
						z0 = plane_z(&sector1->flat[0].plane, v0);
						z1 = plane_z(&sector1->flat[0].plane, v1);
						z2 = plane_z(&sector->flat[0].plane, v1);
						z3 = plane_z(&sector->flat[0].plane, v0);
						if (z1 < z2 && z0 < z3) continue; // XXX no; handle crossing
					} else if (zd == 1) {
						z0 = plane_z(&sector->flat[1].plane, v0);
						z1 = plane_z(&sector->flat[1].plane, v1);
						z2 = plane_z(&sector1->flat[1].plane, v1);
						z3 = plane_z(&sector1->flat[1].plane, v0);
						if (z2 > z1 && z3 > z0) continue; // XXX no; handle crossing
					} else {
						AZ(1);
					}
				}

				struct vec2 uv[4] = {
					{{u0, z0}},
					{{u1, z1}},
					{{u1, z2}},
					{{u0, z3}}
				};

				struct mat23* tx = &sd->tx[zd <= 0 ? 0 : 1];
				for (int uvi = 0; uvi < 4; uvi++) {
					mat23_applyi(tx, &uv[uvi]);
				}

				if (render->begin_wall) render->begin_wall(render, lvl, sectori, ci, zd);
				render->add_wall_vertex(render, v0->s[0], z0, v0->s[1], uv[0].s[0], uv[0].s[1]);
				render->add_wall_vertex(render, v1->s[0], z1, v1->s[1], uv[1].s[0], uv[1].s[1]);
				render->add_wall_vertex(render, v1->s[0], z2, v1->s[1], uv[2].s[0], uv[2].s[1]);
				render->add_wall_vertex(render, v0->s[0], z3, v0->s[1], uv[3].s[0], uv[3].s[1]);
				if (render->end_wall) render->end_wall(render);
			}
		}
	}
}


static void flat_callbacks(
	struct render* render,
	void (*begin_flat)(
		struct render* render,
		struct lvl* lvl,
		int sectori,
		int flati
	),
	void (*add_flat_vertex)(
		struct render* render,
		float x, float y, float z,
		float u, float v
	),
	void (*add_flat_triangle)(
		struct render* render,
		uint32_t indices[3]
	),
	void (*end_flat)(struct render* render))
{

	AN(begin_flat);
	AN(add_flat_vertex);
	AN(add_flat_triangle);
	render->begin_flat = begin_flat;
	render->add_flat_vertex = add_flat_vertex;
	render->add_flat_triangle = add_flat_triangle;
	render->end_flat = end_flat;

	render->begin_wall = NULL;
	render->add_wall_vertex = NULL;
	render->end_wall = NULL;
}

static void wall_callbacks(
	struct render* render,
	void (*begin_wall)(
		struct render* render,
		struct lvl* lvl,
		int sectori,
		int contouri,
		int dz
	),
	void (*add_wall_vertex)(
		struct render* render,
		float x, float y, float z,
		float u, float v
	),
	void (*end_wall)(struct render* render))
{
	AN(begin_wall);
	AN(add_wall_vertex);

	render->begin_wall = begin_wall;
	render->add_wall_vertex = add_wall_vertex;
	render->end_wall = end_wall;

	render->begin_flat = NULL;
	render->add_flat_vertex = NULL;
	render->add_flat_triangle = NULL;
	render->end_flat = NULL;
}


static void render_walls(struct render* render, struct lvl* lvl)
{
	wall_callbacks(render, renderctx_begin_wall, renderctx_add_wall_vertex, NULL);

	render->wall_next_texture = -1;

	shader_use(&render->type0_shader);

	glActiveTexture(GL_TEXTURE0); CHKGL;
	glEnable(GL_TEXTURE_2D); CHKGL;

	glEnableVertexAttribArray(render->type0_a_pos); CHKGL;
	glEnableVertexAttribArray(render->type0_a_uv); CHKGL;
	glEnableVertexAttribArray(render->type0_a_light_level); CHKGL;

	do {
		render->type0_vertex_n = 0;
		render->wall_current_texture = render->wall_next_texture;
		render->wall_next_texture = -1;
		yield_walls(render, lvl);

		if (render->wall_current_texture == -1) continue;

		glBindTexture(GL_TEXTURE_2D, render->walls[render->wall_current_texture].texture); CHKGL;

		glBindBuffer(GL_ARRAY_BUFFER, render->type0_vertex_buffer); CHKGL;
		glBufferSubData(GL_ARRAY_BUFFER, 0, render->type0_vertex_n * sizeof(float) * FLOATS_PER_TYPE0_VERTEX, render->type0_vertex_data); CHKGL;

		glVertexAttribPointer(render->type0_a_pos, 3, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_TYPE0_VERTEX, 0); CHKGL;
		glVertexAttribPointer(render->type0_a_uv, 2, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_TYPE0_VERTEX, (char*)(sizeof(float)*3)); CHKGL;
		glVertexAttribPointer(render->type0_a_light_level, 1, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_TYPE0_VERTEX, (char*)(sizeof(float)*5)); CHKGL;

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render->type0_index_buffer); CHKGL;
		glDrawElements(GL_TRIANGLES, render->type0_vertex_n/4*6, GL_UNSIGNED_INT, NULL); CHKGL;
	} while (render->wall_next_texture != -1);

	glDisableVertexAttribArray(render->type0_a_light_level); CHKGL;
	glDisableVertexAttribArray(render->type0_a_uv); CHKGL;
	glDisableVertexAttribArray(render->type0_a_pos); CHKGL;

	glActiveTexture(GL_TEXTURE0); CHKGL;
	glDisable(GL_TEXTURE_2D); CHKGL;
}

static void render_step(struct render* render)
{
	glBindFramebuffer(GL_FRAMEBUFFER, render->step_framebuffer); CHKGL;
	glViewport(0, 0, MAGIC_RWIDTH, MAGIC_RHEIGHT);

	glMatrixMode(GL_PROJECTION); CHKGL;
	glLoadIdentity(); CHKGL;
	glOrtho(0,MAGIC_RWIDTH,0,MAGIC_RHEIGHT,-1,1); CHKGL;

	glMatrixMode(GL_MODELVIEW); CHKGL;
	glLoadIdentity(); CHKGL;

	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	shader_use(&render->step_shader);

	glActiveTexture(GL_TEXTURE0); CHKGL;
	glEnable(GL_TEXTURE_2D); CHKGL;
	glBindTexture(GL_TEXTURE_2D, render->screen_texture); CHKGL;

	glActiveTexture(GL_TEXTURE1); CHKGL;
	glEnable(GL_TEXTURE_2D); CHKGL;
	glBindTexture(GL_TEXTURE_2D, render->palette_lookup_texture); CHKGL;

	glBindBuffer(GL_ARRAY_BUFFER, render->step_vertex_buffer); CHKGL;

	glEnableVertexAttribArray(render->step_a_pos); CHKGL;
	glVertexAttribPointer(render->step_a_pos, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0); CHKGL;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render->step_index_buffer); CHKGL;
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, NULL); CHKGL;

	glDisableVertexAttribArray(render->step_a_pos); CHKGL;

	glActiveTexture(GL_TEXTURE1); CHKGL;
	glDisable(GL_TEXTURE_2D); CHKGL;
	glActiveTexture(GL_TEXTURE0); CHKGL;
	glDisable(GL_TEXTURE_2D); CHKGL;
}

static void gl_viewport_from_sdl_window(SDL_Window* window)
{
	int w, h;
	SDL_GetWindowSize(window, &w, &h);
	glViewport(0, 0, w, h); CHKGL;
}

static void render_blit(struct render* render)
{
	glUseProgram(0); CHKGL;
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST); CHKGL;
	glDisable(GL_CULL_FACE); CHKGL;

	glBindFramebuffer(GL_FRAMEBUFFER, 0); CHKGL;
	gl_viewport_from_sdl_window(render->window);

	glMatrixMode(GL_PROJECTION); CHKGL;
	glLoadIdentity(); CHKGL;
	glOrtho(0,1,0,1,-1,1); CHKGL;

	glMatrixMode(GL_MODELVIEW); CHKGL;
	glLoadIdentity(); CHKGL;

	glActiveTexture(GL_TEXTURE0); CHKGL;
	glEnable(GL_TEXTURE_2D); CHKGL;
	glBindTexture(GL_TEXTURE_2D, render->step_texture);

	glColor4f(1,1,1,1);
	glBegin(GL_QUADS);
	glMultiTexCoord2f(GL_TEXTURE0, 0, 0); glVertex2f(0,0);
	glMultiTexCoord2f(GL_TEXTURE0, 1, 0); glVertex2f(1,0);
	glMultiTexCoord2f(GL_TEXTURE0, 1, 1); glVertex2f(1,1);
	glMultiTexCoord2f(GL_TEXTURE0, 0, 1); glVertex2f(0,1);
	glEnd();

}
static void render_flats(struct render* render, struct lvl* lvl)
{
	render->flat_vertex_n = 0;
	render->flat_index_n = 0;
	flat_callbacks(
		render,
		renderctx_begin_flat,
		renderctx_add_flat_vertex,
		renderctx_add_flat_triangle,
		NULL
	);
	yield_flats(render, lvl);

	AZ(render->flat_index_n % 3); // threeangles!

	shader_use(&render->flat_shader);

	glActiveTexture(GL_TEXTURE0); CHKGL;
	glEnable(GL_TEXTURE_2D); CHKGL;
	glBindTexture(GL_TEXTURE_2D, render->flatlas_texture); CHKGL;

	glBindBuffer(GL_ARRAY_BUFFER, render->flat_vertex_buffer); CHKGL;
	glBufferSubData(GL_ARRAY_BUFFER, 0, render->flat_vertex_n * sizeof(float) * FLOATS_PER_FLAT_VERTEX, render->flat_vertex_data); CHKGL;

	glEnableVertexAttribArray(render->flat_a_pos); CHKGL;
	glVertexAttribPointer(render->flat_a_pos, 3, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_FLAT_VERTEX, 0); CHKGL;

	glEnableVertexAttribArray(render->flat_a_uv); CHKGL;
	glVertexAttribPointer(render->flat_a_uv, 2, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_FLAT_VERTEX, (char*)(sizeof(float)*3)); CHKGL;

	glEnableVertexAttribArray(render->flat_a_selector); CHKGL;
	glVertexAttribPointer(render->flat_a_selector, 2, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_FLAT_VERTEX, (char*)(sizeof(float)*5)); CHKGL;

	glEnableVertexAttribArray(render->flat_a_light_level); CHKGL;
	glVertexAttribPointer(render->flat_a_light_level, 1, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_FLAT_VERTEX, (char*)(sizeof(float)*7)); CHKGL;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render->flat_index_buffer); CHKGL;
	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, render->flat_index_n * sizeof(uint32_t), render->flat_index_data); CHKGL;

	glDrawElements(GL_TRIANGLES, render->flat_index_n, GL_UNSIGNED_INT, NULL); CHKGL;

	glDisableVertexAttribArray(render->flat_a_light_level); CHKGL;
	glDisableVertexAttribArray(render->flat_a_selector); CHKGL;
	glDisableVertexAttribArray(render->flat_a_uv); CHKGL;
	glDisableVertexAttribArray(render->flat_a_pos); CHKGL;

	glActiveTexture(GL_TEXTURE0); CHKGL;
	glDisable(GL_TEXTURE_2D); CHKGL;
}


static void gl_transform(struct render* render)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	float fovy = render_get_fovy(render);
	float aspect = (float)MAGIC_RWIDTH/(float)MAGIC_RHEIGHT;
	gluPerspective(fovy, aspect, 0.1, 4096);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glRotatef(render->entity_cam->pitch, 1, 0, 0);
	glRotatef(render->entity_cam->yaw, 0, 1, 0);
	glTranslatef(-render->entity_cam->position.s[0], -render->entity_cam->z, -render->entity_cam->position.s[1]);

}

void render_begin2d(struct render* render)
{
	glBindFramebuffer(GL_FRAMEBUFFER, render->screen_framebuffer); CHKGL;
	glViewport(0, 0, MAGIC_RWIDTH, MAGIC_RHEIGHT);

	glDisable(GL_DEPTH_TEST); CHKGL;
	glDisable(GL_CULL_FACE); CHKGL;

	glMatrixMode(GL_PROJECTION); CHKGL;
	glLoadIdentity(); CHKGL;
	glOrtho(0,MAGIC_RWIDTH,MAGIC_RHEIGHT,0,-1,1); CHKGL;

	glMatrixMode(GL_MODELVIEW); CHKGL;
	glLoadIdentity();
}

static void render_lvl_geom(struct render* render, struct lvl* lvl)
{
	render_flats(render, lvl);
	render_walls(render, lvl);
}

static void render_lvl_entities(struct render* render, struct lvl* lvl)
{
	shader_use(&render->type0_shader);

	glActiveTexture(GL_TEXTURE0); CHKGL;
	glEnable(GL_TEXTURE_2D); CHKGL;

	glEnableVertexAttribArray(render->type0_a_pos); CHKGL;
	glEnableVertexAttribArray(render->type0_a_uv); CHKGL;
	glEnableVertexAttribArray(render->type0_a_light_level); CHKGL;

	int next_texture = -1;
	int current_texture;

	// (FIXME maybe sprites ought to be atlas based?)
	do {
		render->type0_vertex_n = 0;
		current_texture = next_texture;
		next_texture = -1;
		for (int i = 0; i < lvl->n_entities; i++) {
			struct lvl_entity* e = lvl_get_entity(lvl, i);
			if (e->type == ENTITY_DELETED) continue;

			int texture = 0; // XXX TODO FIXME who knows this?

			if (texture == current_texture) {
				struct lvl_sector* sector = lvl_get_sector(lvl, e->sector);
				float ll = sector->light_level;
				struct render_texture* t = &render->sprites[texture];

				struct vec2* p = &e->position;
				struct vec2 iv;
				vec2_sub(&iv, &render->entity_cam->position, p);
				struct vec2 ivn;
				vec2_normal(&ivn, &iv);
				vec2_normalize(&ivn);
				vec2_scalei(&ivn, (float)t->width / 2.0);
				float z0 = e->z + MAGIC_EVEN_MORE_MAGIC_ENTITY_HEIGHT;
				float z1 = z0 - (float)t->height;

				struct vec2 p0;
				vec2_copy(&p0, p);
				vec2_add_scalei(&p0, &ivn, -1.0);

				struct vec2 p1;
				vec2_copy(&p1, p);
				vec2_add_scalei(&p1, &ivn, 1.0);

				render_add_type0_vertex(
					render,
					p0.s[0],
					z1,
					p0.s[1],
					0,1,
					ll
				);

				render_add_type0_vertex(
					render,
					p1.s[0],
					z1,
					p1.s[1],
					1,1,
					ll
				);

				render_add_type0_vertex(
					render,
					p1.s[0],
					z0,
					p1.s[1],
					1,0,
					ll
				);

				render_add_type0_vertex(
					render,
					p0.s[0],
					z0,
					p0.s[1],
					0,0,
					ll
				);
			}
			if (texture > current_texture && (next_texture == -1 || texture < next_texture)) {
				next_texture = texture;
			}
		}

		if (current_texture == -1) continue;

		glBindTexture(GL_TEXTURE_2D, render->sprites[current_texture].texture); CHKGL;

		//printf("spr: %d\n", render->type0_vertex_n);

		glBindBuffer(GL_ARRAY_BUFFER, render->type0_vertex_buffer); CHKGL;
		glBufferSubData(GL_ARRAY_BUFFER, 0, render->type0_vertex_n * sizeof(float) * FLOATS_PER_TYPE0_VERTEX, render->type0_vertex_data); CHKGL;

		glVertexAttribPointer(render->type0_a_pos, 3, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_TYPE0_VERTEX, 0); CHKGL;
		glVertexAttribPointer(render->type0_a_uv, 2, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_TYPE0_VERTEX, (char*)(sizeof(float)*3)); CHKGL;
		glVertexAttribPointer(render->type0_a_light_level, 1, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_TYPE0_VERTEX, (char*)(sizeof(float)*5)); CHKGL;

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render->type0_index_buffer); CHKGL;
		glDrawElements(GL_TRIANGLES, render->type0_vertex_n/4*6, GL_UNSIGNED_INT, NULL); CHKGL;
	} while (next_texture != -1);

	glDisableVertexAttribArray(render->type0_a_light_level); CHKGL;
	glDisableVertexAttribArray(render->type0_a_uv); CHKGL;
	glDisableVertexAttribArray(render->type0_a_pos); CHKGL;

	glActiveTexture(GL_TEXTURE0); CHKGL;
	glDisable(GL_TEXTURE_2D); CHKGL;
}

void render_lvl(struct render* render, struct lvl* lvl)
{
	gl_transform(render);

	glBindFramebuffer(GL_FRAMEBUFFER, render->screen_framebuffer); CHKGL;
	glViewport(0, 0, MAGIC_RWIDTH, MAGIC_RHEIGHT);
	glClearColor(0,0,0,0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); CHKGL;
	glEnable(GL_DEPTH_TEST);

	render_lvl_geom(render, lvl);
	render_lvl_entities(render, lvl);
}

void render_flip(struct render* render)
{
	render_step(render);
	render_blit(render);

	glUseProgram(0); CHKGL;
	glBindFramebuffer(GL_FRAMEBUFFER, 0); CHKGL;
}

static void tagsctx_gl_color(int hover, int selected)
{
	if (selected) {
		glColor4f(1,1,0,0.3);
	} else if (hover) {
		glColor4f(1,1,1,0.1);
	} else {
		glColor4f(1,1,1,0);
	}
}

static void tagsctx_begin_flat(struct render* render, struct lvl* lvl, int sectori, int flati)
{
	struct lvl_sector* sector = lvl_get_sector(lvl, sectori);
	int hover = ((flati == 0 && (sector->usr & LVL_HIGHLIGHTED_ZMINUS)) || (flati == 1 && (sector->usr & LVL_HIGHLIGHTED_ZPLUS)));
	int selected = ((flati == 0 && (sector->usr & LVL_SELECTED_ZMINUS)) || (flati == 1 && (sector->usr & LVL_SELECTED_ZPLUS)));
	if (hover || selected) {
		render->tags_flat_vertex_n = 0;
		render->tags_flat_index_n = 0;
		tagsctx_gl_color(hover, selected);
	} else {
		render->tags_flat_vertex_n = -1;
		render->tags_flat_index_n = -1;
	}
}

static void tagsctx_add_flat_vertex(struct render* render, float x, float y, float z, float u, float v)
{
	if (render->tags_flat_vertex_n < 0) return;
	struct vec3 p;
	p.s[0] = x;
	p.s[1] = y;
	p.s[2] = z;
	vec3_copy(&render->tags_flat_vertices[render->tags_flat_vertex_n++], &p);

}

static void tagsctx_add_flat_triangle(struct render* render, uint32_t indices[3])
{
	if (render->tags_flat_index_n < 0) return;
	for (int i = 0; i < 3; i++) {
		render->tags_flat_indices[render->tags_flat_index_n++] = indices[i];
	}
}

static void tagsctx_end_flat(struct render* render)
{
	if (render->tags_flat_index_n < 0) return;
	glBegin(GL_TRIANGLES);
	for (int i = 0; i < render->tags_flat_index_n; i++) {
		struct vec3* v = &render->tags_flat_vertices[render->tags_flat_indices[i]];
		glVertex3f(v->s[0], v->s[1], v->s[2]);
	}
	glEnd();
}

static void tagsctx_begin_wall(struct render* render, struct lvl* lvl, int sectori, int contouri, int dz)
{
	struct lvl_contour* c = lvl_get_contour(lvl, contouri);
	struct lvl_linedef* ld = lvl_get_linedef(lvl, c->linedef);
	struct lvl_sidedef* sd = lvl_get_sidedef(lvl, ld->sidedef[c->usr&1]);

	int hover = (dz <= 0 && (sd->usr & LVL_HIGHLIGHTED_ZMINUS)) || (dz >= 0 && (sd->usr & LVL_HIGHLIGHTED_ZPLUS));
	int selected = (dz <= 0 && (sd->usr & LVL_SELECTED_ZMINUS)) || (dz >= 0 && (sd->usr & LVL_SELECTED_ZPLUS));

	tagsctx_gl_color(hover, selected);
	glBegin(GL_QUADS);
}

static void tagsctx_add_wall_vertex(struct render* render, float x, float y, float z, float u, float v)
{
	glVertex3f(x,y,z);
}

static void tagsctx_end_wall(struct render* render)
{
	glEnd();
}

void render_lvl_tags(struct render* render, struct lvl* lvl)
{
	gl_transform(render);

	glBindFramebuffer(GL_FRAMEBUFFER, 0); CHKGL;
	gl_viewport_from_sdl_window(render->window);
	glClear(GL_DEPTH_BUFFER_BIT);

	glUseProgram(0);
	glActiveTexture(GL_TEXTURE0); CHKGL;
	glDisable(GL_TEXTURE_2D); CHKGL;

	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	flat_callbacks(
		render,
		tagsctx_begin_flat,
		tagsctx_add_flat_vertex,
		tagsctx_add_flat_triangle,
		tagsctx_end_flat
	);
	yield_flats(render, lvl);

	wall_callbacks(
		render,
		tagsctx_begin_wall,
		tagsctx_add_wall_vertex,
		tagsctx_end_wall
	);
	yield_walls(render, lvl);
}

