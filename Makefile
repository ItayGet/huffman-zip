CC = gcc

INCDIR = .
SRCDIR = .
OBJDIR = ./obj

CFLAGS = -I$(INCDIR)
LIBS = -lm

OBJ = $(EXECNAME).o

EXECNAME = huffman-zip

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

all: $(OBJDIR) $(EXECNAME)

$(EXECNAME): $(OBJ:%=$(OBJDIR)/%)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

$(OBJDIR): 
	mkdir $(OBJDIR)

.PHONY: debug clean

debug: CFLAGS += -g
debug: all

debug-funcs: CFLAGS += -DDEBUG_FUNCTIONS
debug-funcs: debug

clean:
	rm -f *~
	rm -rf $(OBJDIR)
