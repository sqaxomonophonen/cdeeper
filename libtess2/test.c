#include <stdio.h>
#include <stdlib.h>

#include "tesselator.h"

static void* my_malloc(void* usr, unsigned int sz)
{
	printf("my_malloc(%u)\n", sz);
	return malloc(sz);
}

static void* my_realloc(void* usr, void* ptr, unsigned int sz)
{
	printf("my_realloc(%u)\n", sz);
	return realloc(ptr, sz);
}

static void my_free(void* usr, void* ptr)
{
	printf("my_free()\n");
	return free(ptr);
}

int main(int argc, char** argv)
{
	TESSalloc ta;
	ta.memalloc = my_malloc;
	ta.memrealloc = my_realloc;
	ta.memfree = my_free;
	ta.userData = NULL;
	ta.meshEdgeBucketSize = 512;
	ta.meshVertexBucketSize = 512;
	ta.meshFaceBucketSize = 256;
	ta.dictNodeBucketSize = 512;
	ta.regionBucketSize = 256;
	ta.extraVertices = 0;

	TESStesselator* t = tessNewTess(&ta);

	int N = 100;

	float* fdata = malloc(sizeof(float) * 2 * N);
	for (int i = 0; i < N; i++) {
	}

	printf("------------------ (1)\n");

	tessAddContour(t, 2,



	tessDeleteTess(t);

	return 0;
}
