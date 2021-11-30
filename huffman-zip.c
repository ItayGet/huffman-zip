#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

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

FreqTree *makeFreqTreeLeaf(char data) {
	FreqTree *node = malloc(sizeof(FreqTree));
	node->lhs = NULL;
	node->data = data;

	return node;
}

FreqTree *makeFreqTreeNode(FreqTree *lhs, FreqTree *rhs) {
	FreqTree *node = malloc(sizeof(FreqTree));
	node->lhs = lhs;
	node->rhs = rhs;

	return node;
}

void cleanFreqTree(FreqTree *tree) {
	if(!isLeaf(tree)) {
		cleanFreqTree(tree->lhs);
		cleanFreqTree(tree->rhs);
	}

	free(tree);
}


void populateTreeStructure(FreqTree *tree, unsigned char **treeStructure, unsigned char *offset) {
	if(!isLeaf(tree)) {
		**treeStructure |= 1 << *offset;
	}

	// Check if current item is full
	if(*offset >= 1 << 7) {
		(*treeStructure)++;
		*offset = 0;
	} else {
		(*offset)++;
	}

	if(!isLeaf(tree)) {
		populateTreeStructure(tree->lhs, treeStructure, offset);
		populateTreeStructure(tree->rhs, treeStructure, offset);
	}
}

unsigned char *makeTreeStructure(FreqTree *tree, int count) {
	// bitfield for the structure of the tree
	unsigned char *treeStructure = malloc(sizeof(unsigned char) * count/8);

	unsigned char offset = 0, *treeStructurePtr = treeStructure;

	// FIXME: Possible buffer overflow
	populateTreeStructure(tree, &treeStructurePtr, &offset);

	return treeStructure;
}

int main(int argc, char *argv[]) {
	FreqTree *l1 = makeFreqTreeLeaf('f');
	FreqTree *l2 = makeFreqTreeLeaf('u');
	FreqTree *l3 = makeFreqTreeLeaf('c');
	FreqTree *l4 = makeFreqTreeLeaf('k');

	FreqTree *n1 = makeFreqTreeNode(l3, l4);
	FreqTree *n2 = makeFreqTreeNode(l2, n1);
	FreqTree *n3 = makeFreqTreeNode(l1, n2);
	
	unsigned char *treeStructure = makeTreeStructure(n3, 7);

	for(int i = 0; i <= 7/8; i++) {
		printf("%x\n", treeStructure[i]);
	}

	free(treeStructure);
	cleanFreqTree(n3);
}
