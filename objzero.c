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
static uint32_t s_currentLine = 1;
static uint32_t s_currentColumn = 1;

struct token_s {
	char text[OBJZ_MAX_TOKEN_LENGTH];
	int line, column;
};

static bool objz_isEol(const char *_buffer) {
	OBJZ_DEBUG_ASSERT(_buffer);
	return (*_buffer == '\n' || (_buffer[0] == '\r' && _buffer[1] != '\n'));
}

static bool objz_isEof(const char *_buffer) {
	OBJZ_DEBUG_ASSERT(_buffer);
	return (*_buffer == 0);
}

static bool objz_isWhitespace(const char *_buffer) {
	OBJZ_DEBUG_ASSERT(_buffer);
	return (*_buffer == ' ' || *_buffer == '\t' || *_buffer == '\r');
}

static const char *objz_skipLine(const char *_buffer) {
	OBJZ_DEBUG_ASSERT(_buffer);
	for (;;) {
		if (objz_isEof(_buffer))
			break;
		if (objz_isEol(_buffer)) {
			s_currentColumn = 1;
			s_currentLine++;
			return _buffer + 1;
		}
		_buffer++;
		s_currentColumn++;
	}
	return _buffer;
}

static const char *objz_skipWhitespace(const char *_buffer) {
	OBJZ_DEBUG_ASSERT(_buffer);
	for (;;) {
		if (objz_isEof(_buffer))
			break;
		if (!objz_isWhitespace(_buffer))
			break;
		_buffer++;
		s_currentColumn++;
	}
	return _buffer;
}

static const char *objz_parseToken(const char *_buffer, struct token_s *_token) {
	OBJZ_DEBUG_ASSERT(_buffer);
	OBJZ_DEBUG_ASSERT(_token);
	int i = 0;
	_buffer = objz_skipWhitespace(_buffer);
	_token->line = s_currentLine;
	_token->column = s_currentColumn;
	for (;;) {
		if (objz_isEof(_buffer) || objz_isEol(_buffer) || objz_isWhitespace(_buffer))
			break;
		_token->text[i++] = *_buffer;
		_buffer++;
		s_currentColumn++;
	}
	_token->text[i] = 0;
	return _buffer;
}

static const char *objz_parseFloatArray(const char *_buffer, float *_result, int n) {
	OBJZ_DEBUG_ASSERT(_buffer);
	OBJZ_DEBUG_ASSERT(_result);
	OBJZ_DEBUG_ASSERT(n > 0);
	for (int i = 0; i < n; i++) {
		struct token_s token;
		_buffer = objz_parseToken(_buffer, &token);
		if (strlen(token.text) == 0) {
			snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Error parsing float array", token.line, token.column);
			return NULL;
		}
		_result[i] = (float)atof(token.text);
	}
	return _buffer;
}

static const char *objz_parseFace(const char *_buffer, int *_face, int *_n) {
	OBJZ_DEBUG_ASSERT(_buffer);
	OBJZ_DEBUG_ASSERT(_face);
	struct token_s token;
	*_n = 0;
	for (int i = 0; i < 4; i++) {
		_buffer = objz_parseToken(_buffer, &token);
		if (token.text[0] == 0) {
			if (i == 3 && objz_isEol(_buffer))
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
		_face[i * 3 + 1] = triplet ? atoi(triplet) : -1;
		// vn (optional)
		triplet = strtok(NULL, delim);
		_face[i * 3 + 2] = triplet ? atoi(triplet) : -1;
		(*_n)++;
	}
	return _buffer;
error:
	snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Failed to parse face", token.line, token.column);
	return NULL;
}

int objz_load(const char *_filename) {
	int result = -1;
	FILE *f = fopen(_filename, "rb");
	if (!f) {
		snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "Failed to open file '%s'", _filename);
		return result;
	}
	fseek(f, 0, SEEK_END);
	const size_t fileLength = (size_t)ftell(f);
	fseek(f, 0, SEEK_SET);
	if (fileLength <= 0) {
		fclose(f);
		return result;
	}
	char *buffer = malloc(fileLength + 1);
	const size_t bytesRead = fread(buffer, 1, fileLength, f);
	if (bytesRead < fileLength)
		goto cleanup;
	buffer[fileLength] = 0;
	const char *current = buffer;
	struct token_s token;
	for (;;) {
		current = objz_parseToken(current, &token);
		if (token.text[0] == 0) {
			if (objz_isEof(current))
				break;
		} else {
			if (token.text[0] == '#')
				current = objz_skipLine(current);
			else if (OBJZ_STRICMP(token.text, "f") == 0) {
				int face[4*3], numVerts;
				current = objz_parseFace(current, face, &numVerts);
				if (!current)
					goto cleanup;
				current = objz_skipLine(current);
				printf("Face (%d verts):\n", numVerts);
				for (int i = 0; i < numVerts; i++)
					printf("%d %d %d\n", face[i * 3 + 0], face[i * 3 + 1], face[i * 3 + 2]);
			}
			else if (OBJZ_STRICMP(token.text, "o") == 0) {
				current = objz_parseToken(current, &token);
				if (token.text[0] == 0) {
					snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Failed to parse object name", token.line, token.column);
					goto cleanup;
				}
				current = objz_skipLine(current);
				printf("Object: %s\n", token.text);
			}
			else if (OBJZ_STRICMP(token.text, "s") == 0) {
				current = objz_skipLine(current); // Ignore smoothing groups
			}
			else if (OBJZ_STRICMP(token.text, "v") == 0 || OBJZ_STRICMP(token.text, "vn") == 0 || OBJZ_STRICMP(token.text, "vt") == 0) {
				int n = 3;
				if (OBJZ_STRICMP(token.text, "vt") == 0)
					n = 2;
				float vertex[3];
				current = objz_parseFloatArray(current, vertex, n);
				if (!current)
					goto cleanup;
				current = objz_skipLine(current);
				if (OBJZ_STRICMP(token.text, "v") == 0)
					printf("Vertex: %g %g %g\n", vertex[0], vertex[1], vertex[2]);
				else if (OBJZ_STRICMP(token.text, "vn") == 0)
					printf("Normal: %g %g %g\n", vertex[0], vertex[1], vertex[2]);
				else if (OBJZ_STRICMP(token.text, "vt") == 0)
					printf("Texcoord: %g %g\n", vertex[0], vertex[1]);
			}
			else {
				snprintf(s_error, OBJZ_MAX_ERROR_LENGTH, "(%u:%u) Unknown token '%s'", token.line, token.column, token.text);
				goto cleanup;
			}
		}
	}
	result = 1;
cleanup:
	free(buffer);
	fclose(f);
	return result;
}

const char *objz_getError()
{
	return s_error;
}
