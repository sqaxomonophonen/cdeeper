#include <stdlib.h>
#include <string.h>
#include "names.h"
#include "a.h"

const char* names_flats[] = { "flat0", "flat1", "glowflat", NULL };

int names_number_of_flats()
{
	int i = 0;
	for (const char** flat = names_flats; *flat; flat++) i++;
	return i;
}

int names_find_flat(const char* name)
{
	int i = 0;
	for (const char** flat = names_flats; *flat; flat++) {
		if (strcmp(name, *flat) == 0) {
			return i;
		}
		i++;
	}
	arghf("flat '%s' not found\n", name);
}


const char* names_walls[] = { "wall0", "wall1", NULL };

int names_number_of_walls()
{
	int i = 0;
	for (const char** wall = names_walls; *wall; wall++) i++;
	return i;
}

int names_find_wall(const char* name)
{
	int i = 0;
	for (const char** wall = names_walls; *wall; wall++) {
		if (strcmp(name, *wall) == 0) {
			return i;
		}
		i++;
	}
	arghf("wall '%s' not found\n", name);
}


const char* names_sprites[] = { "sprite0", NULL };

int names_number_of_sprites()
{
	int i = 0;
	for (const char** sprite = names_sprites; *sprite; sprite++) i++;
	return i;
}

int names_find_sprite(const char* name)
{
	int i = 0;
	for (const char** sprite = names_sprites; *sprite; sprite++) {
		if (strcmp(name, *sprite) == 0) {
			return i;
		}
		i++;
	}
	arghf("sprite '%s' not found\n", name);
}





#define ENTDEF(group,type,radius) type,
const char* names_entity_types[] = {
	#include "entities.inc.h"
	NULL
};
#undef ENTDEF

int names_find_entity_type(const char* type)
{
	int i = 0;
	for (const char** t = names_entity_types; *t; t++) {
		if (strcmp(type, *t) == 0) {
			return i;
		}
		i++;
	}
	arghf("entity type '%s' not found\n", type);
}
