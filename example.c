#include <stdio.h>
#include "objzero.h"

int main(int argc, char **argv) {
	if (argc <= 1)
		return 0;
	if (objz_load(argv[1]) < 0)
		printf("ERROR: %s\n", objz_getError());
	return 0;
}
