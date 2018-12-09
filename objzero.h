#ifndef OBJZERO_H
#define OBJZERO_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

int objz_load(const char *_filename);
const char *objz_getError();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // OBJZERO_H
