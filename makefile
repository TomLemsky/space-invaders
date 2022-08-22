# A simple Makefile for compiling small SDL projects

# set the compiler
CC := g++ #clang

# set the compiler flags
CFLAGS := `sdl2-config --libs --cflags` -ggdb3 -O0 -Wall -lSDL2_image -lm
# add header files here
HDRS := Emulator.h Machine.h reference.h

# add source files here
SRCS := main.cpp Emulator.cpp Machine.cpp reference.c #file-name.c

# generate names of object files
OBJS := $(SRCS:.c=.o)

# name of executable
EXEC := emulator #name your executable file

# default recipe
all: $(EXEC)

showfont: showfont.c Makefile
	$(CC) -o $@ $@.c $(CFLAGS) $(LIBS)

glfont: glfont.c Makefile
	$(CC) -o $@ $@.c $(CFLAGS) $(LIBS)

# recipe for building the final executable
$(EXEC): $(OBJS) $(HDRS)
	$(CC) -o $@ $(OBJS) $(CFLAGS)

# recipe for building object files
#$(OBJS): $(@:.o=.c) $(HDRS) Makefile
#    $(CC) -o $@ $(@:.o=.c) -c $(CFLAGS)

# recipe to clean the workspace

run: $(EXEC)
	./$(EXEC)

clean:
	rm -f $(EXEC) $(OBJS)

.PHONY: all clean
