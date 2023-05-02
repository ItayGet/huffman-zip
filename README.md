# Huffman zip
Compress files using Huffman Encoding and store them for later extraction.

# Usage
Taken directly from usage page
```
Usage: huffman-zip <subcommand>
Subcommands:
    e[ncode] <input> <output>
    d[ecode] <input> <output>
    help\tshows this help menu

whenever there is an <output> \"-\" can be used to mean stdin/stdout
```

# Building
Building the project is using the Makefile provided.
These are the recipes that could be used.
- all: Build project without debug symbols
- debug: Build project with debug symbols
- debug-funcs: Build project with debug symbols and output debug information
  about the frequency tree that's encoded
- clean: Clean residuals and object files

# Encoded file strcuture
The encoded files have a unique file structure.

## Specification Convention
It is represented by a number of groups of bytes which are first presented as a sequence and then explained.

### Groups
The names of the groups are dictated by the following rules.
- If they are surrounded by single-quotes(') they are treated as literals. These characters have to be found in the file. Their size is the length of the string time the length of a character.
- If they begin with a number, that is the size of a single member of an item in the group.
- If they contain an 's' that is used to indicate that this group is a size, probably of the group labeled by the letter that comes next.
- A signle character that represents identifies the group.
- If they end with a dash(-) this indicates that the group does not have a constant size.

#### Examples to groups
- 'Hello world': There needs to be a sequence of 11 bytes spelling out the words written in ascii.
- sT: The size of a group called t. This group is of size 1 byte (Because no number preceeded the group).
- 8t-: There is a sequence of an non-constant number of bytes. Each item in the group is 8 bytes in size

### Explanations
The explanations are given in the following format
```
<name_of_the_group>(<size_of_item>, <count_of_items>): <explanation>
```

count of items can be a refernce to the value of another group or a question-mark(?) if it is determined by a more complex operation.
It may also be omitted entirely if it is 1

## The Specification
### Groups
```
'HZ' sT t- c- 8sD sL d-
```
### Expalanations
- 'HZ'(1, 2): The magic number that makes sure the written file is a compressed Huffman file.
- sT(1): Size of the incoming treeeStrcuture(t-).
- t-(1, sT): The structure of the frquency tree without data formatted as a list of bits that is ibig-endian where every 1 denotes as node and a 0 denotes as a leaf in the tree. The tree is built in a pre-order fashion where inside a byt the first bit is the 0th bit. second bit is the 1st bit and so on. On where the data of the tree in the leaves comes from, see c-.
- c-(1, ?): the data of the frequency tree in a pre-order fashion. Is read with t-.
- 8sD(8): The amount of d-.
- sL(1): The amount of bits in the last byte of d-.
- d-(1, 8sD): The data of the compressed file encoded in the Huffman Encoding using the frequency tree from before. It is big-endian and the first bit is the first bit inside bytes.
