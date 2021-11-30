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


void populateTreeStructure(FreqTree *tree, unsigned char **treeStructure, unsigned char *offset, FILE *file) {
	if(!isLeaf(tree)) {
		**treeStructure |= 1 << *offset;
	} else {
		putc(tree->data, file);
	}

	// Check if current item is full
	if(*offset >= 1 << 7) {
		(*treeStructure)++;
		*offset = 0;
	} else {
		(*offset)++;
	}

	if(!isLeaf(tree)) {
		populateTreeStructure(tree->lhs, treeStructure, offset, file);
		populateTreeStructure(tree->rhs, treeStructure, offset, file);
	}
}

unsigned char *makeTreeStructure(FreqTree *tree, int count, FILE *file) {
	// bitfield for the structure of the tree
	unsigned char *treeStructure = malloc(sizeof(unsigned char) * count/8);

	unsigned char offset = 0, *treeStructurePtr = treeStructure;

	// FIXME: Possible buffer overflow
	populateTreeStructure(tree, &treeStructurePtr, &offset, file);

	return treeStructure;
}

void compressFreqTreeToFile(FreqTree *tree, int count, FILE *file) {
	// Magic number
	fputs("HZ", file);

	int structureLen = count/8 + 1;

	fseek(file, structureLen, SEEK_CUR);

	unsigned char *treeStructure = makeTreeStructure(tree, count, file);

	// Go back and write the missing bytes
	fseek(file, 2, SEEK_SET);
	
	fwrite(treeStructure, sizeof(char), structureLen, file);

	free(treeStructure);
}

int main(int argc, char *argv[]) {
	FreqTree *l1 = makeFreqTreeLeaf('f');
	FreqTree *l2 = makeFreqTreeLeaf('u');
	FreqTree *l3 = makeFreqTreeLeaf('c');
	FreqTree *l4 = makeFreqTreeLeaf('k');
	FreqTree *l5 = makeFreqTreeLeaf('!');

	FreqTree *n1 = makeFreqTreeNode(l3, l4);
	FreqTree *n2 = makeFreqTreeNode(n1, l5);
	FreqTree *n3 = makeFreqTreeNode(l1, l2);
	FreqTree *n4 = makeFreqTreeNode(n3, n2);

	FILE *file = fopen("a.hz", "w");
	
	compressFreqTreeToFile(n4, 9, file);

	fclose(file);

	cleanFreqTree(n4);
}
