#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "objzero.h"

#ifdef _MSC_VER
#define OBJZ_STRICMP _stricmp
#else
#include <strings.h>
#define OBJZ_STRICMP strcasecmp
#endif

#ifndef OBJZ_DEBUG_ASSERT
#define OBJZ_DEBUG_ASSERT assert
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
	OBJZ_DEBUG_ASSERT(_lexer);
	return (_lexer->buf[0] == '\n' || (_lexer->buf[0] == '\r' && _lexer->buf[1] != '\n'));
}

static bool objz_isEof(const Lexer *_lexer) {
	OBJZ_DEBUG_ASSERT(_lexer);
	return (_lexer->buf[0] == 0);
}

static bool objz_isWhitespace(const Lexer *_lexer) {
	OBJZ_DEBUG_ASSERT(_lexer);
	return (_lexer->buf[0] == ' ' || _lexer->buf[0] == '\t' || _lexer->buf[0] == '\r');
}

static void objz_skipLine(Lexer *_lexer) {
	OBJZ_DEBUG_ASSERT(_lexer);
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
	OBJZ_DEBUG_ASSERT(_lexer);
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
	OBJZ_DEBUG_ASSERT(_lexer);
	OBJZ_DEBUG_ASSERT(_token);
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
	OBJZ_DEBUG_ASSERT(_lexer);
	OBJZ_DEBUG_ASSERT(_result);
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
	OBJZ_DEBUG_ASSERT(_lexer);
	OBJZ_DEBUG_ASSERT(_result);
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
	OBJZ_DEBUG_ASSERT(_lexer);
	OBJZ_DEBUG_ASSERT(_face);
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
		char *triplet = strtok(token.text, delim);
		if (!triplet)
			goto error;
		_face[i * 3 + 0] = atoi(triplet);
		// vt (optional)
		triplet = strtok(NULL, delim);
		_face[i * 3 + 1] = triplet ? atoi(triplet) : INT_MAX;
		// vn (optional)
		triplet = strtok(NULL, delim);
		_face[i * 3 + 2] = triplet ? atoi(triplet) : INT_MAX;
		(*_n)++;
	}
	return true;
error:
	snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Failed to parse face", token.line, token.column);
	return false;
}

static char *objz_readFile(const char *_filename) {
	FILE *f = fopen(_filename, "rb");
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
	{ "d", OBJZ_MAT_TOKEN_FLOAT, offsetof(Material, d), 1 },
	{ "illum", OBJZ_MAT_TOKEN_INT, offsetof(Material, illum), 1 },
	{ "Ka", OBJZ_MAT_TOKEN_FLOAT, offsetof(Material, Ka), 3 },
	{ "Kd", OBJZ_MAT_TOKEN_FLOAT, offsetof(Material, Kd), 3 },
	{ "Ke", OBJZ_MAT_TOKEN_FLOAT, offsetof(Material, Ke), 3 },
	{ "Ks", OBJZ_MAT_TOKEN_FLOAT, offsetof(Material, Ks), 3 },
	{ "Ni", OBJZ_MAT_TOKEN_FLOAT, offsetof(Material, Ni), 1 },
	{ "Ns", OBJZ_MAT_TOKEN_FLOAT, offsetof(Material, Ns), 1 },
	{ "map_Bump", OBJZ_MAT_TOKEN_STRING, offsetof(Material, map_Bump), 1 },
	{ "map_Ka", OBJZ_MAT_TOKEN_STRING, offsetof(Material, map_Ka), 1 },
	{ "map_Kd", OBJZ_MAT_TOKEN_STRING, offsetof(Material, map_Kd), 1 }
};

static void objz_printMaterial(const Material *_mat) {
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

static void objz_initMaterial(Material *_mat) {
	memset(_mat, 0, sizeof(*_mat));
}

static int objz_loadMaterial(const char *_objFilename, const char *_materialName) {
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
		strncat(filename, _materialName, sizeof(filename));
	} else
		strncpy(filename, _materialName, sizeof(filename));
	int result = -1;
	char *buffer = objz_readFile(filename);
	if (!buffer)
		goto cleanup;
	Lexer lexer;
	objz_initLexer(&lexer, buffer);
	Token token;
	Material mat;
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
				objz_printMaterial(&mat);
			objz_initMaterial(&mat);
			strncpy(mat.name, token.text, OBJZ_NAME_MAX);
		} else {
			for (int i = 0; i < sizeof(s_materialTokens) / sizeof(s_materialTokens[0]); i++) {
				const MaterialTokenDef *mtd = &s_materialTokens[i];
				uint8_t *dest = &((uint8_t *)&mat)[mtd->offset];
				if (OBJZ_STRICMP(token.text, mtd->name) == 0) {
					if (mtd->type == OBJZ_MAT_TOKEN_STRING) {
						objz_tokenize(&lexer, &token, false);
						if (token.text[0] == 0) {
							snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Expected name after '%s'", token.line, token.column, mtd->name);
							goto cleanup;
						}
						strncpy((char *)dest, token.text, OBJZ_NAME_MAX);
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
		objz_printMaterial(&mat);
	result = 1;
cleanup:
	free(buffer);
	return result;
}

int objz_load(const char *_filename) {
	int result = -1;
	char *buffer = objz_readFile(_filename);
	if (!buffer)
		goto cleanup;
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
			printf("Material lib: %s\n", token.text);
			if (!objz_loadMaterial(_filename, token.text))
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
			if (!objz_parseFloats(&lexer, vertex, 2))
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
	result = 1;
cleanup:
	free(buffer);
	return result;
}

const char *objz_getError()
{
	return s_error;
}
