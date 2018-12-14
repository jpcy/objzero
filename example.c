#include <stdio.h>
#include <time.h>
#include "objzero.h"

static void printModel(const objzModel *_model) {
	/*for (uint32_t i = 0; i < _model->numMaterials; i++) {
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
	for (uint32_t i = 0; i < _model->numObjects; i++) {
		objzObject *object = &_model->objects[i];
		printf("Object: %s\n", object->name);
		printf("   %d meshes\n", object->numMeshes);
		for (uint32_t j = 0; j < object->numMeshes; j++) {
			objzMesh *mesh = &_model->meshes[object->firstMesh + j];
			printf("      %d: '%s' material, %d indices\n", j, mesh->materialIndex < 0 ? "<empty>" : _model->materials[mesh->materialIndex].name, mesh->numIndices);
		}
	}*/
	printf("%u objects\n", _model->numObjects);
	printf("%u materials\n", _model->numMaterials);
	printf("%u meshes\n", _model->numMeshes);
	printf("%u vertices\n", _model->numVertices);
	printf("%u triangles\n", _model->numIndices / 3);
}

int main(int argc, char **argv) {
	if (argc <= 1)
		return 0;
	printf("Loading '%s'\n", argv[1]);
	clock_t start = clock();
	objzModel *model = objz_load(argv[1]);
	clock_t end = clock();
	if (!model) {
		printf("ERROR: %s\n", objz_getError());
		return 1;
	}
	printf("%g milliseconds elapsed\n", (end - start) * 1000.0 / (double)CLOCKS_PER_SEC);
	printModel(model);
	objz_destroy(model);
	return 0;
}
