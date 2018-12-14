#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "objzero.h"

#ifdef _MSC_VER
#define OBJZ_FOPEN(_file, _filename, _mode) { if (fopen_s(&_file, _filename, _mode) != 0) _file = NULL; }
#define OBJZ_STRICMP _stricmp
#define OBJZ_STRNCAT(_dest, _destSize, _src, _count) strncat_s(_dest, _destSize, _src, _count)
#define OBJZ_STRNCPY(_dest, _destSize, _src) strncpy_s(_dest, _destSize, _src, _destSize)
#define OBJZ_STRTOK(_str, _delim, _context) strtok_s(_str, _delim, _context)
#else
#include <strings.h>
#define OBJZ_FOPEN(_file, _filename, _mode) _file = fopen(_filename, _mode)
#define OBJZ_STRICMP strcasecmp
#define OBJZ_STRNCAT(_dest, _destSize, _src, _count) strncat(_dest, _src, _count)
#define OBJZ_STRNCPY(_dest, _destSize, _src) strncpy(_dest, _src, _destSize)
#define OBJZ_STRTOK(_str, _delim, _context) strtok(_str, _delim)
#endif

#define OBJZ_MAX_ERROR_LENGTH 1024
#define OBJZ_MAX_TOKEN_LENGTH 256

static char s_error[OBJZ_MAX_ERROR_LENGTH] = { 0 };
static uint32_t s_indexFormat = OBJZ_INDEX_FORMAT_AUTO;

typedef struct {
	size_t stride;
	size_t positionOffset;
	size_t texcoordOffset;
	size_t normalOffset;
	bool custom;
} VertexFormat;

static VertexFormat s_vertexDecl = {
	.stride = sizeof(float) * 3 * 2 * 3,
	.positionOffset = 0,
	.texcoordOffset = sizeof(float) * 3,
	.normalOffset = sizeof(float) * 3 * 2,
	.custom = false
};

typedef struct {
	uint8_t *data;
	uint32_t length;
	uint32_t capacity;
	uint32_t elementSize;
	uint32_t initialCapacity;
} Array;

static void arrayInit(Array *_array, size_t _elementSize, uint32_t _initialCapacity) {
	_array->data = NULL;
	_array->length = _array->capacity = 0;
	_array->elementSize = (uint32_t)_elementSize;
	_array->initialCapacity = _initialCapacity;
}

static void arrayAppend(Array *_array, const void *_element) {
	if (!_array->data) {
		_array->data = malloc(_array->elementSize * _array->initialCapacity);
		_array->capacity = _array->initialCapacity;
	} else if (_array->length == _array->capacity) {
		_array->capacity *= 2;
		_array->data = realloc(_array->data, _array->capacity * _array->elementSize);
	}
	memcpy(&_array->data[_array->length * _array->elementSize], _element, _array->elementSize);
	_array->length++;
}

static void arraySetCapacity(Array *_array, uint32_t _capacity) {
	if (_capacity <= _array->capacity)
		return;
	if (!_array->data)
		_array->initialCapacity = _capacity;
	else {
		_array->capacity = _capacity;
		_array->data = realloc(_array->data, _array->capacity * _array->elementSize);
	}
}

#define OBJZ_ARRAY_ELEMENT(_array, _index) (void *)&(_array).data[(_array).elementSize * (_index)]

typedef struct {
	char *buf;
	int line, column;
} Lexer;

typedef struct {
	char text[OBJZ_MAX_TOKEN_LENGTH];
	int line, column;
} Token;

static void initLexer(Lexer *_lexer, char *_buf) {
	_lexer->buf = _buf;
	_lexer->line = _lexer->column = 1;
}

static bool isEol(const Lexer *_lexer) {
	return (_lexer->buf[0] == '\n' || (_lexer->buf[0] == '\r' && _lexer->buf[1] != '\n'));
}

static bool isEof(const Lexer *_lexer) {
	return (_lexer->buf[0] == 0);
}

static bool isWhitespace(const Lexer *_lexer) {
	return (_lexer->buf[0] == ' ' || _lexer->buf[0] == '\t' || _lexer->buf[0] == '\r');
}

static void skipLine(Lexer *_lexer) {
	for (;;) {
		if (isEof(_lexer))
			break;
		if (isEol(_lexer)) {
			_lexer->column = 1;
			_lexer->line++;
			_lexer->buf++;
			break;
		}
		_lexer->buf++;
		_lexer->column++;
	}
}

static void skipWhitespace(Lexer *_lexer) {
	for (;;) {
		if (isEof(_lexer))
			break;
		if (!isWhitespace(_lexer))
			break;
		_lexer->buf++;
		_lexer->column++;
	}
}

static void tokenize(Lexer *_lexer, Token *_token, bool includeWhitespace) {
	int i = 0;
	skipWhitespace(_lexer);
	_token->line = _lexer->line;
	_token->column = _lexer->column;
	for (;;) {
		if (isEof(_lexer) || isEol(_lexer) || (!includeWhitespace && isWhitespace(_lexer)))
			break;
		_token->text[i++] = _lexer->buf[0];
		_lexer->buf++;
		_lexer->column++;
	}
	_token->text[i] = 0;
}

static bool parseFloats(Lexer *_lexer, float *_result, int n) {
	Token token;
	for (int i = 0; i < n; i++) {
		tokenize(_lexer, &token, false);
		if (strlen(token.text) == 0) {
			snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Error parsing float", token.line, token.column);
			return false;
		}
		_result[i] = (float)atof(token.text);
	}
	return true;
}

static bool parseInt(Lexer *_lexer, int *_result) {
	Token token;
	tokenize(_lexer, &token, false);
	if (strlen(token.text) == 0) {
		snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Error parsing int", token.line, token.column);
		return false;
	}
	*_result = atoi(token.text);
	return true;
}

typedef struct {
	int v;
	int vt;
	int vn;
} FaceIndexTriplet;

static bool parseFace(Lexer *_lexer, Array *_faceIndexTriplets) {
	Token token;
	_faceIndexTriplets->length = 0;
	for (;;) {
		tokenize(_lexer, &token, false);
		if (token.text[0] == 0) {
			if (isEol(_lexer))
				break;
			goto error;
		}
		// Parse v/vt/vn
		const char *delim = "/";
		char *start = token.text;
		char *end = strstr(start, delim);
		if (!end)
			goto error;
		*end = 0;
		FaceIndexTriplet triplet;
		triplet.v = atoi(start);
		start = end + 1;
		end = strstr(start, delim);
		if (!end)
			goto error;
		*end = 0;
		if (start == end)
			triplet.vt = INT_MAX;
		else
			triplet.vt = atoi(start);
		start = end + 1;
		if (*start == 0)
			triplet.vn = INT_MAX;
		else
			triplet.vn = atoi(start);
		arrayAppend(_faceIndexTriplets, &triplet);
	}
	return true;
error:
	snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Failed to parse face", token.line, token.column);
	return false;
}

static char *readFile(const char *_filename) {
	FILE *f;
	OBJZ_FOPEN(f, _filename, "rb");
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	const size_t fileLength = (size_t)ftell(f);
	fseek(f, 0, SEEK_SET);
	if (fileLength <= 0) {
		fclose(f);
		return NULL;
	}
	char *buffer = malloc(fileLength + 1);
	const size_t bytesRead = fread(buffer, 1, fileLength, f);
	fclose(f);
	if (bytesRead < fileLength) {
		free(buffer);
		return NULL;
	}
	buffer[fileLength] = 0;
	return buffer;
}

#define OBJZ_MAT_TOKEN_STRING 0
#define OBJZ_MAT_TOKEN_FLOAT  1
#define OBJZ_MAT_TOKEN_INT    2

typedef struct {
	const char *name;
	int type;
	size_t offset;
	int n;
} MaterialTokenDef;

static MaterialTokenDef s_materialTokens[] = {
	{ "d", OBJZ_MAT_TOKEN_FLOAT, offsetof(objzMaterial, d), 1 },
	{ "illum", OBJZ_MAT_TOKEN_INT, offsetof(objzMaterial, illum), 1 },
	{ "Ka", OBJZ_MAT_TOKEN_FLOAT, offsetof(objzMaterial, Ka), 3 },
	{ "Kd", OBJZ_MAT_TOKEN_FLOAT, offsetof(objzMaterial, Kd), 3 },
	{ "Ke", OBJZ_MAT_TOKEN_FLOAT, offsetof(objzMaterial, Ke), 3 },
	{ "Ks", OBJZ_MAT_TOKEN_FLOAT, offsetof(objzMaterial, Ks), 3 },
	{ "Ni", OBJZ_MAT_TOKEN_FLOAT, offsetof(objzMaterial, Ni), 1 },
	{ "Ns", OBJZ_MAT_TOKEN_FLOAT, offsetof(objzMaterial, Ns), 1 },
	{ "map_Bump", OBJZ_MAT_TOKEN_STRING, offsetof(objzMaterial, map_Bump), 1 },
	{ "map_Ka", OBJZ_MAT_TOKEN_STRING, offsetof(objzMaterial, map_Ka), 1 },
	{ "map_Kd", OBJZ_MAT_TOKEN_STRING, offsetof(objzMaterial, map_Kd), 1 }
};

static void materialInit(objzMaterial *_mat) {
	memset(_mat, 0, sizeof(*_mat));
}

static int loadMaterialFile(const char *_objFilename, const char *_materialName, Array *_materials) {
	char filename[256] = { 0 };
	const char *lastSlash = strrchr(_objFilename, '/');
	if (!lastSlash)
		lastSlash = strrchr(_objFilename, '\\');
	if (lastSlash) {
		for (int i = 0;; i++) {
			filename[i] = _objFilename[i];
			if (&_objFilename[i] == lastSlash)
				break;
		}
		OBJZ_STRNCAT(filename, sizeof(filename), _materialName, sizeof(filename) - strlen(filename) - 1);
	} else
		OBJZ_STRNCPY(filename, sizeof(filename), _materialName);
	int result = -1;
	char *buffer = readFile(filename);
	if (!buffer)
		return result;
	Lexer lexer;
	initLexer(&lexer, buffer);
	Token token;
	objzMaterial mat;
	materialInit(&mat);
	for (;;) {
		tokenize(&lexer, &token, false);
		if (token.text[0] == 0) {
			if (isEof(&lexer))
				break;
		} else if (OBJZ_STRICMP(token.text, "newmtl") == 0) {
			tokenize(&lexer, &token, false);
			if (token.text[0] == 0) {
				snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Expected name after 'newmtl'", token.line, token.column);
				goto cleanup;
			}
			if (mat.name[0] != 0)
				arrayAppend(_materials, &mat);
			materialInit(&mat);
			OBJZ_STRNCPY(mat.name, sizeof(mat.name), token.text);
		} else {
			for (size_t i = 0; i < sizeof(s_materialTokens) / sizeof(s_materialTokens[0]); i++) {
				const MaterialTokenDef *mtd = &s_materialTokens[i];
				uint8_t *dest = &((uint8_t *)&mat)[mtd->offset];
				if (OBJZ_STRICMP(token.text, mtd->name) == 0) {
					if (mtd->type == OBJZ_MAT_TOKEN_STRING) {
						tokenize(&lexer, &token, false);
						if (token.text[0] == 0) {
							snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Expected name after '%s'", token.line, token.column, mtd->name);
							goto cleanup;
						}
						OBJZ_STRNCPY((char *)dest, OBJZ_NAME_MAX, token.text);
					} else if (mtd->type == OBJZ_MAT_TOKEN_FLOAT) {
						if (!parseFloats(&lexer, (float *)dest, mtd->n))
							goto cleanup;
					} else if (mtd->type == OBJZ_MAT_TOKEN_INT) {
						if (!parseInt(&lexer, (int *)dest))
							goto cleanup;
					}
					break;
				}
			}
		}
		skipLine(&lexer);
	}
	if (mat.name[0] != 0)
		arrayAppend(_materials, &mat);
	result = 1;
cleanup:
	free(buffer);
	return result;
}

#define OBJZ_VERTEX_HASH_MAP_SLOTS 4096

typedef struct {
	float pos[3];
	float texcoord[2];
	float normal[3];
} Vertex;

typedef struct {
	uint32_t pos;
	uint32_t texcoord;
	uint32_t normal;
	uint32_t hashNext; // For hash collisions: next Index with the same hash.
} Index;

typedef struct {
	uint32_t slots[OBJZ_VERTEX_HASH_MAP_SLOTS];
	Array indices;
	Array vertices;
	const Array *positions;
	const Array *texcoords;
	const Array *normals;
} VertexHashMap;

static void vertexHashMapInit(VertexHashMap *_map, const Array *_positions, const Array *_texcoords, const Array *_normals) {
	for (int i = 0; i < OBJZ_VERTEX_HASH_MAP_SLOTS; i++)
		_map->slots[i] = UINT32_MAX;
	arrayInit(&_map->indices, sizeof(Index), UINT16_MAX);
	arrayInit(&_map->vertices, sizeof(Vertex), UINT16_MAX);
	_map->positions = _positions;
	_map->texcoords = _texcoords;
	_map->normals = _normals;
}

static uint32_t vertexHashMapInsert(VertexHashMap *_map, uint32_t _pos, uint32_t _texcoord, uint32_t _normal) {
	// http://www.beosil.com/download/CollisionDetectionHashing_VMV03.pdf
	uint32_t hash = _pos * 73856093;
	if (_texcoord != UINT32_MAX)
		hash ^= _texcoord * 19349663;
	if (_normal != UINT32_MAX)
		hash ^= _normal * 83492791;
	hash %= OBJZ_VERTEX_HASH_MAP_SLOTS;
	uint32_t i = _map->slots[hash];
	while (i != UINT32_MAX) {
		const Index *index = OBJZ_ARRAY_ELEMENT(_map->indices, i);
		if (index->pos == _pos && index->texcoord == _texcoord && index->normal == _normal)
			return i;
		i = index->hashNext;
	}
	Index index;
	index.pos = _pos;
	index.texcoord = _texcoord;
	index.normal = _normal;
	index.hashNext = _map->slots[hash];
	_map->slots[hash] = _map->indices.length;
	arrayAppend(&_map->indices, &index);
	const size_t posSize = sizeof(float) * 3;
	const size_t texcoordSize = sizeof(float) * 2;
	const size_t normalSize = sizeof(float) * 3;
	Vertex vertex;
	memcpy(vertex.pos, &_map->positions->data[_pos * posSize], posSize);
	if (_texcoord == UINT32_MAX)
		vertex.texcoord[0] = vertex.texcoord[1] = 0;
	else
		memcpy(vertex.texcoord, &_map->texcoords->data[_texcoord * texcoordSize], texcoordSize);
	if (_normal == UINT32_MAX)
		vertex.normal[0] = vertex.normal[1] = vertex.normal[2] = 0;
	else
		memcpy(vertex.normal, &_map->normals->data[_normal * normalSize], normalSize);
	arrayAppend(&_map->vertices, &vertex);
	return i;
}

static void finalizeObject(objzObject *_object, Array *_objectIndices, Array *_objectFaceMaterials, Array *_meshes, Array *_indices, uint32_t _numMaterials) {
	_object->firstMesh = _meshes->length;
	_object->numMeshes = 0;
	// We know exactly how many indices are about to be appended. Avoid what would probably be a bunch of reallocations by setting the capacity directly.
	arraySetCapacity(_indices, _indices->capacity + _objectIndices->length);
	// Create one mesh per material. No material (-1) gets a mesh too.
	for (int material = -1; material < (int)_numMaterials; material++) {
		objzMesh mesh;
		mesh.firstIndex = _indices->length;
		mesh.numIndices = 0;
		mesh.materialIndex = material;
		for (uint32_t i = 0; i < _objectFaceMaterials->length; i++) {
			const int *faceMaterial = OBJZ_ARRAY_ELEMENT(*_objectFaceMaterials, i);
			if (*faceMaterial != material)
				continue;
			for (int k = 0; k < 3; k++)
				arrayAppend(_indices, OBJZ_ARRAY_ELEMENT(*_objectIndices, i * 3 + k));
			mesh.numIndices += 3;
		}
		if (mesh.numIndices > 0) {
			arrayAppend(_meshes, &mesh);
			_object->numMeshes++;
		}
	}
}

static uint32_t sanitizeVertexAttribIndex(int _index, uint32_t _n) {
	if (_index == INT_MAX)
		return UINT32_MAX;
	// Handle relative index.
	if (_index < 0)
		return (uint32_t)(_index + _n);
	// Convert from 1-indexed to 0-indexed.
	return (uint32_t)(_index - 1);
}

void objz_setIndexFormat(int _format) {
	s_indexFormat = _format;
}

void objz_setVertexFormat(size_t _stride, size_t _positionOffset, size_t _texcoordOffset, size_t _normalOffset) {
	s_vertexDecl.custom = true;
	s_vertexDecl.stride = _stride;
	s_vertexDecl.positionOffset = _positionOffset;
	s_vertexDecl.texcoordOffset = _texcoordOffset;
	s_vertexDecl.normalOffset = _normalOffset;
}

objzOutput *objz_load(const char *_filename) {
	objzOutput *output = NULL;
	char *buffer = readFile(_filename);
	if (!buffer) {
		snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "Failed to read file '%s'", _filename);
		return NULL;
	}
	Array materials, meshes, objects, indices, positions, texcoords, normals;
	Array objectIndices, objectFaceMaterials; // per object
	Array faceIndexTriplets, faceIndices; // per face
	arrayInit(&materials, sizeof(objzMaterial), 8);
	arrayInit(&meshes, sizeof(objzMesh), 8);
	arrayInit(&objects, sizeof(objzObject), 8);
	arrayInit(&indices, sizeof(uint32_t), UINT16_MAX);
	arrayInit(&positions, sizeof(float) * 3, UINT16_MAX);
	arrayInit(&texcoords, sizeof(float) * 2, UINT16_MAX);
	arrayInit(&normals, sizeof(float) * 3, UINT16_MAX);
	arrayInit(&objectIndices, sizeof(uint32_t), UINT16_MAX);
	arrayInit(&objectFaceMaterials, sizeof(int), UINT16_MAX);
	arrayInit(&faceIndexTriplets, sizeof(FaceIndexTriplet), 8);
	arrayInit(&faceIndices, sizeof(uint32_t), 8);
	VertexHashMap vertexHashMap;
	vertexHashMapInit(&vertexHashMap, &positions, &texcoords, &normals);
	int currentMaterialIndex = -1;
	uint32_t flags = 0;
	Lexer lexer;
	initLexer(&lexer, buffer);
	Token token;
	for (;;) {
		tokenize(&lexer, &token, false);
		if (token.text[0] == 0) {
			if (isEof(&lexer))
				break;
		} else if (OBJZ_STRICMP(token.text, "f") == 0) {
			if (!parseFace(&lexer, &faceIndexTriplets))
				goto error;
			faceIndices.length = 0;
			for (uint32_t i = 0; i < faceIndexTriplets.length; i++) {
				const FaceIndexTriplet *triplet = OBJZ_ARRAY_ELEMENT(faceIndexTriplets, i);
				const uint32_t v = sanitizeVertexAttribIndex(triplet->v, positions.length);
				const uint32_t vt = sanitizeVertexAttribIndex(triplet->vt, texcoords.length);
				const uint32_t vn = sanitizeVertexAttribIndex(triplet->vn, normals.length);
				const uint32_t index = vertexHashMapInsert(&vertexHashMap, v, vt, vn);
				if (index > UINT16_MAX)
					flags |= OBJZ_FLAG_INDEX32;
				arrayAppend(&faceIndices, &index);
			}
			if (faceIndices.length < 3) {
				snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Face needs at least 3 indices", token.line, token.column);
				goto error;
			}
			for (int i = 0; i < (int)faceIndices.length - 3 + 1; i++) {
				arrayAppend(&objectIndices, OBJZ_ARRAY_ELEMENT(faceIndices, 0));
				arrayAppend(&objectIndices, OBJZ_ARRAY_ELEMENT(faceIndices, i + 1));
				arrayAppend(&objectIndices, OBJZ_ARRAY_ELEMENT(faceIndices, i + 2));
				arrayAppend(&objectFaceMaterials, &currentMaterialIndex);
			}
		} else if (OBJZ_STRICMP(token.text, "mtllib") == 0) {
			tokenize(&lexer, &token, true);
			if (token.text[0] == 0) {
				snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Expected name after 'mtllib'", token.line, token.column);
				goto error;
			}
			if (!loadMaterialFile(_filename, token.text, &materials))
				goto error;
		} else if (OBJZ_STRICMP(token.text, "o") == 0) {
			tokenize(&lexer, &token, false);
			if (token.text[0] == 0) {
				snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Expected name after 'o'", token.line, token.column);
				goto error;
			}
			if (objects.length > 0)
				finalizeObject(OBJZ_ARRAY_ELEMENT(objects, objects.length - 1), &objectIndices, &objectFaceMaterials, &meshes, &indices, materials.length);
			objzObject o;
			OBJZ_STRNCPY(o.name, sizeof(o.name), token.text);
			arrayAppend(&objects, &o);
			objectIndices.length = 0;
			objectFaceMaterials.length = 0;
		} else if (OBJZ_STRICMP(token.text, "usemtl") == 0) {
			tokenize(&lexer, &token, false);
			if (token.text[0] == 0) {
				snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Expected name after 'usemtl'", token.line, token.column);
				goto error;
			}
			currentMaterialIndex = -1;
			for (uint32_t i = 0; i < materials.length; i++) {
				const objzMaterial *mat = OBJZ_ARRAY_ELEMENT(materials, i);
				if (OBJZ_STRICMP(mat->name, token.text) == 0) {
					currentMaterialIndex = (int)i;
					break;
				}
			}
		} else if (OBJZ_STRICMP(token.text, "v") == 0 || OBJZ_STRICMP(token.text, "vn") == 0 || OBJZ_STRICMP(token.text, "vt") == 0) {
			int n = 3;
			if (OBJZ_STRICMP(token.text, "vt") == 0)
				n = 2;
			float vertex[3];
			if (!parseFloats(&lexer, vertex, n))
				goto error;
			if (OBJZ_STRICMP(token.text, "v") == 0)
				arrayAppend(&positions, vertex);
			else if (OBJZ_STRICMP(token.text, "vn") == 0) {
				arrayAppend(&normals, vertex);
				flags |= OBJZ_FLAG_NORMALS;
			}
			else if (OBJZ_STRICMP(token.text, "vt") == 0) {
				arrayAppend(&texcoords, vertex);
				flags |= OBJZ_FLAG_TEXCOORDS;
			}
		}
		skipLine(&lexer);
	}
	if (objects.length == 0 && objectIndices.length > 0) {
		// No objects specifed, but there's some geometry, so create one.
		objzObject o;
		o.name[0] = 0;
		arrayAppend(&objects, &o);
	}
	if (objects.length > 0)
		finalizeObject(OBJZ_ARRAY_ELEMENT(objects, objects.length - 1), &objectIndices, &objectFaceMaterials, &meshes, &indices, materials.length);
	printf("%u positions\n", positions.length);
	printf("%u normals\n", normals.length);
	printf("%u texcoords\n", texcoords.length);
	printf("%u unique vertices\n", vertexHashMap.vertices.length);
	output = malloc(sizeof(objzOutput));
	output->flags = flags;
	if (s_indexFormat == OBJZ_INDEX_FORMAT_U32 || (flags & OBJZ_FLAG_INDEX32))
		output->indices = indices.data;
	else {
		flags &= ~OBJZ_FLAG_INDEX32;
		output->indices = malloc(sizeof(uint16_t) * indices.length);
		for (uint32_t i = 0; i < indices.length; i++) {
			uint32_t *index = (uint32_t *)OBJZ_ARRAY_ELEMENT(indices, i);
			((uint16_t *)output->indices)[i] = (uint16_t)*index;
		}
		free(indices.data);
	}
	output->numIndices = indices.length;
	output->materials = (objzMaterial *)materials.data;
	output->numMaterials = materials.length;
	output->meshes = (objzMesh *)meshes.data;
	output->numMeshes = meshes.length;
	output->objects = (objzObject *)objects.data;
	output->numObjects = objects.length;
	if (!s_vertexDecl.custom) {
		// Desired vertex format matches the internal one.
		output->vertices = vertexHashMap.vertices.data;
	} else {
		// Copy vertex data into the desired format.
		output->vertices = malloc(s_vertexDecl.stride * vertexHashMap.vertices.length);
		memset(output->vertices, 0, s_vertexDecl.stride * vertexHashMap.vertices.length);
		for (uint32_t i = 0; i < vertexHashMap.vertices.length; i++) {
			uint8_t *vOut = &((uint8_t *)output->vertices)[i * s_vertexDecl.stride];
			const Vertex *vIn = OBJZ_ARRAY_ELEMENT(vertexHashMap.vertices, i);
			if (s_vertexDecl.positionOffset != SIZE_MAX)
				memcpy(&vOut[s_vertexDecl.positionOffset], vIn->pos, sizeof(float) * 3);
			if (s_vertexDecl.texcoordOffset != SIZE_MAX)
				memcpy(&vOut[s_vertexDecl.texcoordOffset], vIn->texcoord, sizeof(float) * 2);
			if (s_vertexDecl.normalOffset != SIZE_MAX)
				memcpy(&vOut[s_vertexDecl.normalOffset], vIn->normal, sizeof(float) * 3);
		}
		free(vertexHashMap.vertices.data);
	}
	output->numVertices = vertexHashMap.vertices.length;
	free(positions.data);
	free(texcoords.data);
	free(normals.data);
	free(objectIndices.data);
	free(objectFaceMaterials.data);
	free(faceIndexTriplets.data);
	free(faceIndices.data);
	free(vertexHashMap.indices.data);
	free(buffer);
	return output;
error:
	free(materials.data);
	free(meshes.data);
	free(objects.data);
	free(indices.data);
	free(positions.data);
	free(texcoords.data);
	free(normals.data);
	free(objectIndices.data);
	free(objectFaceMaterials.data);
	free(faceIndexTriplets.data);
	free(faceIndices.data);
	free(vertexHashMap.vertices.data);
	free(vertexHashMap.indices.data);
	free(buffer);
	return NULL;
}

void objz_destroy(objzOutput *_output) {
	if (!_output)
		return;
	free(_output->indices);
	free(_output->materials);
	free(_output->meshes);
	free(_output->objects);
	free(_output->vertices);
	free(_output);
}

const char *objz_getError() {
	return s_error;
}
