#ifndef OBJZERO_H
#define OBJZERO_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*objzReallocFunc)(void *_ptr, size_t _size);
void objz_setRealloc(objzReallocFunc _realloc);

#define OBJZ_INDEX_FORMAT_AUTO 0
#define OBJZ_INDEX_FORMAT_U32  1

// OBJZ_INDEX_FORMAT_AUTO: objzModel indices are uint32_t if any index > UINT16_MAX, otherwise they are uint16_t.
// OBJZ_INDEX_FORMAT_U32: objzModel indices are always uint32_t.
// Default is OBJZ_INDEX_FORMAT_AUTO.
void objz_setIndexFormat(uint32_t _format);

/*
Default vertex data structure looks like this:

typedef struct {
float pos[3];
float texcoord[2];
float normal[3];
} Vertex;

Which is equivalent to:

objz_setVertexFormat(sizeof(Vertex), offsetof(Vertex, pos), offsetof(Vertex, texcoord), offsetof(Vertex, normal));

texcoordOffset - optional: set to SIZE_MAX to ignore
normalOffset - optional: set to SIZE_MAX to ignore
*/
void objz_setVertexFormat(size_t _stride, size_t _positionOffset, size_t _texcoordOffset, size_t _normalOffset);

#define OBJZ_NAME_MAX 64

typedef struct {
	char name[OBJZ_NAME_MAX];
	float ambient[3]; // Ka
	float diffuse[3]; // Kd
	float emission[3]; // Ke
	float specular[3]; // Ks
	float specularExponent; // Ns
	float opacity; // d
	char ambientTexture[OBJZ_NAME_MAX]; // map_Ka
	char bumpTexture[OBJZ_NAME_MAX]; // map_Bump
	char diffuseTexture[OBJZ_NAME_MAX]; // map_Kd
	char specularTexture[OBJZ_NAME_MAX]; // map_Ks
	char specularExponentTexture[OBJZ_NAME_MAX]; // map_Ns
	char opacityTexture[OBJZ_NAME_MAX];  // map_d
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

	// If you want per-object vertices and indices, use these and subtract firstVertex from all the objzModel indices in firstIndex to firstIndex + numIndices - 1 range.
	uint32_t firstIndex;
	uint32_t numIndices;
	uint32_t firstVertex;
	uint32_t numVertices;
} objzObject;

#define OBJZ_FLAG_TEXCOORDS (1<<0)
#define OBJZ_FLAG_NORMALS   (1<<1)
#define OBJZ_FLAG_INDEX32   (1<<2)

typedef struct {
	uint32_t flags;
	
	// uint32_t if OBJZ_FLAG_INDEX32 flag is set, otherwise uint16_t.
	// See: objz_setIndexFormat
	void *indices;

	uint32_t numIndices;
	objzMaterial *materials;
	uint32_t numMaterials;
	objzMesh *meshes;
	uint32_t numMeshes;
	objzObject *objects;
	uint32_t numObjects;
	void *vertices; // See: objz_setVertexFormat
	uint32_t numVertices;
} objzModel;

objzModel *objz_load(const char *_filename);
void objz_destroy(objzModel *_model);
const char *objz_getError();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // OBJZERO_H
