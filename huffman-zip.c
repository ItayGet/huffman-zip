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
		unsigned char data;
	};
} FreqTree;

#define isLeaf(t) ((t)->lhs==NULL)

FreqTree *makeFreqTreeLeaf(unsigned char data) {
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

#define HEAP_CAP (UCHAR_MAX + 1)

typedef struct {
	FreqTreeHeapNode heap[HEAP_CAP];
	size_t size;
} FreqTreeHeap;

#define GET_INDEX_HEAP(treeheap, index) ((treeheap).heap[index])

#define EXCHANGE_NODES_HEAP(heap, index1, index2)\
	do {\
		FreqTreeHeapNode tmp = GET_INDEX_HEAP((heap), (index1));\
		GET_INDEX_HEAP((heap), (index1)) = GET_INDEX_HEAP((heap), (index2));\
		GET_INDEX_HEAP((heap), (index2)) = tmp;\
	} while(0);

// Swin an index up the heap
void swimFreqTreeHeap(FreqTreeHeap *heap, int index) {
	if (index <= 1) {
		// We have nowhere to swim to
		return;
	}

	int parent = (index-1) / 2;

	// Specifies the heap invariant
	if(GET_INDEX_HEAP(*heap, index).val >=
	   GET_INDEX_HEAP(*heap, parent).val) { return; }

	EXCHANGE_NODES_HEAP(*heap, index, parent);
	
	swimFreqTreeHeap(heap, parent);
}

// Sink an index down the heap
void sinkFreqTreeHeap(FreqTreeHeap *heap, int index) {
	// Find index of minimum child

	// Index has no child
	if (index >= heap->size / 2) {
		return;
	}
	
	int minChild = 2 * index + 1;
	// Make sure second child exists by checking minChild isn't the index
	// of the last item in the heap
	if(minChild != heap->size - 1) {
		// add 1 if the second child is smaller
		minChild += GET_INDEX_HEAP(*heap, 2 * index + 1).val >
			    GET_INDEX_HEAP(*heap, 2 * index + 2).val;
	}

	// Nothing left to do if the heap invariant is specified
	if(GET_INDEX_HEAP(*heap, index).val <=
	   GET_INDEX_HEAP(*heap, minChild).val) { return; }

	EXCHANGE_NODES_HEAP(*heap, index, minChild);
	
	sinkFreqTreeHeap(heap, minChild);
}

void heapify(FreqTreeHeap *heap) {
	for(int i = heap->size/2 - 1; i > 0; i--) {
		sinkFreqTreeHeap(heap, i);
	}
}

void enqueueFreqTreeHeap(FreqTreeHeap *heap, FreqTreeHeapNode *node) {
	assert(heap->size + 1 < HEAP_CAP);

	GET_INDEX_HEAP(*heap, heap->size) = *node;
	heap->size++;

	swimFreqTreeHeap(heap, heap->size - 1);
}

void dequeueFreqTreeHeap(FreqTreeHeap *heap, FreqTreeHeapNode *node) {
	*node = GET_INDEX_HEAP(*heap, 0);

	GET_INDEX_HEAP(*heap, 0) = GET_INDEX_HEAP(*heap, heap->size - 1);

	heap->size--;

	sinkFreqTreeHeap(heap, 0);
}

typedef struct {
	// Length in bits
	int length;

	unsigned long int bits;
} BitField;

void appendBitBitField(BitField *bf, bool bit) {
	assert(bf->bits < ULONG_MAX);

	bf->bits += bit << bf->length++;
}

bool popBitBitField(BitField *bf) {
	return bf->bits & 1 << bf->length--;
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

void insertEntryEncMap(EncMap *map, unsigned char key, BitField *value) {
	int hash = ENC_MAP_HASH(key);

	EncMapEntry *newEntry = malloc(sizeof(EncMapEntry)),
		    *prevEntry = map->entries[hash];
	map->entries[hash] = newEntry;
	newEntry->next = prevEntry;
	
	newEntry->key = key;
	newEntry->value = *value;
}

BitField *getEntryEncMap(EncMap *map, unsigned char key) {
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

// Returns the length of the last byte in bits
unsigned char closeBitFieldFile(BitFieldFile *bff) {
	if(bff->bufferLength != 0) {
		putc(bff->buffer, bff->file);
	}

	return bff->bufferLength;
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
		bf.bits >>= CHAR_BIT;
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
			fprintf(stderr, "Error in reading file: unexpected EOF\n");
		}

		bf->buffer = c;
	}
	return bf->buffer & (1 << bf->bufferBit++);
}

// ***************
// * Encode file *
// ***************

// Return value is count of nodes
int buildFreqTreeFromRawFile(FreqTree **tree, FILE *file) {
	int freqMap[UCHAR_MAX + 1] = {0};

	int c;
	while((c = getc(file)) != EOF) {
		freqMap[c]++;
	}

	// Put into a heap
	FreqTreeHeap heap;
	heap.size = 0;
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

	// Count of nodes starts with the size of the heap
	int count = heap.size;

	// Remove 2 minimum elements, combine their nodes and values and put
	// back into the heap
	while(heap.size > 1) {
		FreqTreeHeapNode node1;
		dequeueFreqTreeHeap(&heap, &node1);

		FreqTreeHeapNode *node2 = &GET_INDEX_HEAP(heap, 0);

		// Combine the first node into the first index in the heap
		FreqTree *lhs = node2->tree;
		node2->tree = malloc(sizeof(FreqTree));
		node2->tree->lhs = lhs;
		node2->tree->rhs = node1.tree;
		node2->val += node1.val;

		// Keep the heap invariant 
		sinkFreqTreeHeap(&heap, 0);

		// A new node is created so count is updated
		count++;
	}

	// Return the remaining tree
	*tree = GET_INDEX_HEAP(heap, 0).tree;

	return count;
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
	if(*offset >= 7) {
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
	unsigned char *treeStructure = malloc(sizeof(unsigned char) * count/8 + 1);

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

void addBitFieldToEncMap(FreqTree *tree, EncMap *map, BitField bf) {
	if(isLeaf(tree)) {
		insertEntryEncMap(map, tree->data, &bf);
		return;
	}

	// Recurse in lhs
 	appendBitBitField(&bf, false);
 	addBitFieldToEncMap(tree->lhs, map, bf);

	// Remove last bit
	popBitBitField(&bf);

	// Recurse in rhs
	appendBitBitField(&bf, true);
 	addBitFieldToEncMap(tree->rhs, map, bf);
}

// Converts a tree into a hash map in order for a lookup of a byte to its
// encoded forms to be linear on average
EncMap *getEncMapFromFreqTree(FreqTree *tree) {
	EncMap *map = malloc(sizeof(EncMap));
	makeSymbolTable(map);

	BitField bf;
	bf.bits = 0;
	bf.length = 0;

	addBitFieldToEncMap(tree, map, bf);

	return map;
}

#ifdef DEBUG_FUNCTIONS
void writeFreqTree(FreqTree *ft, FILE *file);
void writeEncMap(EncMap *em, FILE *file);
#endif

void encodeFile(FILE *input, FILE *output) {
	long curPosInp = ftell(input);

	FreqTree *tree;
	int count = buildFreqTreeFromRawFile(&tree, input);

	writeMetadataToFile(tree, count, output);

	// Leave room for bit field size and bits of last byte
	long bitFieldSizePos = ftell(output);
	fseek(output, sizeof(long) + sizeof(unsigned char), SEEK_CUR);

	long bitFieldFirstPos = bitFieldSizePos + sizeof(long) + sizeof(unsigned char);

	// Write encoded bits into output

	EncMap *map = getEncMapFromFreqTree(tree);

	BitFieldFile bff;
	makeBitFieldFile(&bff, output);

	// Seek file back 
	fseek(input, curPosInp, SEEK_SET);

#ifdef DEBUG_FUNCTIONS
	writeFreqTree(tree, stdout);
	//writeEncMap(map, stdout);
#endif

	int c;
	while((c = getc(input)) != EOF) {
		BitField *bf = getEntryEncMap(map, c);
		writeBitField(&bff, *bf);
	}

	unsigned char lastbitFieldSize = closeBitFieldFile(&bff);

	long bitFieldSize = ftell(output) - bitFieldFirstPos;

	cleanFreqTree(tree);
	cleanEncMap(map);

	// Go back and write bit field size and bits of last byte
	fseek(output, bitFieldSizePos, SEEK_SET);
	fwrite(&bitFieldSize, sizeof(long), 1, output);
	putc(lastbitFieldSize, output);
}

// ***************
// * Decode File *
// ***************

FreqTree *buildFreqTreeFromEncodedFile(BitFile *bf, FILE *dataFile) {
	FreqTree *node = malloc(sizeof(FreqTree));
	if(readBit(bf)) {
		node->lhs = buildFreqTreeFromEncodedFile(bf, dataFile);
		node->rhs = buildFreqTreeFromEncodedFile(bf, dataFile);

		return node;
	}

	node->lhs = NULL;
	node->data = getc(dataFile);

	return node;
}

void decodeFreqTree(BitFile *input, FILE *output, FreqTree *tree, long dataSizeBytes, char dataSizeBitsLastByte) {
	FreqTree *root = tree;
	char c;

	long currBytes = 1;
	char currBit = 1;

	while((currBytes < dataSizeBytes) || (currBytes == dataSizeBytes && currBit <= dataSizeBitsLastByte)) {
		tree = root;

		while(!isLeaf(tree)) {
			if(!readBit(input)) {
				tree = tree->lhs;
			} else {
				tree = tree->rhs;
			}

			currBit++;
			if(currBit >= 8) {
				currBytes++;
				currBit -= 8;
			}
		}

		c = tree->data;
		putc(c, output);
	}
}

void decodeFile(FILE *input, FILE *output) {
	char buf[2], magicNumber[] = { 'H', 'Z' };
	fread(buf, sizeof(char), 2, input);

	if(memcmp(buf, magicNumber, 2)) {
		fprintf(stderr, "File format not supported\n");
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

	FreqTree *ft = buildFreqTreeFromEncodedFile(&bf, dataFile);

	unsigned long bitFieldSize;
	fread(&bitFieldSize, sizeof(long), 1, dataFile);
	unsigned char dataSizeBitsLastByte = getc(dataFile);

	// Clear any half-read bits and use the position of dataFile
	makeBitFile(&bf, dataFile);

	decodeFreqTree(&bf, output, ft, bitFieldSize, dataSizeBitsLastByte);

	cleanFreqTree(ft);
}

// *******************
// * DEBUG FUNCTIONS *
// *******************

#ifdef DEBUG_FUNCTIONS
void writeFreqTreeNode(FreqTree *ft, FILE *file, char *name) {
	if(isLeaf(ft)) {
		char *fmtChar;
		bool isfmtCharMalloced = false;
		switch(ft->data) {
		case '"':
			fmtChar = "\\\"";
			break;
		case '\n':
			fmtChar = "\\n";
			break;
		case ' ':
			fmtChar = "space";
			break;
		case '\'':
			fmtChar = "\\'";
			break;
		case '\0':
			fmtChar = "\\\\0";
			break;
		default:
			fmtChar = malloc(sizeof(char) * 2);
			fmtChar[0] = ft->data;
			fmtChar[1] = '\0';
			isfmtCharMalloced = true;
		}
		fprintf(file, "\t\"%s\" [label=\"%s\\n%s\" shape=box]\n", name, fmtChar, name);

		if(isfmtCharMalloced) {
			free(fmtChar);
		}
		return;
	}

	const char *lhsC = "0";
	const char *rhsC = "1";
	unsigned long len = strlen(name);
	unsigned long lenLhs = strlen(lhsC);
	unsigned long lenRhs = strlen(rhsC);

	char *lhs = malloc(sizeof(char) * (len + lenLhs + 1));
	char *rhs = malloc(sizeof(char) * (len + lenRhs + 1));
	
	strcpy(lhs, lhsC);
	strcat(lhs, name);

	strcpy(rhs, rhsC);
	strcat(rhs, name);

	fprintf(file, "\t\"%s\" -> \"%s\"\n", name, lhs);
	fprintf(file, "\t\"%s\" -> \"%s\"\n", name, rhs);

	writeFreqTreeNode(ft->lhs, file, lhs);
	writeFreqTreeNode(ft->rhs, file, rhs);

	free(lhs);
	free(rhs);
}

// Write freqTree into a file in the VizGraph Format
void writeFreqTree(FreqTree *ft, FILE *file) {
	fputs("digraph {\n", file);

	writeFreqTreeNode(ft, file, "");

	fputs("}\n", file);
}

char *stringifyBitField(BitField *bf) {
	char *string = malloc(sizeof(char) * (bf->length + 1));
	for(int i = 0; i < bf->length; i++) {
		string[i] = bf->bits & 1 << i ? '1' : '0';
	}

	string[bf->length] = '\0';
	
	return string;
}

void writeEncMapEntry(EncMapEntry *eme, FILE *file) {
	if(eme == NULL) { return; }

	char *bitsStr = stringifyBitField(&eme->value);

	fprintf(file, "%c %s\n", eme->key, bitsStr);
	
	free(bitsStr);

	writeEncMapEntry(eme->next, file);
}

// Write the bits required to write a certain character to a file
void writeEncMap(EncMap *em, FILE *file) {
	for(int i = 0; i < SIZEOF_ARR(em->entries); i++) {
		writeEncMapEntry(em->entries[i], file);
	}
}
#endif

int main(int argc, char *argv[]) {
	FILE *input, *output;
	input = fopen("input.txt", "r"),
	output = fopen("temp.hz", "w");

	encodeFile(input, output);
	fclose(input);
	fclose(output);

	input = fopen("temp.hz", "r"),
	output = fopen("output.txt", "w");

	decodeFile(input, output);
	fclose(input);
	fclose(output);
}
