#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "objzero.h"

#ifdef _MSC_VER
#define OBJZ_FOPEN(file, filename, mode) { if (fopen_s(&file, filename, mode) != 0) file = NULL; }
#define OBJZ_STRICMP _stricmp
#define OBJZ_STRNCAT(dest, destSize, src, count) strncat_s(dest, destSize, src, count)
#define OBJZ_STRNCPY(dest, destSize, src) strncpy_s(dest, destSize, src, destSize)
#define OBJZ_STRTOK(str, delim, context) strtok_s(str, delim, context)
#else
#include <strings.h>
#define OBJZ_FOPEN(file, filename, mode) file = fopen(filename, mode)
#define OBJZ_STRICMP strcasecmp
#define OBJZ_STRNCAT(dest, destSize, src, count) strncat(dest, src, count)
#define OBJZ_STRNCPY(dest, destSize, src) strncpy(dest, src, destSize)
#define OBJZ_STRTOK(str, delim, context) strtok(str, delim)
#endif

#define OBJZ_MAX_ERROR_LENGTH 1024
#define OBJZ_MAX_TOKEN_LENGTH 256

static char s_error[OBJZ_MAX_ERROR_LENGTH] = { 0 };

typedef struct {
	char *buf;
	int line, column;
} Lexer;

typedef struct {
	char text[OBJZ_MAX_TOKEN_LENGTH];
	int line, column;
} Token;

static void objz_initLexer(Lexer *_lexer, char *_buf) {
	_lexer->buf = _buf;
	_lexer->line = _lexer->column = 1;
}

static bool objz_isEol(const Lexer *_lexer) {
	return (_lexer->buf[0] == '\n' || (_lexer->buf[0] == '\r' && _lexer->buf[1] != '\n'));
}

static bool objz_isEof(const Lexer *_lexer) {
	return (_lexer->buf[0] == 0);
}

static bool objz_isWhitespace(const Lexer *_lexer) {
	return (_lexer->buf[0] == ' ' || _lexer->buf[0] == '\t' || _lexer->buf[0] == '\r');
}

static void objz_skipLine(Lexer *_lexer) {
	for (;;) {
		if (objz_isEof(_lexer))
			break;
		if (objz_isEol(_lexer)) {
			_lexer->column = 1;
			_lexer->line++;
			_lexer->buf++;
			break;
		}
		_lexer->buf++;
		_lexer->column++;
	}
}

static void objz_skipWhitespace(Lexer *_lexer) {
	for (;;) {
		if (objz_isEof(_lexer))
			break;
		if (!objz_isWhitespace(_lexer))
			break;
		_lexer->buf++;
		_lexer->column++;
	}
}

static void objz_tokenize(Lexer *_lexer, Token *_token, bool includeWhitespace) {
	int i = 0;
	objz_skipWhitespace(_lexer);
	_token->line = _lexer->line;
	_token->column = _lexer->column;
	for (;;) {
		if (objz_isEof(_lexer) || objz_isEol(_lexer) || (!includeWhitespace && objz_isWhitespace(_lexer)))
			break;
		_token->text[i++] = _lexer->buf[0];
		_lexer->buf++;
		_lexer->column++;
	}
	_token->text[i] = 0;
}

static bool objz_parseFloats(Lexer *_lexer, float *_result, int n) {
	Token token;
	for (int i = 0; i < n; i++) {
		objz_tokenize(_lexer, &token, false);
		if (strlen(token.text) == 0) {
			snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Error parsing float", token.line, token.column);
			return false;
		}
		_result[i] = (float)atof(token.text);
	}
	return true;
}

static bool objz_parseInt(Lexer *_lexer, int *_result) {
	Token token;
	objz_tokenize(_lexer, &token, false);
	if (strlen(token.text) == 0) {
		snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Error parsing int", token.line, token.column);
		return false;
	}
	*_result = atoi(token.text);
	return true;
}

static bool objz_parseFace(Lexer *_lexer, int *_face, int *_n) {
	Token token;
	*_n = 0;
	for (int i = 0; i < 4; i++) {
		objz_tokenize(_lexer, &token, false);
		if (token.text[0] == 0) {
			if (i == 3 && objz_isEol(_lexer))
				break;
			goto error;
		}
		const char *delim = "/";
		// v
		char *context = NULL;
		context = context; // Silence "unused variable" warning.
		char *triplet = OBJZ_STRTOK(token.text, delim, &context);
		if (!triplet)
			goto error;
		_face[i * 3 + 0] = atoi(triplet);
		// vt (optional)
		triplet = OBJZ_STRTOK(NULL, delim, &context);
		_face[i * 3 + 1] = triplet ? atoi(triplet) : INT_MAX;
		// vn (optional)
		triplet = OBJZ_STRTOK(NULL, delim, &context);
		_face[i * 3 + 2] = triplet ? atoi(triplet) : INT_MAX;
		(*_n)++;
	}
	return true;
error:
	snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Failed to parse face", token.line, token.column);
	return false;
}

typedef struct {
	uint8_t *data;
	size_t length;
	size_t capacity;
	size_t initialSize;
} Array;

static void objz_initArray(Array *_array, size_t _initialSize) {
	_array->data = NULL;
	_array->length = _array->capacity = 0;
	_array->initialSize = _initialSize;
}

static void objz_appendArray(Array *_array, void *_object, size_t _objectSize) {
	if (!_array->data) {
		_array->data = malloc(_objectSize * _array->initialSize);
		_array->capacity = _array->initialSize;
	} else if (_array->length == _array->capacity) {
		const void *oldData = _array->data;
		const size_t oldCapacity = _array->capacity;
		_array->capacity *= 2;
		_array->data = realloc(_array->data, _array->capacity * _objectSize);
		memcpy(_array->data, oldData, oldCapacity * _objectSize);
	}
	memcpy(&_array->data[_array->length * _objectSize], _object, _objectSize);
	_array->length++;
}

static char *objz_readFile(const char *_filename) {
	FILE *f;
	OBJZ_FOPEN(f, _filename, "rb");
	if (!f) {
		snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "Failed to open file '%s'", _filename);
		return NULL;
	}
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

static void objz_initMaterial(objzMaterial *_mat) {
	memset(_mat, 0, sizeof(*_mat));
}

static int objz_loadMaterialFile(const char *_objFilename, const char *_materialName, Array *_materials) {
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
	char *buffer = objz_readFile(filename);
	if (!buffer)
		goto cleanup;
	Lexer lexer;
	objz_initLexer(&lexer, buffer);
	Token token;
	objzMaterial mat;
	objz_initMaterial(&mat);
	for (;;) {
		objz_tokenize(&lexer, &token, false);
		if (token.text[0] == 0) {
			if (objz_isEof(&lexer))
				break;
		} else if (OBJZ_STRICMP(token.text, "newmtl") == 0) {
			objz_tokenize(&lexer, &token, false);
			if (token.text[0] == 0) {
				snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Expected name after 'newmtl'", token.line, token.column);
				goto cleanup;
			}
			if (mat.name[0] != 0)
				objz_appendArray(_materials, &mat, sizeof(mat));
			objz_initMaterial(&mat);
			OBJZ_STRNCPY(mat.name, sizeof(mat.name), token.text);
		} else {
			for (size_t i = 0; i < sizeof(s_materialTokens) / sizeof(s_materialTokens[0]); i++) {
				const MaterialTokenDef *mtd = &s_materialTokens[i];
				uint8_t *dest = &((uint8_t *)&mat)[mtd->offset];
				if (OBJZ_STRICMP(token.text, mtd->name) == 0) {
					if (mtd->type == OBJZ_MAT_TOKEN_STRING) {
						objz_tokenize(&lexer, &token, false);
						if (token.text[0] == 0) {
							snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Expected name after '%s'", token.line, token.column, mtd->name);
							goto cleanup;
						}
						OBJZ_STRNCPY((char *)dest, OBJZ_NAME_MAX, token.text);
					} else if (mtd->type == OBJZ_MAT_TOKEN_FLOAT) {
						if (!objz_parseFloats(&lexer, (float *)dest, mtd->n))
							goto cleanup;
					} else if (mtd->type == OBJZ_MAT_TOKEN_INT) {
						if (!objz_parseInt(&lexer, (int *)dest))
							goto cleanup;
					}
					break;
				}
			}
		}
		objz_skipLine(&lexer);
	}
	if (mat.name[0] != 0)
		objz_appendArray(_materials, &mat, sizeof(mat));
	result = 1;
cleanup:
	free(buffer);
	return result;
}

objzOutput *objz_load(const char *_filename) {
	objzOutput *output = NULL;
	char *buffer = objz_readFile(_filename);
	if (!buffer)
		goto cleanup;
	Array materials;
	objz_initArray(&materials, 8);
	Lexer lexer;
	objz_initLexer(&lexer, buffer);
	Token token;
	for (;;) {
		objz_tokenize(&lexer, &token, false);
		if (token.text[0] == 0) {
			if (objz_isEof(&lexer))
				break;
		} else if (OBJZ_STRICMP(token.text, "f") == 0) {
			int face[4*3], numVerts;
			if (!objz_parseFace(&lexer, face, &numVerts))
				goto cleanup;
			/*printf("Face (%d verts):\n", numVerts);
			for (int i = 0; i < numVerts; i++)
				printf("%d %d %d\n", face[i * 3 + 0], face[i * 3 + 1], face[i * 3 + 2]);*/
		} else if (OBJZ_STRICMP(token.text, "mtllib") == 0) {
			objz_tokenize(&lexer, &token, true);
			if (token.text[0] == 0) {
				snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Expected name after 'mtllib'", token.line, token.column);
				goto cleanup;
			}
			if (!objz_loadMaterialFile(_filename, token.text, &materials))
				goto cleanup;
		} else if (OBJZ_STRICMP(token.text, "o") == 0) {
			objz_tokenize(&lexer, &token, false);
			if (token.text[0] == 0) {
				snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Expected name after 'o'", token.line, token.column);
				goto cleanup;
			}
			printf("Object: %s\n", token.text);
		} else if (OBJZ_STRICMP(token.text, "usemtl") == 0) {
			objz_tokenize(&lexer, &token, false);
			if (token.text[0] == 0) {
				snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Expected name after 'usemtl'", token.line, token.column);
				goto cleanup;
			}
			printf("Use material: %s\n", token.text);
		} else if (OBJZ_STRICMP(token.text, "v") == 0 || OBJZ_STRICMP(token.text, "vn") == 0 || OBJZ_STRICMP(token.text, "vt") == 0) {
			int n = 3;
			if (OBJZ_STRICMP(token.text, "vt") == 0)
				n = 2;
			float vertex[3];
			if (!objz_parseFloats(&lexer, vertex, n))
				goto cleanup;
			/*if (OBJZ_STRICMP(token.text, "v") == 0)
				printf("Vertex: %g %g %g\n", vertex[0], vertex[1], vertex[2]);
			else if (OBJZ_STRICMP(token.text, "vn") == 0)
				printf("Normal: %g %g %g\n", vertex[0], vertex[1], vertex[2]);
			else if (OBJZ_STRICMP(token.text, "vt") == 0)
				printf("Texcoord: %g %g\n", vertex[0], vertex[1]);*/
		}
		objz_skipLine(&lexer);
	}
	output = malloc(sizeof(objzOutput));
	memset(output, 0, sizeof(*output));
	output->materials = (objzMaterial *)materials.data;
	output->numMaterials = (uint32_t)materials.length;
cleanup:
	free(buffer);
	return output;
}

void objz_destroy(objzOutput *_output) {
	if (!_output)
		return;
	free(_output->materials);
	free(_output);
}

const char *objz_getError() {
	return s_error;
}
