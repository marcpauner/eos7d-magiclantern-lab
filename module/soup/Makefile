
# Soup Engine film looks, as a module (name must be max 8 characters)
MODULE_NAME=soup

# The look engine, vendored from Soup Engine's src/engine. Only the files
# the camera develop path needs (same list as the Film-in-ML builds).
SOUP_ENGINE=engine
ENGINE_OBJS=se.o cr2.o jpeg_dec.o jpeg_enc.o look.o stock.o wb.o color.o grade.o lut.o
vpath %.c $(SOUP_ENGINE)

MODULE_OBJS=soup.o film_menu.o film_develop.o $(ENGINE_OBJS)
MODULE_CFLAGS=-I$(SOUP_ENGINE)

# include modules environment
include ../Makefile.modules

# ML builds with -ffast-math. The engine must not: its looks are meant to
# match the desktop build exactly, and under fast-math GCC 5.4 turns the
# repeated multiplies in grade.c into __powisf2, which the 7D autoexec
# does not export (the module would refuse to load).
$(ENGINE_OBJS): CFLAGS += -fno-fast-math
