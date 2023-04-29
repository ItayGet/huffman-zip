# Huffman zip
Compress files using Huffman Encoding and store them for later extraction.

# Usage
Taken directly from usage page

Usage: huffman-zip <subcommand>
Subcommands:
    e[ncode] <input> <output>
    d[ecode] <input> <output>
    help\tshows this help menu

whenever there is an <output> \"-\" can be used to mean stdin/stdout

# Building
Building the project is using the Makefile provided.
These are the recipes that could be used.
- all: Build project without debug symbols
- debug: Build project with debug symbols
- debug-funcs: Build project with debug symbols and output debug information
  about the frequency tree that's encoded
- clean: Clean residuals and object files

# Encoded file strcuture
The encoded files have a unique file structure:

###TODO###
