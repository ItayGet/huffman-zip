#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>

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

typedef struct {
	// Length in bits
	int length;

	unsigned long int bits;
} BitField;

void AppendBitBitField(BitField *bf, bool bit) {
	assert(bf->bits < ULONG_MAX);

	bf->bits += bit << bf->length++;
}

#define BUCKETS 8

#define HASH(key) ((key)%BUCKETS)

typedef struct EncMapEntry {
	unsigned char key;
	BitField value;
	struct EncMapEntry *next;
} EncMapEntry;

// Hash map that is converted from a FreqTree in order to encode but not to
// decode
typedef struct {
	EncMapEntry *entries[BUCKETS];
} EncMap;

void makeSymbolTable(EncMap *em) {
	for(int i = 0; i < BUCKETS; i++) {
		em->entries[i] = NULL;
	}
}

void cleanEncMapEntry(EncMapEntry *eme) {
	if(!eme) { return; }

	cleanEncMapEntry(eme->next);
	free(eme);
}

void cleanEncMap(EncMap *em) {
	for(int i = 0; i < BUCKETS; i++) {
		cleanEncMapEntry(em->entries[i]);
	}
}

void insertEntryEncMap(EncMap *map, char key, BitField *value) {
	int hash = HASH(key);

	EncMapEntry *newEntry = malloc(sizeof(EncMapEntry)),
		    *prevEntry = map->entries[hash];
	map->entries[hash] = newEntry;
	newEntry->next = prevEntry;
	
	newEntry->key = key;
	newEntry->value = *value;
}

BitField *getEntryEncMap(EncMap *map, char key) {
	for(EncMapEntry *eme = map->entries[HASH(key)]; eme; eme = eme->next) {
		if(eme->key == key) {
			return &eme->value;
		}
	}

	return NULL;
}

// A file with a buffer that enables to write bitfields into files without padding
// After finishing writing to file use the closeBitFieldFile to finish writing the file
typedef struct {
	unsigned char buffer;

	// In bits
	unsigned char bufferLength;

	FILE *file;
} BitFieldFile;

void makeBitFieldFile(BitFieldFile *bff, FILE *file) {
	bff->bufferLength = 0;

	bff->file = file;
}

void closeBitFieldFile(BitFieldFile *bff) {
	putc(bff->buffer, bff->file);
}

void writeBitField(BitFieldFile *bff, BitField bf) {
	// Complete bits inside current buffer
	if(bff->bufferLength > 0) {
		bff->buffer += bf.bits << bff->bufferLength;

		int addedBits;
		if(CHAR_BIT - bff->bufferLength < bf.length) {
			addedBits = CHAR_BIT - bff->bufferLength;
		} else { 
			addedBits = bf.length;
		}

		bff->bufferLength += addedBits;
		bf.length -= addedBits;
		bf.bits >>= addedBits;

		if(bff->bufferLength == CHAR_BIT) {
			putc(bff->buffer, bff->file);
			bff->bufferLength = 0;
		}
	}

	// Write whole bytes
	while(bf.length / 8 > 0) {
		putc((unsigned char)bf.bits, bff->file);
		bf.bits <<= CHAR_BIT;
		bf.length -= CHAR_BIT;
	}

	// Store remaining bits in buffer
	if(bf.length > 0) {
		bff->buffer = bf.bits;
		bff->bufferLength = bf.length;
	}
}

// Walks preorder in the tree structure, builds up structure bits of the tree
// and writes data to file as per specification
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

// Prepare calling for populateTreeStructure
unsigned char *makeTreeStructure(FreqTree *tree, int count, FILE *file) {
	unsigned char *treeStructure = malloc(sizeof(unsigned char) * count/8);

	unsigned char offset = 0, *treeStructurePtr = treeStructure;

	// FIXME: Possible buffer overflow
	populateTreeStructure(tree, &treeStructurePtr, &offset, file);

	return treeStructure;
}

// Write any necessary metadata to file
// Handles writing structure bits to file and calling populateTreeStructure
void writeMetadataToFile(FreqTree *tree, int count, FILE *file) {
	// Magic number
	fputs("HZ", file);
	int structureLen = count/8 + 1;

	fseek(file, structureLen, SEEK_CUR);

	// This function writes all data at leaves and per specification the
	// data comes after tree structure so we seek before the function
	unsigned char *treeStructure = makeTreeStructure(tree, count, file);

	// Go back and write the missing bytes
	fseek(file, 2, SEEK_SET);
	
	fwrite(treeStructure, sizeof(char), structureLen, file);

	free(treeStructure);
}

void AddBitFieldToEncMap(FreqTree *tree, EncMap *map, BitField bf) {
	if(isLeaf(tree)) {
		insertEntryEncMap(map, tree->data, &bf);
		return;
	}

	AppendBitBitField(&bf, false);
	AddBitFieldToEncMap(tree->lhs, map, bf);
	bf.bits += 1 << (bf.length - 1);
	AddBitFieldToEncMap(tree->rhs, map, bf);
}

// Converts a tree into a hash map in order for a lookup of a byte to its
// encoded forms to be linear on average
EncMap *getEncMapFromFreqTree(FreqTree *tree) {
	EncMap *map = malloc(sizeof(EncMap));
	makeSymbolTable(map);

	BitField bf;
	bf.bits = 0;
	bf.length = 0;

	AddBitFieldToEncMap(tree, map, bf);

	return map;
}

int main(int argc, char *argv[]) {
	FreqTree *l1 = makeFreqTreeLeaf('f');
	FreqTree *l2 = makeFreqTreeLeaf('u');
	FreqTree *l3 = makeFreqTreeLeaf('c');
	FreqTree *l4 = makeFreqTreeLeaf('k');
	FreqTree *l5 = makeFreqTreeLeaf('\0');

	FreqTree *n1 = makeFreqTreeNode(l3, l4);
	FreqTree *n2 = makeFreqTreeNode(n1, l5);
	FreqTree *n3 = makeFreqTreeNode(l1, l2);
	FreqTree *n4 = makeFreqTreeNode(n3, n2);

	FILE *file = fopen("a.hz", "w");
	
	writeMetadataToFile(n4, 9, file);
	EncMap *map = getEncMapFromFreqTree(n4);

	char a[] = "fuckffucckk";
	
	BitFieldFile bff;
	makeBitFieldFile(&bff, file);

	for(int i = 0; i < sizeof(a)/sizeof(a[0]); i++) {
		BitField *bf = getEntryEncMap(map, a[i]);
		writeBitField(&bff, *bf);
		//printf("%c: bits %ld length %d\n", a[i], bf->bits, bf->length);
	}
	closeBitFieldFile(&bff);

	fclose(file);

	cleanFreqTree(n4);
	cleanEncMap(map);
}
