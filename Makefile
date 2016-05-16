# Copyright 2010 - 2015 Paul Chote
# This file is part of pixelshift, which is free software. It is made available to
# you under the terms of version 3 or later of the GNU General Public License, as
# published by the Free Software Foundation. For more information, see LICENSE.

CC ?= gcc
LINKER ?= gcc
CFLAGS = -g -c -Wall -pedantic --std=c99 -D_POSIX_C_SOURCE=200112L -D_BSD_SOURCE
LFLAGS = -lcfitsio -lm

ifeq ($(shell uname),Darwin)
	CFLAGS += -D_DARWIN_C_SOURCE -I/usr/local/include/
	LFLAGS += -L/usr/local/lib
endif

PS_SRC = pixelshift.c framedata.c fit.c
PS_OBJ = $(PS_SRC:.c=.o)

BS_SRC = backgroundsubtract.c framedata.c
BS_OBJ = $(BS_SRC:.c=.o)

all: pixelshift backgroundsubtract

pixelshift: $(PS_OBJ)
	$(LINKER) -o $@ $(PS_OBJ) $(LFLAGS)

backgroundsubtract: $(BS_OBJ)
	$(LINKER) -o $@ $(BS_OBJ) $(LFLAGS)

clean:
	-rm $(PS_OBJ) $(BS_OBJ) pixelshift backgroundsubtract

.SUFFIXES: .c
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
