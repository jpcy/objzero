#ifndef OBJZERO_H
#define OBJZERO_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OBJZ_NAME_MAX 64
#define OBJZ_INDEX_FORMAT_AUTO 0
#define OBJZ_INDEX_FORMAT_U32  1
#define OBJZ_FLAG_TEXCOORDS (1<<0)
#define OBJZ_FLAG_NORMALS   (1<<1)
#define OBJZ_FLAG_INDEX32   (1<<2)

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
	// If you want per-object vertices and indices, use these and subtract firstVertex from all the objzModel::indices in firstIndex to firstIndex + numIndices range.
	uint32_t firstIndex;
	uint32_t numIndices;
	uint32_t firstVertex;
	uint32_t numVertices;
} objzObject;

typedef struct {
	uint32_t flags;
	// Will be uint32_t if OBJZ_FLAG_INDEX32 flag is set, otherwise uint16_t.
	// See: objz_setIndexFormat
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
} objzModel;

typedef void *(*objzReallocFunc)(void *_ptr, size_t _size);

void objz_setRealloc(objzReallocFunc _realloc);

// Default is OBJZ_INDEX_FORMAT_AUTO.
void objz_setIndexFormat(int _format);

/*
Default vertex data structure looks like this:

typedef struct {
	float pos[3];
	float texcoord[2];
	float normal[3];
} Vertex;

texcoordOffset - optional: set to SIZE_MAX to ignore
normalOffset - optional: set to SIZE_MAX to ignore
*/
void objz_setVertexFormat(size_t _stride, size_t _positionOffset, size_t _texcoordOffset, size_t _normalOffset);

objzModel *objz_load(const char *_filename);
void objz_destroy(objzModel *_model);
const char *objz_getError();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // OBJZERO_H
