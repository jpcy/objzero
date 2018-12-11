#ifndef OBJZERO_H
#define OBJZERO_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OBJZ_NAME_MAX 64
#define OBJZ_FLAG_TEXCOORDS 1<<0
#define OBJZ_FLAG_NORMALS   1<<1
#define OBJZ_FLAG_INDEX32   1<<2

typedef struct {
	size_t stride;
	size_t positionOffset;
	size_t texcoordOffset; // optional: set to SIZE_MAX to ignore
	size_t normalOffset; // optional: set to SIZE_MAX to ignore
} objzVertexDecl;

typedef struct {
	char name[OBJZ_NAME_MAX];
	float d;
	int illum;
	float Ka[3];
	float Kd[3];
	float Ke[3];
	float Ks[3];
	float Ni;
	float Ns;
	char map_Bump[OBJZ_NAME_MAX];
	char map_Ka[OBJZ_NAME_MAX];
	char map_Kd[OBJZ_NAME_MAX];
} objzMaterial;

typedef struct
{
	int32_t materialIndex; // -1 if no material
	uint32_t firstIndex;
	uint32_t numIndices;
} objzMesh;

typedef struct {
	char name[OBJZ_NAME_MAX];
	uint32_t firstMesh;
	uint32_t numMeshes;
} objzObject;

typedef struct {
	uint32_t flags;
	void *indices;
	uint32_t numIndices;
	objzMaterial *materials;
	uint32_t numMaterials;
	objzMesh *meshes;
	uint32_t numMeshes;
	objzObject *objects;
	uint32_t numObjects;
	void *vertices;
	uint32_t numVertices;
} objzOutput;

/*
Initialize objzVertexDecl to use a vertex data structure like this:

typedef struct {
	float pos[3];
	float texcoord[2];
	float normal[3];
}Vertex;
*/
void objz_vertexDeclInit(objzVertexDecl *_vertexDecl);

objzOutput *objz_load(const char *_filename, objzVertexDecl *_vertexDecl);
void objz_destroy(objzOutput *_output);
const char *objz_getError();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // OBJZERO_H
