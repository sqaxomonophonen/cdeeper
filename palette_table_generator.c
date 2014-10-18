#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "magic.h"
#include "mud.h"
#include "m.h"
#include "a.h"

int main(int argc, char** argv)
{
	// XXX since we're loading textures... why not also verify that all
	// palettes are identical?

	if (argc != 2) {
		fprintf(stderr, "usage: %s <8-bit PNG>\n", argv[0]);
		return EXIT_FAILURE;
	}

	uint8_t palette[768];
	mud_load_png_palette(argv[1], palette);

	int unicorns = 1;

	for (int lvl = 0; lvl < MAGIC_NUM_LIGHT_LEVELS; lvl++) {
		for (int c0 = 0; c0 < 256; c0++) {
			struct vec3 target;
			vec3_set8(&target, palette[c0*3], palette[c0*3+1], palette[c0*3+2]);
			if (c0 < (256-16)) {
				float t = (float)lvl/(float)(MAGIC_NUM_LIGHT_LEVELS-1);
				float tt = powf(t, 1.4f);
				vec3_scalei(&target, tt);
			}
			if (unicorns) vec3_rgb2unicorns(&target);
			int best_match = -1;
			float best_distance = 0;
			for (int c1 = 0; c1 < 256; c1++) {
				struct vec3 col;
				vec3_set8(&col, palette[c1*3], palette[c1*3+1], palette[c1*3+2]);
				if (unicorns) vec3_rgb2unicorns(&col);

				struct vec3 d;
				vec3_sub(&d, &col, &target);
				float distance = vec3_dot(&d, &d);

				if (best_match == -1 || distance < best_distance) {
					best_match = c1;
					best_distance = distance;
				}
			}
			ASSERT(best_match != -1);

			int r = palette[best_match*3+0];
			int g = palette[best_match*3+1];
			int b = palette[best_match*3+2];
			fputc(r, stdout);
			fputc(g, stdout);
			fputc(b, stdout);
		}
	}
	return 0;
}

