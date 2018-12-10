#ifndef OBJZERO_H
#define OBJZERO_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef OBJZ_NAME_MAX
#define OBJZ_NAME_MAX 64
#endif OBJZ_NAME_MAX

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
} Material;

int objz_load(const char *_filename);
const char *objz_getError();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // OBJZERO_H
