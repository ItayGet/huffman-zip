#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#define SIZEOF_ARR(ARR) (sizeof(ARR)/sizeof((ARR)[0]))

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
	FreqTree *tree;
	int val;
} FreqTreeHeapNode;

typedef struct {
	FreqTreeHeapNode heap[UCHAR_MAX + 1];
	size_t size;
} FreqTreeHeap;

#define GET_INDEX_HEAP(treeheap, index) ((treeheap).heap[index])

// Swin an index up the heap
void swimFreqTreeHeap(FreqTreeHeap *heap, int index) {
	if (index <= 1) {
		// We have nowhere to swim to
		return;
	}

	int parent = (index-1) / 2;

	if(GET_INDEX_HEAP(*heap, index).val >=
	   GET_INDEX_HEAP(*heap, parent).val) { return; }

	// Exchange index and minChild
	FreqTreeHeapNode tmp = GET_INDEX_HEAP(*heap, index);
	GET_INDEX_HEAP(*heap, index) = GET_INDEX_HEAP(*heap, parent);
	GET_INDEX_HEAP(*heap, parent) = tmp;
	
	swimFreqTreeHeap(heap, parent);
}

// Sink an index down the heap
void sinkFreqTreeHeap(FreqTreeHeap *heap, int index) {
	// Find index of minimum child

	int minChild;
	if (heap->size <= 2 * index - 2) {
		// Whether index has a child
		return;
	}
	else if(heap->size <= 2 * index - 1) {
		minChild = 2 * index + 1;
	} else {
		// First calculate first child then add 1 if the second one is
		// smaller
		minChild = 2 * index + 1;
		minChild += GET_INDEX_HEAP(*heap, 2 * index + 1).val >
		            GET_INDEX_HEAP(*heap, 2 * index + 2).val;
	}

	if(GET_INDEX_HEAP(*heap, index).val <=
	   GET_INDEX_HEAP(*heap, minChild).val) { return; }

	// Exchange index and minChild
	FreqTreeHeapNode tmp = GET_INDEX_HEAP(*heap, index);
	GET_INDEX_HEAP(*heap, index) = GET_INDEX_HEAP(*heap, minChild);
	GET_INDEX_HEAP(*heap, minChild) = tmp;
	
	sinkFreqTreeHeap(heap, minChild);
}

void heapify(FreqTreeHeap *heap) {
	for(int i = heap->size/2 - 1; i > 0; i--) {
		sinkFreqTreeHeap(heap, i);
	}
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

#define ENC_MAP_BUCKETS 8

#define ENC_MAP_HASH(key) ((key)%ENC_MAP_BUCKETS)

typedef struct EncMapEntry {
	unsigned char key;
	BitField value;
	struct EncMapEntry *next;
} EncMapEntry;

// Hash map that is converted from a FreqTree in order to encode but not to
// decode
typedef struct {
	EncMapEntry *entries[ENC_MAP_BUCKETS];
} EncMap;

void makeSymbolTable(EncMap *em) {
	for(int i = 0; i < ENC_MAP_BUCKETS; i++) {
		em->entries[i] = NULL;
	}
}

void cleanEncMapEntry(EncMapEntry *eme) {
	if(!eme) { return; }

	cleanEncMapEntry(eme->next);
	free(eme);
}

void cleanEncMap(EncMap *em) {
	for(int i = 0; i < ENC_MAP_BUCKETS; i++) {
		cleanEncMapEntry(em->entries[i]);
	}
}

void insertEntryEncMap(EncMap *map, char key, BitField *value) {
	int hash = ENC_MAP_HASH(key);

	EncMapEntry *newEntry = malloc(sizeof(EncMapEntry)),
		    *prevEntry = map->entries[hash];
	map->entries[hash] = newEntry;
	newEntry->next = prevEntry;
	
	newEntry->key = key;
	newEntry->value = *value;
}

BitField *getEntryEncMap(EncMap *map, char key) {
	for(EncMapEntry *eme = map->entries[ENC_MAP_HASH(key)]; eme; eme = eme->next) {
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

// A struct that allows to read a single bit off files efficiently using a
// buffer
typedef struct {
	unsigned char buffer;

	unsigned char bufferBit;

	FILE *file;
} BitFile;

void makeBitFile(BitFile *bf, FILE *file) {
	bf->bufferBit = CHAR_BIT;

	bf->file = file;
}

bool readBit(BitFile *bf) {
	if(bf->bufferBit == CHAR_BIT) {
		bf->bufferBit = 0;

		int c = getc(bf->file);
		if(c == EOF) {
			fprintf(stderr, "Error in reading file: unexpected EOF");
		}

		bf->buffer = c;
	}
	return bf->buffer & (1 << bf->bufferBit++);
}

// *******************
// * Writing to file *
// *******************

FreqTree *buildFreqTreeFromFile(FILE *file) {
	int freqMap[UCHAR_MAX + 1] = {0};

	int c;
	while((c = getc(file)) != EOF) {
		freqMap[c]++;
	}

	// Put into a heap
	FreqTreeHeap heap;
	for(int i = 0; i < SIZEOF_ARR(freqMap); i++) {
		int val = freqMap[i];
		if(val != 0) {
			FreqTreeHeapNode *node = &heap.heap[heap.size++];

			node->val = val;
			node->tree = malloc(sizeof(FreqTree));
			node->tree->lhs = NULL;
			node->tree->data = i;
		}
	}

	heapify(&heap);

	// Remove 2 minimum elements, combine their nodes and values and put
	// back into the heap
	while(heap.size > 1) {
		FreqTreeHeapNode *minNode = &GET_INDEX_HEAP(heap, heap.size - 2);

		minNode[0].val += minNode[1].val;
		
		FreqTree *tree = minNode[0].tree;

		minNode[0].tree = malloc(sizeof(FreqTree));
		minNode[0].tree->lhs = tree;
		minNode[0].tree->rhs = minNode[1].tree;

		heap.size--;

		swimFreqTreeHeap(&heap, heap.size - 1);
	}

	// Return the remaining tree
	return GET_INDEX_HEAP(heap, 0).tree;
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

	fputc(structureLen, file);

	fseek(file, structureLen, SEEK_CUR);

	// This function writes all data at leaves and per specification the
	// data comes after tree structure so we seek before the function
	unsigned char *treeStructure = makeTreeStructure(tree, count, file);

	long int pos = ftell(file);

	// Go back and write the missing bytes
	fseek(file, 3-pos, SEEK_CUR);
	
	fwrite(treeStructure, sizeof(char), structureLen, file);

	// Seek back to end of file
	fseek(file, pos, SEEK_SET);

	free(treeStructure);
}

// ******************
// * Read from file *
// ******************

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

FreqTree *buildTreeFromFile(BitFile *bf, FILE *dataFile) {
	FreqTree *node = malloc(sizeof(FreqTree));
	if(readBit(bf)) {
		node->lhs = buildTreeFromFile(bf, dataFile);
		node->rhs = buildTreeFromFile(bf, dataFile);

		return node;
	}

	node->lhs = NULL;
	node->data = getc(dataFile);

	return node;
}

void decodeFile(BitFile *input, FILE *output, FreqTree *tree) {
	FreqTree *root = tree;
	char c;

	do {
		tree = root;

		while(!isLeaf(tree)) {
			if(!readBit(input)) {
				tree = tree->lhs;
			} else {
				tree = tree->rhs;
			}
		}

		c = tree->data;
		putc(c, output);
	} while(c != '\0');
}

void parseCompressedFile(FILE *input, FILE *output) {
	char buf[2], magicNumber[] = { 'H', 'Z' };
	fread(buf, sizeof(char), 2, input);

	if(memcmp(buf, magicNumber, 2)) {
		fprintf(stderr, "File format not supported");
		exit(1);
	}

	char treeStructureSize = getc(input);

	long filePos = ftell(input);

	// HACK: No standard way to duplicate file pointers
	FILE *dataFile = fdopen(dup(fileno(input)), "r");

	// Seek after tree structure to get to data
	// Use seek set because file position has to be set absolutely after
	// duplicating
	fseek(dataFile, filePos + treeStructureSize, SEEK_SET);

	BitFile bf;
	makeBitFile(&bf, input);

	FreqTree *ft = buildTreeFromFile(&bf, dataFile);

	// Clear any half-read bits and use the position of dataFile
	makeBitFile(&bf, dataFile);

	decodeFile(&bf, output, ft);

	cleanFreqTree(ft);
}

int main(int argc, char *argv[]) {
	//FreqTree *l1 = makeFreqTreeLeaf('f');
	//FreqTree *l2 = makeFreqTreeLeaf('u');
	//FreqTree *l3 = makeFreqTreeLeaf('c');
	//FreqTree *l4 = makeFreqTreeLeaf('k');
	//FreqTree *l5 = makeFreqTreeLeaf('\0');

	//FreqTree *n1 = makeFreqTreeNode(l3, l4);
	//FreqTree *n2 = makeFreqTreeNode(n1, l5);
	//FreqTree *n3 = makeFreqTreeNode(l1, l2);
	//FreqTree *n4 = makeFreqTreeNode(n3, n2);

	//FILE *file = fopen("a.hz", "w");
	//
	//writeMetadataToFile(n4, 9, file);
	//EncMap *map = getEncMapFromFreqTree(n4);

	//char a[] = "fuckffucckk";
	//
	//BitFieldFile bff;
	//makeBitFieldFile(&bff, file);

	//for(int i = 0; i < sizeof(a)/sizeof(a[0]); i++) {
	//	BitField *bf = getEntryEncMap(map, a[i]);
	//	writeBitField(&bff, *bf);
	//	//printf("%c: bits %ld length %d\n", a[i], bf->bits, bf->length);
	//}
	//closeBitFieldFile(&bff);

	//fclose(file);

	//cleanFreqTree(n4);
	//cleanEncMap(map);

	//file = fopen("a.hz", "r");

	//parseCompressedFile(file, stdout);

	//fclose(file);
	
	FILE *file = fopen("input.hz", "r");

	FreqTree *tree = buildFreqTreeFromFile(file);

	fclose(file);
}
