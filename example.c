#include <stdio.h>
#include <time.h>
#include "objzero.h"

static void printMaterial(const objzMaterial *_mat) {
	printf("Material %s\n", _mat->name);
	printf("   d: %g\n", _mat->d);
	printf("   illum: %d\n", _mat->illum);
	printf("   Ka: %g %g %g\n", _mat->Ka[0], _mat->Ka[1], _mat->Ka[2]);
	printf("   Kd: %g %g %g\n", _mat->Kd[0], _mat->Kd[1], _mat->Kd[2]);
	printf("   Ke: %g %g %g\n", _mat->Ke[0], _mat->Ke[1], _mat->Ke[2]);
	printf("   Ks: %g %g %g\n", _mat->Ks[0], _mat->Ks[1], _mat->Ks[2]);
	printf("   Ni: %g\n", _mat->Ni);
	printf("   Ns: %g\n", _mat->Ns);
	printf("   map_Bump: %s\n", _mat->map_Bump);
	printf("   map_Ka: %s\n", _mat->map_Ka);
	printf("   map_Kd: %s\n", _mat->map_Kd);
}

int main(int argc, char **argv) {
	if (argc <= 1)
		return 0;
	objzVertexDecl vertexDecl;
	objz_vertexDeclInit(&vertexDecl);
	clock_t start = clock();
	objzOutput *output = objz_load(argv[1], &vertexDecl);
	clock_t end = clock();
	if (!output) {
		printf("ERROR: %s\n", objz_getError());
		return 1;
	}
	for (uint32_t i = 0; i < output->numMaterials; i++)
		printMaterial(&output->materials[i]);
	for (uint32_t i = 0; i < output->numObjects; i++) {
		objzObject *object = &output->objects[i];
		printf("Object: %s\n", object->name);
		printf("   %d meshes\n", object->numMeshes);
		for (uint32_t j = 0; j < object->numMeshes; j++) {
			objzMesh *mesh = &output->meshes[object->firstMesh + j];
			printf("      %d: '%s' material, %d indices\n", j, mesh->materialIndex < 0 ? "<empty>" : output->materials[mesh->materialIndex].name, mesh->numIndices);
		}
	}
	printf("%g milliseconds elapsed\n", (end - start) * 1000.0 / (double)CLOCKS_PER_SEC);
	objz_destroy(output);
	return 0;
}
