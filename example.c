#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "objzero.h"

static void printModel(const objzModel *_model) {
#if 0
	for (uint32_t i = 0; i < _model->numMaterials; i++) {
		objzMaterial *mat = &_model->materials[i];
		printf("Material %s\n", mat->name);
		printf("   d: %g\n", mat->d);
		printf("   illum: %d\n", mat->illum);
		printf("   Ka: %g %g %g\n", mat->Ka[0], mat->Ka[1], mat->Ka[2]);
		printf("   Kd: %g %g %g\n", mat->Kd[0], mat->Kd[1], mat->Kd[2]);
		printf("   Ke: %g %g %g\n", mat->Ke[0], mat->Ke[1], mat->Ke[2]);
		printf("   Ks: %g %g %g\n", mat->Ks[0], mat->Ks[1], mat->Ks[2]);
		printf("   Ni: %g\n", mat->Ni);
		printf("   Ns: %g\n", mat->Ns);
		printf("   map_Bump: %s\n", mat->map_Bump);
		printf("   map_Ka: %s\n", mat->map_Ka);
		printf("   map_Kd: %s\n", mat->map_Kd);
	}
#endif
#if 0
	for (uint32_t i = 0; i < _model->numObjects; i++) {
		objzObject *object = &_model->objects[i];
		printf("Object: %s\n", object->name);
		printf("   %d indices\n", object->numIndices);
		printf("   %d vertices\n", object->numVertices);
		printf("   %d meshes\n", object->numMeshes);
		for (uint32_t j = 0; j < object->numMeshes; j++) {
			objzMesh *mesh = &_model->meshes[object->firstMesh + j];
			printf("      %d: '%s' material, %d indices\n", j, mesh->materialIndex < 0 ? "<empty>" : _model->materials[mesh->materialIndex].name, mesh->numIndices);
		}
	}
#endif
	printf("%u objects\n", _model->numObjects);
	printf("%u materials\n", _model->numMaterials);
	printf("%u meshes\n", _model->numMeshes);
	printf("%u vertices\n", _model->numVertices);
	printf("%u triangles\n", _model->numIndices / 3);
}

static size_t s_totalBytesUsed = 0;
static size_t s_peakBytesUsed = 0;

static void *custom_realloc(void *_ptr, size_t _size) {
	uint32_t *realPtr = _ptr ? ((uint32_t *)_ptr) - 1 : NULL;
	if (!_size) {
		s_totalBytesUsed -= *realPtr;
		return realloc(realPtr, 0);
	}
	_size += 4;
	if (realPtr)
		s_totalBytesUsed -= *realPtr;
	uint32_t *newPtr = realloc(realPtr, _size);
	*newPtr = _size;
	s_totalBytesUsed += _size;
	if (s_totalBytesUsed > s_peakBytesUsed)
		s_peakBytesUsed = s_totalBytesUsed;
	return newPtr + 1;
}

int main(int argc, char **argv) {
	if (argc <= 1)
		return 0;
	printf("Loading '%s'\n", argv[1]);
	objz_setRealloc(custom_realloc);
	clock_t start = clock();
	objzModel *model = objz_load(argv[1]);
	clock_t end = clock();
	if (!model) {
		printf("ERROR: %s\n", objz_getError());
		return 1;
	}
	printf("%g milliseconds elapsed\n", (end - start) * 1000.0 / (double)CLOCKS_PER_SEC);
	printf("%0.2fMB peak memory usage\n", s_peakBytesUsed / 1024.0f / 1024.0f);
	printModel(model);
	objz_destroy(model);
	return 0;
}
