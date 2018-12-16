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
static objzReallocFunc s_realloc = NULL;
static uint32_t s_indexFormat = OBJZ_INDEX_FORMAT_AUTO;

static void *objz_malloc(size_t _size) {
	void *result;
	if (s_realloc)
		result = s_realloc(NULL, _size);
	else
		result = malloc(_size);
	if (!result) {
		fprintf(stderr, "Memory allocation failed\n");
		abort();
	}
	return result;
}

static void *objz_realloc(void *_ptr, size_t _size) {
	void *result;
	if (s_realloc)
		result = s_realloc(_ptr, _size);
	else
		result = realloc(_ptr, _size);
	if (!result) {
		fprintf(stderr, "Memory allocation failed\n");
		abort();
	}
	return result;
}

static void objz_free(void *_ptr) {
	if (!_ptr)
		return;
	if (s_realloc)
		s_realloc(_ptr, 0);
	else
		free(_ptr);
}

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
		_array->data = objz_malloc(_array->elementSize * _array->initialCapacity);
		_array->capacity = _array->initialCapacity;
	} else if (_array->length == _array->capacity) {
		_array->capacity *= 2;
		_array->data = objz_realloc(_array->data, _array->capacity * _array->elementSize);
	}
	memcpy(&_array->data[_array->length * _array->elementSize], _element, _array->elementSize);
	_array->length++;
}

#if 0
static void arraySetCapacity(Array *_array, uint32_t _capacity) {
	if (_capacity <= _array->capacity)
		return;
	if (!_array->data)
		_array->initialCapacity = _capacity;
	else {
		_array->capacity = _capacity;
		_array->data = objz_realloc(_array->data, _array->capacity * _array->elementSize);
	}
}
#endif

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
	char *buffer = objz_malloc(fileLength + 1);
	const size_t bytesRead = fread(buffer, 1, fileLength, f);
	fclose(f);
	if (bytesRead < fileLength) {
		objz_free(buffer);
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
	objz_free(buffer);
	return result;
}

typedef struct {
	float pos[3];
	float texcoord[2];
	float normal[3];
} Vertex;

typedef struct {
	uint32_t object;
	uint32_t pos;
	uint32_t texcoord;
	uint32_t normal;
	uint32_t hashNext; // For hash collisions: next Index with the same hash.
} Index;

typedef struct {
	uint32_t *slots;
	uint32_t numSlots;
	Array indices;
	Array vertices;
	const Array *positions;
	const Array *texcoords;
	const Array *normals;
} VertexHashMap;

static void vertexHashMapInit(VertexHashMap *_map, const Array *_positions, const Array *_texcoords, const Array *_normals) {
	_map->numSlots = _positions->length * 2;
	_map->slots = objz_malloc(sizeof(uint32_t) * _map->numSlots);
	for (uint32_t i = 0; i < _map->numSlots; i++)
		_map->slots[i] = UINT32_MAX;
	arrayInit(&_map->indices, sizeof(Index), _positions->length);
	arrayInit(&_map->vertices, sizeof(Vertex), _positions->length);
	_map->positions = _positions;
	_map->texcoords = _texcoords;
	_map->normals = _normals;
}

static void vertexHashMapDestroy(VertexHashMap *_map) {
	objz_free(_map->slots);
}

static uint32_t sdbmHash(const uint8_t *_data, uint32_t _size)
{
	uint32_t hash = 0;
	for (uint32_t i = 0; i < _size; i++)
		hash = (uint32_t)_data[i] + (hash << 6) + (hash << 16) - hash;
	return hash;
}

static uint32_t vertexHashMapInsert(VertexHashMap *_map, uint32_t _object, uint32_t _pos, uint32_t _texcoord, uint32_t _normal) {
	uint32_t hashData[4] = { 0 };
	hashData[0] = _object;
	hashData[1] = _pos;
	if (_texcoord != UINT32_MAX)
		hashData[2] = _texcoord;
	if (_normal != UINT32_MAX)
		hashData[3] = _normal;
	const uint32_t hash = sdbmHash((const uint8_t *)hashData, sizeof(hashData)) % _map->numSlots;
	uint32_t i = _map->slots[hash];
	while (i != UINT32_MAX) {
		const Index *index = OBJZ_ARRAY_ELEMENT(_map->indices, i);
		if (index->object == _object && index->pos == _pos && index->texcoord == _texcoord && index->normal == _normal)
			return i;
		i = index->hashNext;
	}
	Index index;
	index.object = _object;
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
	return _map->slots[hash];
}

// Convert 1-indexed relative vertex attrib index to a 0-indexed absolute index.
static uint32_t fixVertexAttribIndex(int32_t _index, uint32_t _n) {
	if (_index == INT_MAX)
		return UINT32_MAX;
	// Handle relative index.
	if (_index < 0)
		return (uint32_t)(_index + _n);
	// Convert from 1-indexed to 0-indexed.
	return (uint32_t)(_index - 1);
}

typedef struct {
	char name[OBJZ_NAME_MAX];
	uint32_t firstFace;
	uint32_t numFaces;
} TempObject;

typedef struct {
	uint32_t v;
	uint32_t vt;
	uint32_t vn;
} VertexAttribTriplet;

typedef struct {
	int32_t materialIndex;
	VertexAttribTriplet vertices[3];
} TempFace;

void objz_setRealloc(objzReallocFunc _realloc) {
	s_realloc = _realloc;
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

objzModel *objz_load(const char *_filename) {
	objzModel *model = NULL;
	char *buffer = readFile(_filename);
	if (!buffer) {
		snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "Failed to read file '%s'", _filename);
		return NULL;
	}
	// Parse the obj file and any material files.
	Array materials, positions, texcoords, normals, tempObjects, tempFaces;
	Array vertexAttribTriplets; // Re-used per face.
	arrayInit(&materials, sizeof(objzMaterial), 8);
	arrayInit(&positions, sizeof(float) * 3, UINT16_MAX);
	arrayInit(&texcoords, sizeof(float) * 2, UINT16_MAX);
	arrayInit(&normals, sizeof(float) * 3, UINT16_MAX);
	arrayInit(&tempObjects, sizeof(TempObject), 8);
	arrayInit(&tempFaces, sizeof(TempFace), UINT16_MAX);
	arrayInit(&vertexAttribTriplets, sizeof(VertexAttribTriplet), 8);
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
			// Get current object.
			if (tempObjects.length == 0) {
				// No objects specifed, but there's a face, so create one.
				TempObject o;
				o.name[0] = 0;
				o.firstFace = o.numFaces = 0;
				arrayAppend(&tempObjects, &o);
			}
			TempObject *object = OBJZ_ARRAY_ELEMENT(tempObjects, tempObjects.length - 1);
			// Parse triplets.
			vertexAttribTriplets.length = 0;
			for (;;) {
				Token tripletToken;
				tokenize(&lexer, &tripletToken, false);
				if (tripletToken.text[0] == 0) {
					if (isEol(&lexer))
						break;
					snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Failed to parse face", tripletToken.line, tripletToken.column);
					goto error;
				}
				// Parse v/vt/vn
				const char *delim = "/";
				char *start = tripletToken.text;
				char *end = strstr(start, delim);
				if (!end) {
					snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Failed to parse face", tripletToken.line, tripletToken.column);
					goto error;
				}
				int32_t v, vt, vn;
				*end = 0;
				v = atoi(start);
				start = end + 1;
				end = strstr(start, delim);
				if (!end) {
					snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Failed to parse face", tripletToken.line, tripletToken.column);
					goto error;
				}
				*end = 0;
				if (start == end)
					vt = INT_MAX;
				else
					vt = atoi(start);
				start = end + 1;
				if (*start == 0)
					vn = INT_MAX;
				else
					vn = atoi(start);
				VertexAttribTriplet triplet;
				triplet.v = fixVertexAttribIndex(v, positions.length);
				triplet.vt = fixVertexAttribIndex(vt, texcoords.length);
				triplet.vn = fixVertexAttribIndex(vn, normals.length);
				arrayAppend(&vertexAttribTriplets, &triplet);
			}
			if (vertexAttribTriplets.length < 3) {
				snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Face needs at least 3 indices", token.line, token.column);
				goto error;
			}
			// Triangulate.
			// TODO: handle concave
			for (int i = 0; i < (int)vertexAttribTriplets.length - 3 + 1; i++) {
				TempFace face;
				face.materialIndex = currentMaterialIndex;
				face.vertices[0] = *(VertexAttribTriplet *)OBJZ_ARRAY_ELEMENT(vertexAttribTriplets, 0);
				face.vertices[1] = *(VertexAttribTriplet *)OBJZ_ARRAY_ELEMENT(vertexAttribTriplets, i + 1);
				face.vertices[2] = *(VertexAttribTriplet *)OBJZ_ARRAY_ELEMENT(vertexAttribTriplets, i + 2);
				arrayAppend(&tempFaces, &face);
				object->numFaces++;
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
			TempObject o;
			OBJZ_STRNCPY(o.name, sizeof(o.name), token.text);
			o.firstFace = tempFaces.length;
			o.numFaces = 0;
			arrayAppend(&tempObjects, &o);
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
	// Do some post-processing of parsed data:
	//   * find unique vertices from separately index vertex attributes (pos, texcoord, normal).
	//   * build meshes by batching object faces by material
	Array meshes, objects, indices;
	arrayInit(&meshes, sizeof(objzMesh), tempObjects.length * 4);
	arrayInit(&objects, sizeof(objzObject), tempObjects.length);
	arrayInit(&indices, sizeof(uint32_t), UINT16_MAX);
	VertexHashMap vertexHashMap;
	vertexHashMapInit(&vertexHashMap, &positions, &texcoords, &normals);
	for (uint32_t i = 0; i < tempObjects.length; i++) {
		const TempObject *tempObject = OBJZ_ARRAY_ELEMENT(tempObjects, i);
		objzObject object;
		OBJZ_STRNCPY(object.name, sizeof(object.name), tempObject->name);
		// Create one mesh per material. No material (-1) gets a mesh too.
		object.firstMesh = meshes.length;
		object.numMeshes = 0;
		for (int material = -1; material < (int)materials.length; material++) {
			objzMesh mesh;
			mesh.firstIndex = indices.length;
			mesh.numIndices = 0;
			mesh.materialIndex = material;
			for (uint32_t j = 0; j < tempObject->numFaces; j++) {
				const TempFace *face = OBJZ_ARRAY_ELEMENT(tempFaces, tempObject->firstFace + j);
				if (face->materialIndex != material)
					continue;
				for (int k = 0; k < 3; k++) {
					const VertexAttribTriplet *triplet = &face->vertices[k];
					const uint32_t index = vertexHashMapInsert(&vertexHashMap, i, triplet->v, triplet->vt, triplet->vn);
					if (index > UINT16_MAX)
						flags |= OBJZ_FLAG_INDEX32;
					arrayAppend(&indices, &index);
					mesh.numIndices++;
				}
			}
			if (mesh.numIndices > 0) {
				arrayAppend(&meshes, &mesh);
				object.numMeshes++;
			}
		}
		if (objects.length > 0) {
			const objzObject *prev = OBJZ_ARRAY_ELEMENT(objects, objects.length - 1);
			object.firstIndex = prev->firstIndex + prev->numIndices;
			object.firstVertex = prev->firstVertex + prev->numVertices;
		} else {
			object.firstIndex = 0;
			object.firstVertex = 0;
		}
		object.numIndices = indices.length - object.firstIndex;
		object.numVertices = vertexHashMap.vertices.length - object.firstVertex;
		arrayAppend(&objects, &object);
	}
	// Build output data structure.
	model = objz_malloc(sizeof(objzModel));
	model->flags = flags;
	if (s_indexFormat == OBJZ_INDEX_FORMAT_U32 || (flags & OBJZ_FLAG_INDEX32))
		model->indices = indices.data;
	else {
		flags &= ~OBJZ_FLAG_INDEX32;
		model->indices = objz_malloc(sizeof(uint16_t) * indices.length);
		for (uint32_t i = 0; i < indices.length; i++) {
			uint32_t *index = (uint32_t *)OBJZ_ARRAY_ELEMENT(indices, i);
			((uint16_t *)model->indices)[i] = (uint16_t)*index;
		}
		objz_free(indices.data);
	}
	model->numIndices = indices.length;
	model->materials = (objzMaterial *)materials.data;
	model->numMaterials = materials.length;
	model->meshes = (objzMesh *)meshes.data;
	model->numMeshes = meshes.length;
	model->objects = (objzObject *)objects.data;
	model->numObjects = objects.length;
	if (!s_vertexDecl.custom) {
		// Desired vertex format matches the internal one.
		model->vertices = vertexHashMap.vertices.data;
	} else {
		// Copy vertex data into the desired format.
		model->vertices = objz_malloc(s_vertexDecl.stride * vertexHashMap.vertices.length);
		memset(model->vertices, 0, s_vertexDecl.stride * vertexHashMap.vertices.length);
		for (uint32_t i = 0; i < vertexHashMap.vertices.length; i++) {
			uint8_t *vOut = &((uint8_t *)model->vertices)[i * s_vertexDecl.stride];
			const Vertex *vIn = OBJZ_ARRAY_ELEMENT(vertexHashMap.vertices, i);
			if (s_vertexDecl.positionOffset != SIZE_MAX)
				memcpy(&vOut[s_vertexDecl.positionOffset], vIn->pos, sizeof(float) * 3);
			if (s_vertexDecl.texcoordOffset != SIZE_MAX)
				memcpy(&vOut[s_vertexDecl.texcoordOffset], vIn->texcoord, sizeof(float) * 2);
			if (s_vertexDecl.normalOffset != SIZE_MAX)
				memcpy(&vOut[s_vertexDecl.normalOffset], vIn->normal, sizeof(float) * 3);
		}
		objz_free(vertexHashMap.vertices.data);
	}
	model->numVertices = vertexHashMap.vertices.length;
	objz_free(positions.data);
	objz_free(texcoords.data);
	objz_free(normals.data);
	objz_free(vertexAttribTriplets.data);
	objz_free(vertexHashMap.indices.data);
	vertexHashMapDestroy(&vertexHashMap);
	objz_free(buffer);
	return model;
error:
	objz_free(materials.data);
	objz_free(positions.data);
	objz_free(texcoords.data);
	objz_free(normals.data);
	objz_free(vertexAttribTriplets.data);
	objz_free(buffer);
	return NULL;
}

void objz_destroy(objzModel *_model) {
	if (!_model)
		return;
	objz_free(_model->indices);
	objz_free(_model->materials);
	objz_free(_model->meshes);
	objz_free(_model->objects);
	objz_free(_model->vertices);
	objz_free(_model);
}

const char *objz_getError() {
	return s_error;
}
