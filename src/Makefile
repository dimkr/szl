# this file is part of szl.
#
# Copyright (c) 2016 Dima Krasner
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

include ../Makefile.common

LDFLAGS += -L.

EXT_LIBS = -lszl

STATIC_LIB = libszl.a
PROGS = szlsh

BUILTIN_EXT_NAMES = posix obj io math logic loop exc proc ext sugar
BUILTIN_EXT_SRCS = $(addsuffix .c,$(addprefix szl_,$(BUILTIN_EXT_NAMES)))
BUILTIN_EXT_OBJECTS = $(BUILTIN_EXT_SRCS:.c=.o)

EXTERNAL_EXT_NAMES = zlib
EXTERNAL_EXT_SRCS = $(addsuffix .c,$(addprefix szl_,$(EXTERNAL_EXT_NAMES)))
EXTERNAL_EXT_OBJECTS = $(EXTERNAL_EXT_SRCS:.c=.o)
EXTERNAL_EXT_LIBS = $(EXTERNAL_EXT_SRCS:.c=.so)

TEMPLATES = $(wildcard *.in)
SCRIPTS = $(wildcard *.szl)
GENERATED = $(TEMPLATES:.in=) $(SCRIPTS:.szl=.inc) szl_builtin.c

all: $(PROGS) $(EXTERNAL_EXT_LIBS)

szl_builtin.c:
	echo "#include \"szl.h\"" > $@
	for i in $(BUILTIN_EXT_NAMES); \
	do \
		echo "extern enum szl_res szl_init_$$i(struct szl_interp *interp);"; \
	done >> $@
	echo "enum szl_res szl_init_builtin_exts(struct szl_interp *interp) \
{ \
	enum szl_res res;" >> $@
	for i in $(BUILTIN_EXT_NAMES); \
	do \
		echo "	res = szl_init_$$i(interp); \
	if (res != SZL_OK) return res;"; \
	done >> $@
	echo "	return SZL_OK; \
}" >> $@

szl_conf.h: szl_conf.h.in
	sed s~@EXT_DIR@~$(EXT_DIR)~ $^ > $@

%.o: %.c szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS)

szl.o: szl.c szl.h
	$(CC) -c -o $@ $< $(CFLAGS) $(ZLIB_CFLAGS)

szlsh.o: szlsh.c szl.h szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS)

libszl.a: szl.o szl_builtin.o $(BUILTIN_EXT_OBJECTS)
	$(AR) rcs $@ $^

szlsh: szlsh.o libszl.a
	$(CC) -o $@ $< $(LDFLAGS) -lszl $(LIBS) $(ZLIB_LIBS)

szl_zlib.o: szl_zlib.c szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS) $(ZLIB_CFLAGS)

szl_zlib.so: szl_zlib.o libszl.a
	$(CC) -o $@ $^ $(LDFLAGS) -shared $(EXT_LIBS) $(ZLIB_LIBS)

%.inc: %.szl
	sed -e s~'^\t*'~~g \
	    -e s~'\\'~'\\\\'~g \
	    -e s~'"'~'\\"'~g \
	    -e s~'^'~'"'~g \
	    -e s~'$$'~'\\n" \\'~g \
	    $^ | \
	grep -v \
	     -e ^\"\# \
	     -e '^"\\n" \\' > $@; \
	echo \"\" >> $@

szl_sugar.o: szl_sugar.c szl_conf.h szl_sugar.inc
	$(CC) -c -o $@ $< $(CFLAGS)

install: all
	$(INSTALL) -D -m 755 szlsh $(DESTDIR)/$(BIN_DIR)/szlsh
	$(INSTALL) -D -d -m 755 $(DESTDIR)/$(EXT_DIR)
	for i in $(EXTERNAL_EXT_LIBS); \
	do \
		$(INSTALL) -m 755 $$i $(DESTDIR)/$(EXT_DIR)/$$i; \
	done

clean:
	rm -rf $(EXTERNAL_EXT_LIBS) $(PROGS) $(STATIC_LIB) $(OBJECTS) $(GENERATED)
