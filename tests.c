#include <stdio.h>
#include "objzero.c"

#define ASSERT(_condition) if (!(_condition)) printf("[FAIL] '%s' %s %d\n", #_condition, __FILE__, __LINE__);

int main(int argc, char **argv) {
	{
		printf("parseVertexAttribIndices\n");
		Token token;
		int32_t triplet[3];
		// optional texcoord and normal
		snprintf(token.text, sizeof(token.text), "1/2/3");
		ASSERT(parseVertexAttribIndices(&token, triplet));
		ASSERT(triplet[0] == 1);
		ASSERT(triplet[1] == 2);
		ASSERT(triplet[2] == 3);
		snprintf(token.text, sizeof(token.text), "1/2/");
		ASSERT(parseVertexAttribIndices(&token, triplet));
		ASSERT(triplet[0] == 1);
		ASSERT(triplet[1] == 2);
		ASSERT(triplet[2] == INT_MAX);
		snprintf(token.text, sizeof(token.text), "1/2");
		ASSERT(parseVertexAttribIndices(&token, triplet));
		ASSERT(triplet[0] == 1);
		ASSERT(triplet[1] == 2);
		ASSERT(triplet[2] == INT_MAX);
		snprintf(token.text, sizeof(token.text), "1//");
		ASSERT(parseVertexAttribIndices(&token, triplet));
		ASSERT(triplet[0] == 1);
		ASSERT(triplet[1] == INT_MAX);
		ASSERT(triplet[2] == INT_MAX);
		snprintf(token.text, sizeof(token.text), "1/");
		ASSERT(parseVertexAttribIndices(&token, triplet));
		ASSERT(triplet[0] == 1);
		ASSERT(triplet[1] == INT_MAX);
		ASSERT(triplet[2] == INT_MAX);
		snprintf(token.text, sizeof(token.text), "1");
		ASSERT(parseVertexAttribIndices(&token, triplet));
		ASSERT(triplet[0] == 1);
		ASSERT(triplet[1] == INT_MAX);
		ASSERT(triplet[2] == INT_MAX);
		snprintf(token.text, sizeof(token.text), "1//3");
		ASSERT(parseVertexAttribIndices(&token, triplet));
		ASSERT(triplet[0] == 1);
		ASSERT(triplet[1] == INT_MAX);
		ASSERT(triplet[2] == 3);
		// position isn't optional
		snprintf(token.text, sizeof(token.text), "/2/3");
		ASSERT(!parseVertexAttribIndices(&token, triplet));
		snprintf(token.text, sizeof(token.text), "/2/");
		ASSERT(!parseVertexAttribIndices(&token, triplet));
		snprintf(token.text, sizeof(token.text), "/2");
		ASSERT(!parseVertexAttribIndices(&token, triplet));
		snprintf(token.text, sizeof(token.text), "//3");
		ASSERT(!parseVertexAttribIndices(&token, triplet));
		snprintf(token.text, sizeof(token.text), "//");
		ASSERT(!parseVertexAttribIndices(&token, triplet));
		snprintf(token.text, sizeof(token.text), "/");
		ASSERT(!parseVertexAttribIndices(&token, triplet));
		snprintf(token.text, sizeof(token.text), "");
		ASSERT(!parseVertexAttribIndices(&token, triplet));
	}
	printf("Done\n");
	return 0;
}
