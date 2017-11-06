# Copyright 2010 - 2015 Paul Chote
# This file is part of pixelshift, which is free software. It is made available to
# you under the terms of version 3 or later of the GNU General Public License, as
# published by the Free Software Foundation. For more information, see LICENSE.

CC ?= gcc
LINKER ?= gcc
CFLAGS = -g -c -Wall -pedantic --std=c99 -D_POSIX_C_SOURCE=200112L -D_BSD_SOURCE -I/usr/include/cfitsio
LFLAGS = -lcfitsio -lm

ifeq ($(shell uname),Darwin)
	CFLAGS += -D_DARWIN_C_SOURCE -I/usr/local/include/
	LFLAGS += -L/usr/local/lib
endif

SRC = pixelshift.c framedata.c fit.c
OBJ = $(SRC:.c=.o)

RPMBUILD = rpmbuild --define "_topdir %(pwd)/build" \
        --define "_builddir %{_topdir}" \
        --define "_rpmdir %{_topdir}" \
        --define "_srcrpmdir %{_topdir}" \
        --define "_sourcedir %(pwd)"

rpm: pixelshift
	mkdir -p build
	${RPMBUILD} -ba onemetre-pixelshift.spec
	mv build/x86_64/*.rpm .
	rm -rf build

pixelshift: $(OBJ)
	$(LINKER) -o $@ $(OBJ) $(LFLAGS)

clean:
	-rm $(OBJ) pixelshift

.SUFFIXES: .c
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
