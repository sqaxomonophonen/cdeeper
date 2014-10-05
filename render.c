#include "magic.h"
#include "render.h"
#include "mud.h"
#include "a.h"
#include "mem.h"

#include <tesselator.h>
#include <GL/glew.h>


#define FLOATS_PER_FLAT_VERTEX (7)
#define FLOATS_PER_WALL_VERTEX (5)

// XXX TODO should roll my own matrix stack. gl_ModelViewProjectionMatrix,
// glLoadIdentity() and so on are all deprecated

// flat shader source
static const char* flat_shader_vertex_src =
	"#version 130\n"
	"\n"
	"attribute vec3 a_pos;\n"
	"attribute vec2 a_uv;\n"
	"attribute vec2 a_selector;\n"
	"\n"
	"varying vec2 v_uv;\n"
	"varying vec2 v_selector;\n"
	"\n"
	"void main()\n"
	"{\n"
	"	v_uv = a_uv;\n"
	"	v_selector = a_selector;\n"
	"	gl_Position = gl_ModelViewProjectionMatrix * vec4(a_pos, 1);\n"
	"}\n";

static const char* flat_shader_fragment_src =
	"#version 130\n"
	"\n"
	"varying vec2 v_uv;\n"
	"varying vec2 v_selector;\n"
	"\n"
	"uniform sampler2D u_flatlas;\n"
	"\n"
	"void main(void)\n"
	"{\n"
	"	float flat_size = " STR_VALUE(MAGIC_FLAT_SIZE) ";\n"
	"	float atlas_size = " STR_VALUE(MAGIC_FLAT_ATLAS_SIZE) ";\n"
	"	vec2 uv = (clamp(fract(v_uv / flat_size) * flat_size, 0.5, flat_size - 0.5) + v_selector) / atlas_size;\n"
	"	float index = texture2D(u_flatlas, uv).r;\n"
	//"	gl_FragColor = vec4(index, 0, 0, 1);\n" // TODO green is light value
	"	gl_FragColor = vec4(index, 0.5, 0, 1);\n"
	"}\n";


// wall shader source
static const char* wall_shader_vertex_src =
	"#version 130\n"
	"\n"
	"attribute vec3 a_pos;\n"
	"attribute vec2 a_uv;\n"
	"\n"
	"varying vec2 v_uv;\n"
	"\n"
	"void main()\n"
	"{\n"
	"	v_uv = a_uv;\n"
	"	gl_Position = gl_ModelViewProjectionMatrix * vec4(a_pos, 1);\n"
	"}\n";

static const char* wall_shader_fragment_src =
	"#version 130\n"
	"\n"
	"varying vec2 v_uv;\n"
	"\n"
	"uniform sampler2D u_texture;\n"
	"\n"
	"void main(void)\n"
	"{\n"
	"	vec2 nuv = vec2(v_uv.x/256, v_uv.y/128);\n" // XXX texture dimension assumption.. probably move it to the "client"
	"	float index = texture2D(u_texture, nuv).r;\n"
	//"	gl_FragColor = vec4(index, 0, 0, 1);\n" // TODO green is light value
	"	gl_FragColor = vec4(index, 0.7, 0, 1);\n"
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
	"		brite += 1.0f/32.0f;\n" // half a brightness level
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

static const char* flats[] = { "flat0", NULL };

static void render_init_flats(struct render* render)
{
	static char path[1024];

	size_t atlas_sz = MAGIC_FLAT_ATLAS_SIZE * MAGIC_FLAT_ATLAS_SIZE;
	uint8_t* atlas = malloc(atlas_sz);
	AN(atlas);

	int flats_per_row_exp = MAGIC_FLAT_ATLAS_SIZE_EXP - MAGIC_FLAT_SIZE_EXP;

	int slot = 0;
	for (const char** flat = flats; *flat; flat++) {
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

static void render_init_walls(struct render* render)
{
	uint8_t* data;
	int width = 0;
	int height = 0;
	AZ(mud_load_png_paletted("gfx/wall0.png", &data, &width, &height));

	int level = 0;
	int border = 0;

	glGenTextures(1, &render->wall0_texture); CHKGL;
	glBindTexture(GL_TEXTURE_2D, render->wall0_texture); CHKGL;
	glTexImage2D(GL_TEXTURE_2D, level, 1, width, height, border, GL_RED, GL_UNSIGNED_BYTE, data); CHKGL;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); CHKGL;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); CHKGL;
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
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height); CHKGL;
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

	#if 1
	GLuint dbuf;
	glGenRenderbuffers(1, &dbuf); CHKGL;
	glBindRenderbuffer(GL_RENDERBUFFER, dbuf); CHKGL;
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height); CHKGL;
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, dbuf); CHKGL;
	#endif

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
	glUniform1i(glGetUniformLocation(render->flat_shader.program, "u_flatlas"), 0); CHKGL;

	// wall shader
	shader_init(&render->wall_shader, wall_shader_vertex_src, wall_shader_fragment_src);
	shader_use(&render->wall_shader);
	render->wall_a_pos = glGetAttribLocation(render->wall_shader.program, "a_pos"); CHKGL;
	render->wall_a_uv = glGetAttribLocation(render->wall_shader.program, "a_uv"); CHKGL;
	glUniform1i(glGetUniformLocation(render->wall_shader.program, "u_texture"), 0); CHKGL;

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

	glGenBuffers(1, &render->wall_vertex_buffer); CHKGL;
	glBindBuffer(GL_ARRAY_BUFFER, render->wall_vertex_buffer); CHKGL;
	size_t wall_vertex_data_sz = RENDER_BUFSZ * sizeof(float) * FLOATS_PER_WALL_VERTEX;
	render->wall_vertex_data = malloc(wall_vertex_data_sz);
	AN(render->wall_vertex_data);
	glBufferData(GL_ARRAY_BUFFER, wall_vertex_data_sz, render->wall_vertex_data, GL_STREAM_DRAW); CHKGL;

	glGenBuffers(1, &render->wall_index_buffer); CHKGL;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render->wall_index_buffer); CHKGL;
	size_t wall_index_data_sz = RENDER_BUFSZ * sizeof(uint32_t);
	render->wall_index_data = malloc(wall_index_data_sz);
	AN(render->wall_index_data);
	int offset = 0;
	for (int i = 0; i < (RENDER_BUFSZ-6); i += 6) {
		render->wall_index_data[i+0] = 0 + offset;
		render->wall_index_data[i+1] = 1 + offset;
		render->wall_index_data[i+2] = 2 + offset;
		render->wall_index_data[i+3] = 0 + offset;
		render->wall_index_data[i+4] = 2 + offset;
		render->wall_index_data[i+5] = 3 + offset;
		offset += 4;
	}
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, wall_index_data_sz, render->wall_index_data, GL_STATIC_DRAW); CHKGL;

	static_quad_buffers(&render->step_vertex_buffer, &render->step_index_buffer, MAGIC_RWIDTH, MAGIC_RHEIGHT);
}

void render_init(struct render* render, SDL_Window* window)
{
	render->window = window;

	render_init_palette_table(render);
	render_init_flats(render);
	render_init_walls(render);
	render_init_framebuffers(render);
	render_init_shaders(render);
	render_init_buffers(render);
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


static void add_flat_vertex(struct render* render, float x, float y, float z, float u, float v, float select_u, float select_v)
{
	ASSERT(render->flat_vertex_n < RENDER_BUFSZ);
	float* data = &render->flat_vertex_data[render->flat_vertex_n * FLOATS_PER_FLAT_VERTEX];
	data[0] = x;
	data[1] = y;
	data[2] = z;
	data[3] = u;
	data[4] = v;
	data[5] = select_u;
	data[6] = select_v;
	//printf("%f %f %f\n", x, y, z);
	render->flat_vertex_n++;
}

static void add_flat_index(struct render* render, uint32_t index)
{
	ASSERT(render->flat_index_n < RENDER_BUFSZ);
	//printf("index: %d\n", index);
	render->flat_index_data[render->flat_index_n++] = index;
}

static void render_flat_partial(struct render* render, struct lvl* lvl, int sectori, int flati)
{
	struct lvl_sector* sector = lvl_get_sector(lvl, sectori);

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

	int offset = render->flat_vertex_n;

	int nverts = tessGetVertexCount(t);
	const float* verts = tessGetVertices(t);
	for (int i = 0; i < nverts; i++) {
		struct plane* plane = &sector->flat[flati].plane;
		struct vec2 v;
		v.s[0] = verts[i*2];
		v.s[1] = verts[i*2+1];
		float z = plane_z(plane, &v);
		add_flat_vertex(
			render,
			v.s[0], z, v.s[1], // XYZ
			v.s[0], v.s[1], // UV
			0, 0 // selector (XXX TODO FIXME get from texture)
		);
	}

	int nelems = tessGetElementCount(t);
	const int* elems = tessGetElements(t);

	for (int i = 0; i < nelems; i++) {
		const int* p = &elems[i * nvp];
		for (int j = 0; j < nvp; j++) {
			ASSERT(p[j] != TESS_UNDEF);
			int k = flati ? nvp - 1 - j : j;
			add_flat_index(render, offset + p[k]);
		}
	}

	tessDeleteTess(t);
}

static void render_flats(struct render* render, struct lvl* lvl)
{
	render->flat_vertex_n = 0;
	render->flat_index_n = 0;

	for (int i = 0; i < lvl->n_sectors; i++) {
		render_flat_partial(render, lvl, i, 0);
		render_flat_partial(render, lvl, i, 1);
	}

	AZ(render->flat_index_n % 3); // threeangles!

	#if 0
	for (int i = 0; i < (render->flat_index_n/3); i++) {
		glBegin(GL_LINE_LOOP);
		//glColor4f(i&1,i&2,i&4,1);
		glColor4f(1,1,1,1);
		for (int j = 0; j < 3; j++) {
			uint32_t k =  render->flat_index_data[i*3+j];
			float x = render->flat_vertex_data[k*FLOATS_PER_FLAT_VERTEX];
			float y = render->flat_vertex_data[k*FLOATS_PER_FLAT_VERTEX+1];
			float z = render->flat_vertex_data[k*FLOATS_PER_FLAT_VERTEX+2];
			glVertex3f(x, y, z);
		}
		glEnd();
	}
	#endif

	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

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

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render->flat_index_buffer); CHKGL;
	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, render->flat_index_n * sizeof(uint32_t), render->flat_index_data); CHKGL;

	glDrawElements(GL_TRIANGLES, render->flat_index_n, GL_UNSIGNED_INT, NULL); CHKGL;

	glDisableVertexAttribArray(render->flat_a_selector); CHKGL;
	glDisableVertexAttribArray(render->flat_a_uv); CHKGL;
	glDisableVertexAttribArray(render->flat_a_pos); CHKGL;

	glActiveTexture(GL_TEXTURE0); CHKGL;
	glDisable(GL_TEXTURE_2D); CHKGL;
}

static void add_wall_vertex(struct render* render, float x, float y, float z, float u, float v)
{
	ASSERT(render->wall_vertex_n < RENDER_BUFSZ);
	float* data = &render->wall_vertex_data[render->wall_vertex_n * FLOATS_PER_WALL_VERTEX];
	data[0] = x;
	data[1] = y;
	data[2] = z;
	data[3] = u;
	data[4] = v;
	render->wall_vertex_n++;
}

static void render_walls(struct render* render, struct lvl* lvl)
{
	render->wall_vertex_n = 0;

	for (int i = 0; i < lvl->n_sectors; i++) {
		struct lvl_sector* sector = lvl_get_sector(lvl, i);

		for (int i = 0; i < sector->contourn; i++) {
			int ci = sector->contour0 + i;

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

			if (sopp == NULL) {
				// one-sided
				float z0 = plane_z(&sector->flat[1].plane, v0);
				float z1 = plane_z(&sector->flat[1].plane, v1);
				float z2 = plane_z(&sector->flat[0].plane, v1);
				float z3 = plane_z(&sector->flat[0].plane, v0);

				float tu0 = 0;
				float tv0 = z0;
				float tu1 = vd_length;
				float tv1 = z2;

				add_wall_vertex(render, v0->s[0], z0, v0->s[1], tu0, tv0);
				add_wall_vertex(render, v1->s[0], z1, v1->s[1], tu1, tv0);
				add_wall_vertex(render, v1->s[0], z2, v1->s[1], tu1, tv1);
				add_wall_vertex(render, v0->s[0], z3, v0->s[1], tu0, tv1);

			} else {
				// two-sided
				struct lvl_sector* sector1 = lvl_get_sector(lvl, sopp->sector);
				{
					float z0 = plane_z(&sector1->flat[0].plane, v0);
					float z1 = plane_z(&sector1->flat[0].plane, v1);
					float z2 = plane_z(&sector->flat[0].plane, v1);
					float z3 = plane_z(&sector->flat[0].plane, v0);

					float tu0 = 0;
					float tv0 = z0;
					float tu1 = vd_length;
					float tv1 = z2;

					if (z1 > z2 && z0 > z3) {
						add_wall_vertex(render, v0->s[0], z0, v0->s[1], tu0, tv0);
						add_wall_vertex(render, v1->s[0], z1, v1->s[1], tu1, tv0);
						add_wall_vertex(render, v1->s[0], z2, v1->s[1], tu1, tv1);
						add_wall_vertex(render, v0->s[0], z3, v0->s[1], tu0, tv1);
					} // XXX else: crossing?
				}
				{
					float z0 = plane_z(&sector1->flat[1].plane, v0);
					float z1 = plane_z(&sector1->flat[1].plane, v1);
					float z2 = plane_z(&sector->flat[1].plane, v1);
					float z3 = plane_z(&sector->flat[1].plane, v0);

					float tu0 = 0;
					float tv0 = z0;
					float tu1 = vd_length;
					float tv1 = z2;

					if (z1 < z2 && z0 < z3) {
						add_wall_vertex(render, v0->s[0], z3, v0->s[1], tu0, tv0);
						add_wall_vertex(render, v1->s[0], z2, v1->s[1], tu1, tv0);
						add_wall_vertex(render, v1->s[0], z1, v1->s[1], tu1, tv1);
						add_wall_vertex(render, v0->s[0], z0, v0->s[1], tu0, tv1);
					} // XXX else: crossing?
				}
			}
		}
	}


	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	shader_use(&render->wall_shader);

	glActiveTexture(GL_TEXTURE0); CHKGL;
	glEnable(GL_TEXTURE_2D); CHKGL;
	glBindTexture(GL_TEXTURE_2D, render->wall0_texture); CHKGL;

	glBindBuffer(GL_ARRAY_BUFFER, render->wall_vertex_buffer); CHKGL;
	glBufferSubData(GL_ARRAY_BUFFER, 0, render->wall_vertex_n * sizeof(float) * FLOATS_PER_WALL_VERTEX, render->wall_vertex_data); CHKGL;

	glEnableVertexAttribArray(render->wall_a_pos); CHKGL;
	glVertexAttribPointer(render->wall_a_pos, 3, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_WALL_VERTEX, 0); CHKGL;

	glEnableVertexAttribArray(render->wall_a_uv); CHKGL;
	glVertexAttribPointer(render->wall_a_uv, 2, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_WALL_VERTEX, (char*)(sizeof(float)*3)); CHKGL;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render->wall_index_buffer); CHKGL;
	glDrawElements(GL_TRIANGLES, render->wall_vertex_n/4*6, GL_UNSIGNED_INT, NULL); CHKGL;

	glDisableVertexAttribArray(render->wall_a_uv); CHKGL;
	glDisableVertexAttribArray(render->wall_a_pos); CHKGL;

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

static void render_blit(struct render* render)
{
	glUseProgram(0); CHKGL;
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST); CHKGL;
	glDisable(GL_CULL_FACE); CHKGL;

	glBindFramebuffer(GL_FRAMEBUFFER, 0); CHKGL;
	int w, h;
	SDL_GetWindowSize(render->window, &w, &h);
	glViewport(0, 0, w, h); CHKGL;

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

void render_lvl(struct render* render, struct lvl* lvl)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	float fovy = render_get_fovy(render);
	float aspect = (float)MAGIC_RWIDTH/(float)MAGIC_RHEIGHT;
	gluPerspective(fovy, aspect, 0.1, 4096);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glRotatef(render->entity_cam->yaw, 0, 1, 0);
	glTranslatef(-render->entity_cam->p.s[0], -render->entity_cam->z, -render->entity_cam->p.s[1]);

	glBindFramebuffer(GL_FRAMEBUFFER, render->screen_framebuffer); CHKGL;
	glViewport(0, 0, MAGIC_RWIDTH, MAGIC_RHEIGHT);
	glClearColor(0,0,0,0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	render_flats(render, lvl);
	render_walls(render, lvl);

	render_step(render);
	render_blit(render);

	glUseProgram(0); CHKGL;
	glBindFramebuffer(GL_FRAMEBUFFER, 0); CHKGL;
}

