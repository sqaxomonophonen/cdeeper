#include <GL/glew.h>

#include "a.h"

void glew_init()
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

