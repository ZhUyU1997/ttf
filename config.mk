PROG := ttf
VERSION := 0.0.0

MODULES := base tables glyph parse raster utils

TAGFILE := .tags

# C compiler
CC := gcc

# C compiler flags
CFLAGS := -Wall  -pedantic -std=c99 -I.

# C preprocessor
CPP := gcc

# C preprocessor flags
CPPFLAGS :=

# Link flags
LDFLAGS := -Wl,--as-needed

# Libraries
LDLIBS := -lm -lpng

# Dependency creation flags
DEPFLAGS := -MG -MP

# Release flags
CFLAGS.release := -O2 -flto
LDFLAGS.release := -O2 -flto

# Debug flags
CFLAGS.debug := -g -O0 -DDEBUG
LDFLAGS.debug := -O0
