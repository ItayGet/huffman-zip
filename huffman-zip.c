#include <stdio.h>
#include <stdbool.h>

// The tree is used to build the internal frequency tree in a huffman encoding
// An internal node does not have data so if lhs is not null accessing data is
// prohibited
typedef struct FreqTree {
	// NULL if leaf
	struct FreqTree *lhs;

	union {
		struct FreqTree *rhs;
		char data;
	};

} FreqTree;

#define isLeaf(t) ((t)->lhs==NULL)


int main(int argc, char *argv[]) {
}
