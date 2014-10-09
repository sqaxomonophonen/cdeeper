#include <SDL.h>
#include <GL/glew.h>

#include "a.h"
#include "font.h"
#include "render.h"
#include "runtime.h"
#include "lvl.h"
#include "llvl.h"
#include "magic.h"

int main(int argc, char** argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s <plan>\n", argv[0]);
		return EXIT_FAILURE;
	}
	char* plan = argv[1];

	SAZ(SDL_Init(SDL_INIT_VIDEO));
	atexit(SDL_Quit);

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

	// get refresh rate
	SDL_DisplayMode mode;
	SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &mode);
	//printf("refresh rate: %dhz\n", mode.refresh_rate);
	float dt = 1.0f / (float)mode.refresh_rate;
	/* ^^^ XXX use refresh rate for dt per default to get a smoother
	 * experience? allow time() based alternative? */

	struct font font;
	font_init(&font);

	struct render render;
	render_init(&render, window);

	struct lvl lvl;
	lvl_init(&lvl);
	llvl_build(plan, &lvl);

	struct lvl_entity player;
	memset(&player, 0, sizeof(player));

	int exiting = 0;
	int ctrl_turn_left = 0;
	int ctrl_turn_right = 0;
	int ctrl_strafe_left = 0;
	int ctrl_strafe_right = 0;
	int ctrl_forward = 0;
	int ctrl_backward = 0;
	int overhead_mode = 0;

	while (!exiting) {
		SDL_Event e;

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
				if (e.key.keysym.sym == SDLK_q) ctrl_turn_left = 1;
				if (e.key.keysym.sym == SDLK_e) ctrl_turn_right = 1;
				if (e.key.keysym.sym == SDLK_a) ctrl_strafe_left = 1;
				if (e.key.keysym.sym == SDLK_d) ctrl_strafe_right = 1;
				if (e.key.keysym.sym == SDLK_w) ctrl_forward = 1;
				if (e.key.keysym.sym == SDLK_s) ctrl_backward = 1;

			}

			if (e.type == SDL_KEYUP) {
				if (e.key.keysym.sym == SDLK_q) ctrl_turn_left = 0;
				if (e.key.keysym.sym == SDLK_e) ctrl_turn_right = 0;
				if (e.key.keysym.sym == SDLK_a) ctrl_strafe_left = 0;
				if (e.key.keysym.sym == SDLK_d) ctrl_strafe_right = 0;
				if (e.key.keysym.sym == SDLK_w) ctrl_forward = 0;
				if (e.key.keysym.sym == SDLK_s) ctrl_backward = 0;
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

		if (overhead_mode) {
			glClearColor(0,0,0,0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); CHKGL;
			glUseProgram(0);
			glActiveTexture(GL_TEXTURE0); CHKGL;
			glDisable(GL_TEXTURE_2D); CHKGL;
			float overhead_scale = 0.125f;
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(-MAGIC_RWIDTH/2, MAGIC_RWIDTH/2, MAGIC_RHEIGHT/2, -MAGIC_RHEIGHT/2, 1, 0);

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
		} else {
			render_set_entity_cam(&render, &player);
			render_lvl_geom(&render, &lvl);

			render_begin2d(&render);
			font_begin(&font, 6);
			font_goto(&font, 6, 6);
			font_color(&font, 3);
			font_printf(&font, "score: %d", -1);
			font_end(&font);
			render_flip(&render);
		}

		SDL_GL_SwapWindow(window);
	}

	SDL_DestroyWindow(window);
	SDL_GL_DeleteContext(glctx);

	return 0;
}
