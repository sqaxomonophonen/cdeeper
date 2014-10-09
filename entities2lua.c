#include <stdio.h>

int main(int argc, char** argv)
{
	printf("return {\n");
	#define ENTDEF(group,type,radius) printf("\t{group = \"%s\", type = \"%s\", radius = %d},\n", #group, type, radius);
	#include "entities.inc.h"
	#undef ENTDEF
	printf("}\n");
	return 0;
}
