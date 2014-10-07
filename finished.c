#include <SDL.h>
#include <GL/glew.h>

#include "a.h"
#include "llvl.h"
#include "m.h"
#include "render.h"
#include "font.h"
#include "names.h"

static void glew_init()
{
	GLenum err = glewInit();
	if (err != GLEW_OK) {
		arghf("glewInit() failed: %s", glewGetErrorString(err));
	}

	#define CHECK_GL_EXT(x) { if(!GLEW_ ## x) arghf("OpenGL extension not found: " #x); }
	CHECK_GL_EXT(ARB_shader_objects)
	CHECK_GL_EXT(ARB_vertex_shader)
	CHECK_GL_EXT(ARB_fragment_shader)
	CHECK_GL_EXT(ARB_framebuffer_object)
	CHECK_GL_EXT(ARB_vertex_buffer_object)
	#undef CHECK_GL_EXT

	/* to figure out what extension something belongs to, see:
	 * http://www.opengl.org/registry/#specfiles */

	// XXX check that version is at least 1.30?
	// printf("GLSL version %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
}

/*
static float frand(float min, float max)
{
	float s = (float)rand() / (float)RAND_MAX;
	return min + s * (max - min);
}
*/


#include "magic.h"
int main(int argc, char** argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s <brickname>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char* brickname = argv[1];

	SAZ(SDL_Init(SDL_INIT_VIDEO));
	atexit(SDL_Quit);

	//int bitmask = SDL_WINDOW_OPENGL;
	//int bitmask = SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL;
	int bitmask = SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_OPENGL;

	SDL_Window* window = SDL_CreateWindow(
		"deeper",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		0, 0,
		bitmask);
	SAN(window);

	SDL_GLContext glctx = SDL_GL_CreateContext(window);
	SAN(glctx);

	SAZ(SDL_GL_SetSwapInterval(1)); // or -1, "late swap tearing"?

	glew_init();

	// get display width/height
	int width = 0;
	int height = 0;
	SDL_GetWindowSize(window, &width, &height);


	// get refresh rate
	SDL_DisplayMode mode;
	SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &mode);
	printf("refresh rate: %dhz\n", mode.refresh_rate);
	float dt = 1.0f / (float)mode.refresh_rate;
	/* ^^^ XXX use refresh rate for dt per default to get a smoother
	 * experience? allow time() based alternative? */


	struct font font;
	font_init(&font);

	struct render render;
	render_init(&render, window);

	glEnable(GL_DEPTH_TEST); CHKGL;
	glEnable(GL_CULL_FACE); CHKGL;
	glEnable(GL_BLEND); CHKGL;
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); CHKGL;
	glDepthFunc(GL_LEQUAL);

	struct lvl lvl;
	lvl_init(&lvl);

	llvl_load(brickname, &lvl);



	struct lvl_entity player;
	memset(&player, 0, sizeof(player));

	int exiting = 0;
	//int go = 0;
	//float a = 0;


	int ctrl_turn_left = 0;
	int ctrl_turn_right = 0;
	int ctrl_strafe_left = 0;
	int ctrl_strafe_right = 0;
	int ctrl_forward = 0;
	int ctrl_backward = 0;

	int ctrl_zoom_in = 0;
	int ctrl_zoom_out = 0;

	int overhead_mode = 0;
	float overhead_scale = 0.2f;

	int mouse_x = 0;
	int mouse_y = 0;

	int frame = 1;
	int tooling = 0;

	enum {
		ED_NONE = 1,
		ED_FLAT_Z,
		ED_FLAT_TEXTURE,
		ED_FLAT_TEXTURE_TRANSLATE,
		ED_SIDEDEF_TEXTURE,
		ED_LIGHT_LEVEL
	//} ed = ED_FLAT_Z;
	} ed = ED_NONE;


	while (!exiting) {
		SDL_Event e;
		int do_select = 0;
		float tool_dx = 0;
		float tool_dy = 0;

		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) {
				exiting = 1;
			}

			if (e.type == SDL_KEYDOWN) {
				if (e.key.keysym.sym == SDLK_ESCAPE) {
					exiting = 1;
				}
				if (e.key.keysym.sym == SDLK_TAB) {
					overhead_mode = !overhead_mode;
				}
				//if (e.key.keysym.sym == SDLK_SPACE) {
					//go = !go;
				//}
				if (e.key.keysym.sym == SDLK_q) ctrl_turn_left = 1;
				if (e.key.keysym.sym == SDLK_e) ctrl_turn_right = 1;
				if (e.key.keysym.sym == SDLK_a) ctrl_strafe_left = 1;
				if (e.key.keysym.sym == SDLK_d) ctrl_strafe_right = 1;
				if (e.key.keysym.sym == SDLK_w) ctrl_forward = 1;
				if (e.key.keysym.sym == SDLK_s) ctrl_backward = 1;

				if (e.key.keysym.sym == SDLK_KP_PLUS) ctrl_zoom_in = 1;
				if (e.key.keysym.sym == SDLK_KP_MINUS) ctrl_zoom_out = 1;

				switch (e.key.keysym.sym) {
					case SDLK_BACKQUOTE:
						ed = ED_NONE;
						break;
					case SDLK_1:
						ed = ED_LIGHT_LEVEL;
						break;
					case SDLK_2:
						ed = ED_FLAT_Z;
						break;
					case SDLK_3:
						ed = ED_FLAT_TEXTURE;
						break;
					case SDLK_4:
						ed = ED_FLAT_TEXTURE_TRANSLATE;
						break;
					case SDLK_5:
						ed = ED_SIDEDEF_TEXTURE;
						break;
				}
			}

			if (e.type == SDL_KEYUP) {
				if (e.key.keysym.sym == SDLK_q) ctrl_turn_left = 0;
				if (e.key.keysym.sym == SDLK_e) ctrl_turn_right = 0;
				if (e.key.keysym.sym == SDLK_a) ctrl_strafe_left = 0;
				if (e.key.keysym.sym == SDLK_d) ctrl_strafe_right = 0;
				if (e.key.keysym.sym == SDLK_w) ctrl_forward = 0;
				if (e.key.keysym.sym == SDLK_s) ctrl_backward = 0;

				if (e.key.keysym.sym == SDLK_KP_PLUS) ctrl_zoom_in = 0;
				if (e.key.keysym.sym == SDLK_KP_MINUS) ctrl_zoom_out = 0;
			}

			if (e.type == SDL_MOUSEMOTION) {
				mouse_x = e.motion.x;
				mouse_y = e.motion.y;
				if (tooling) {
					float scale = 0.1f;
					tool_dx -= (float)e.motion.xrel * scale;
					tool_dy -= (float)e.motion.yrel * scale;
				}

			}

			if (e.type == SDL_MOUSEBUTTONDOWN) {
				int button = e.button.button;
				if (button == 1) {
					tooling = 1;
				}
				if (button == 3) {
					do_select = 1;
				}
			}

			if (e.type == SDL_MOUSEBUTTONUP) {
				int button = e.button.button;
				if (button == 1) {
					tooling = 0;
				}
			}

			if (e.type == SDL_MOUSEWHEEL) {
				tool_dy += e.wheel.y;
			}
		}

		if (overhead_mode) {
			float speed = 1.05f;
			if (ctrl_zoom_in) {
				overhead_scale *= speed;
			}
			if (ctrl_zoom_out) {
				overhead_scale /= speed;
			}
		}

		if (ctrl_turn_left || ctrl_turn_right) {
			float turn_speed = 200;
			float sgn = 0;
			if (ctrl_turn_left) sgn -= 1.0f;
			if (ctrl_turn_right) sgn += 1.0f;
			player.yaw += dt * turn_speed * sgn;
		}

		struct vec2 move;
		move.s[0] = 0;
		move.s[1] = 0;

		struct vec2 forward;
		vec2_angle(&forward, player.yaw);
		struct vec2 right;
		vec2_normal(&right, &forward);

		if (ctrl_forward || ctrl_backward) {
			float move_speed = 12;
			float sgn = 0;
			if (ctrl_forward) sgn -= 1.0f;
			if (ctrl_backward) sgn += 1.0f;
			struct vec2 dmove;
			vec2_copy(&dmove, &forward);
			vec2_scalei(&dmove, move_speed * sgn);
			vec2_addi(&move, &dmove);
		}

		if (ctrl_strafe_left || ctrl_strafe_right) {
			float strafe_speed = 7;
			float sgn = 0;
			if (ctrl_strafe_left) sgn -= 1.0f;
			if (ctrl_strafe_right) sgn += 1.0f;
			struct vec2 dmove;
			vec2_copy(&dmove, &right);
			vec2_scalei(&dmove, strafe_speed * sgn);
			vec2_addi(&move, &dmove);
		}


		lvl_entity_clipmove(&lvl, &player, &move);


		if (tool_dx != 0 || tool_dy != 0) {
			if (ed == ED_FLAT_Z) {
				for (int i = 0; i < lvl.n_sectors; i++) {
					struct lvl_sector* sector = lvl_get_sector(&lvl, i);
					float d = tool_dy * 8;
					if (sector->usr & LVL_SELECTED_ZMINUS) {
						sector->flat[0].plane.s[3] -= d;
					}
					if (sector->usr & LVL_SELECTED_ZPLUS) {
						sector->flat[1].plane.s[3] += d;
					}
				}
			}
			if (ed == ED_FLAT_TEXTURE || ed == ED_FLAT_TEXTURE_TRANSLATE) {
				for (int i = 0; i < lvl.n_sectors; i++) {
					struct lvl_sector* sector = lvl_get_sector(&lvl, i);

					for (int flati = 0; flati < 2; flati++) {
						if (sector->usr & (flati == 0 ? LVL_SELECTED_ZMINUS : LVL_SELECTED_ZPLUS)) {
							struct lvl_flat* flat = &sector->flat[flati];
							if (ed == ED_FLAT_TEXTURE) {
								flat->texture = clampi(flat->texture + (int)tool_dy, 0, names_number_of_flats()-1);
							}
							if (ed == ED_FLAT_TEXTURE_TRANSLATE) {
								struct vec2 d;
								vec2_zero(&d);
								vec2_add_scalei(&d, &forward, tool_dy);
								vec2_add_scalei(&d, &right, tool_dx);
								vec2_scalei(&d, 2.5f);
								flat->tx.s[4] += d.s[0];
								flat->tx.s[5] += d.s[1];
							}
						}
					}
				}
			}
			if (ed == ED_SIDEDEF_TEXTURE) {
				for (int i = 0; i < lvl.n_sectors; i++) {
					struct lvl_sector* sector = lvl_get_sector(&lvl, i);

					for (int j = 0; j < sector->contourn; j++) {
						int32_t ci = sector->contour0 + j;
						struct lvl_contour* c = lvl_get_contour(&lvl, ci);
						struct lvl_linedef* ld = lvl_get_linedef(&lvl, c->linedef);
						uint32_t sdi = ld->sidedef[c->usr&1];
						ASSERT(sdi != -1);
						struct lvl_sidedef* sd = lvl_get_sidedef(&lvl, sdi);
						if (sd->usr & LVL_SELECTED_ZMINUS) {
							sd->texture[0] = clampi(sd->texture[0] + (int)tool_dy, 0, names_number_of_walls()-1);
						}
						if (sd->usr & LVL_SELECTED_ZPLUS) {
							sd->texture[1] = clampi(sd->texture[1] + (int)tool_dy, 0, names_number_of_walls()-1);
						}
					}
				}
			}
			if (ed == ED_LIGHT_LEVEL) {
				for (int i = 0; i < lvl.n_sectors; i++) {
					struct lvl_sector* sector = lvl_get_sector(&lvl, i);
					float d = tool_dy * 0.0125f;
					if (sector->usr & LVL_SELECTED_ZMINUS) {
						sector->light_level += d;
						if (sector->light_level < 0) sector->light_level = 0;
						if (sector->light_level > 1) sector->light_level = 1;
					}
				}
			}
		}

		if (overhead_mode) {
			// XXX TODO FIXME port/move this to render.c
			#if 0
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(-WIDTH/2, WIDTH/2, HEIGHT/2, -HEIGHT/2, 1, 0);

			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();

			//glTranslatef(0,0,-400);
			glScalef(overhead_scale, overhead_scale, overhead_scale);

			glColor4f(1,0,0,1);
			glBegin(GL_LINE_LOOP);
			for (int i = 0; i < 32; i++) {
				float r = lvl_entity_radius(&player);
				float phi = (float)i / 32.0f * 6.2830f;
				float x = cosf(phi) * r;
				float y = sinf(phi) * r;
				glVertex2f(x, y);
			}
			glEnd();

			glRotatef(-player.yaw, 0, 0, 1);
			glTranslatef(-player.p.s[0],-player.p.s[1],0);

			glBegin(GL_LINES);
			for (int i = 0; i < lvl.n_linedefs; i++) {
				struct lvl_linedef* l = lvl_get_linedef(&lvl, i);
				if (l->sidedef[0] == -1 || l->sidedef[1] == -1) {
					glColor4f(1,1,1,1);
				} else {
					glColor4f(0,0.6,0.6,1);
				}
				struct vec2* v0 = lvl_get_vertex(&lvl, l->vertex[0]);
				glVertex2f(v0->s[0], v0->s[1]);
				struct vec2* v1 = lvl_get_vertex(&lvl, l->vertex[1]);
				glVertex2f(v1->s[0], v1->s[1]);
			}
			glEnd();
			#endif
		} else {
			struct vec3 pos;
			struct vec3 mdir;
			lvl_entity_mouse(&player, &pos, &mdir, render_get_fovy(&render), mouse_x, mouse_y, width, height);
			struct lvl_trace_result trace_result;
			//int sectori = lvl_sector_find(&lvl, &player.p);
			//int sectori = player.sector;
			lvl_trace(&lvl, player.sector, &pos, &mdir, &trace_result);

			lvl_tag_clear_highlights(&lvl);

			if (ed == ED_FLAT_Z || ed == ED_FLAT_TEXTURE || ed == ED_FLAT_TEXTURE_TRANSLATE) {
				lvl_tag_flats(&lvl, &trace_result, do_select);
			}
			if (ed == ED_SIDEDEF_TEXTURE) {
				lvl_tag_sidedefs(&lvl, &trace_result, do_select);
			}
			if (ed == ED_LIGHT_LEVEL) {
				lvl_tag_sectors(&lvl, &trace_result, do_select);
			}

			render_set_entity_cam(&render, &player);
			render_lvl_geom(&render, &lvl);

			render_begin2d(&render);
			font_begin(&font, 6);
			font_goto(&font, 6, 6);
			font_color(&font, 3);
			font_printf(&font,
				"mode: %s",
				ed == ED_NONE ? "none" :
				ed == ED_FLAT_Z ? "flat z" :
				ed == ED_FLAT_TEXTURE ? "flat texture" :
				ed == ED_FLAT_TEXTURE_TRANSLATE ? "flat texture translate" :
				ed == ED_SIDEDEF_TEXTURE ? "sidedef texture" :
				ed == ED_LIGHT_LEVEL ? "light level" :
				"???"
			);
			frame++;
			font_end(&font);

			render_flip(&render);
			render_lvl_tags(&render, &lvl);
		}

		SDL_GL_SwapWindow(window);
	}

	SDL_DestroyWindow(window);
	SDL_GL_DeleteContext(glctx);

	llvl_save(brickname, &lvl);

	return 0;
}

