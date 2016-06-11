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

SZL_VERSION = "0.1 $(shell git rev-parse HEAD)"

LDFLAGS += -L.
CFLAGS += -DSZL_VERSION=\"$(SZL_VERSION)\"

OPENSSL_CFLAGS = $(shell pkg-config --cflags openssl)
OPENSSL_LIBS = $(shell pkg-config --libs openssl)

LIBFFI_CFLAGS = $(shell pkg-config --cflags libffi)
LIBFFI_LIBS = $(shell pkg-config --libs libffi)

CURL_CFLAGS = $(shell pkg-config --cflags libcurl)
CURL_LIBS = $(shell pkg-config --libs libcurl)

LIBARCHIVE_CFLAGS = $(shell pkg-config --cflags libarchive)
LIBARCHIVE_LIBS = $(shell pkg-config --libs libarchive)

EXT_LIBS = -lszl

STATIC_LIBS = libszl.a ed25519.a
SHARED_LIB = libszl.so
PROGS = szlsh

BUILTIN_EXT_NAMES = obj proc str exc socket null logic io list math loop ext \
                    time env path

BUILTIN_EXT_SRCS = $(addsuffix .c,$(addprefix szl_,$(BUILTIN_EXT_NAMES)))
BUILTIN_EXT_OBJECTS = $(BUILTIN_EXT_SRCS:.c=.o)

EXTERNAL_EXT_NAMES = tls zlib dict ffi curl archive linenoise ed25519
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
		echo "extern int szl_init_$$i(struct szl_interp *interp);"; \
	done >> $@
	echo "enum szl_res szl_init_builtin_exts(struct szl_interp *interp) \
{" >> $@
	for i in $(BUILTIN_EXT_NAMES); \
	do \
		echo "	if (!szl_init_$$i(interp)) return SZL_ERR;"; \
	done >> $@
	echo "	return SZL_OK; \
}" >> $@

szl_conf.h: szl_conf.h.in
	sed s~@EXT_DIR@~$(EXT_DIR)~ $^ > $@

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

%.o: %.c szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS)

szl.o: szl.c szl.h
	$(CC) -c -o $@ $< $(CFLAGS) $(ZLIB_CFLAGS)

szl_str.o: szl_str.c szl_str.inc szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS)

szl_io.o: szl_io.c szl_io.inc szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS)

szl_loop.o: szl_loop.c szl_loop.inc szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS)

szlsh.o: szlsh.c szlsh.inc szl.h szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS)

libszl.a: szl.o szl_builtin.o $(BUILTIN_EXT_OBJECTS)
	$(AR) rcs $@ $^

libszl.so: szl.o szl_builtin.o $(BUILTIN_EXT_OBJECTS)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LIBS) $(ZLIB_LIBS)

szlsh: szlsh.o libszl.so
	$(CC) -o $@ $< $(LDFLAGS) -lszl

%.so: %.o $(SHARED_LIB)
	$(CC) -o $@ $< $(LDFLAGS) -shared $(EXT_LIBS) $(OPENSSL_LIBS)

szl_zlib.o: szl_zlib.c szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS) $(ZLIB_CFLAGS)

szl_dict.o: szl_dict.c szl_dict.inc szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS)

szl_tls.o: szl_tls.c szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS) $(OPENSSL_CFLAGS)

szl_tls.so: szl_tls.o $(SHARED_LIB)
	$(CC) -o $@ $< $(LDFLAGS) -shared $(EXT_LIBS) $(OPENSSL_LIBS)

szl_zlib.so: szl_zlib.o $(SHARED_LIB)
	$(CC) -o $@ $< $(LDFLAGS) -shared $(EXT_LIBS) $(ZLIB_LIBS)

szl_dict.so: szl_dict.o $(SHARED_LIB)
	$(CC) -o $@ $< $(LDFLAGS) -shared $(EXT_LIBS)

szl_ffi.o: szl_ffi.c szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS) $(LIBFFI_CFLAGS)

szl_ffi.so: szl_ffi.o $(SHARED_LIB)
	$(CC) -o $@ $< $(LDFLAGS) -shared $(EXT_LIBS) $(LIBFFI_LIBS)

szl_curl.o: szl_curl.c szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS) $(CURL_CFLAGS)

szl_curl.so: szl_curl.o $(SHARED_LIB)
	$(CC) -o $@ $< $(LDFLAGS) -shared $(EXT_LIBS) $(CURL_LIBS)

szl_archive.o: szl_archive.c szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS) $(LIBARCHIVE_CFLAGS)

szl_archive.so: szl_archive.o $(SHARED_LIB)
	$(CC) -o $@ $< $(LDFLAGS) -shared $(EXT_LIBS) $(LIBARCHIVE_LIBS)

szl_linenoise.so: szl_linenoise.o linenoise/linenoise.o $(SHARED_LIB)
	$(CC) -o $@ \
	      szl_linenoise.o linenoise/linenoise.o \
	      $(LDFLAGS) -shared \
	      $(EXT_LIBS)

libed25519.a: ed25519/src/add_scalar.o \
              ed25519/src/fe.o \
              ed25519/src/ge.o \
              ed25519/src/key_exchange.o \
              ed25519/src/keypair.o \
              ed25519/src/sc.o \
              ed25519/src/seed.o \
              ed25519/src/sha512.o \
              ed25519/src/sign.o \
              ed25519/src/verify.o
	$(AR) rcs $@ $^

szl_ed25519.so: szl_ed25519.o libed25519.a $(SHARED_LIB)
	$(CC) -o $@ $< $(LDFLAGS) -shared $(EXT_LIBS) -led25519

install: all
	$(INSTALL) -D -m 755 szlsh $(DESTDIR)/$(BIN_DIR)/szlsh
	$(INSTALL) -D -m 644 $(SHARED_LIB) $(DESTDIR)/$(LIB_DIR)/$(SHARED_LIB)
	$(INSTALL) -D -d -m 755 $(DESTDIR)/$(EXT_DIR)
	for i in $(EXTERNAL_EXT_LIBS); \
	do \
		$(INSTALL) -m 755 $$i $(DESTDIR)/$(EXT_DIR)/$$i; \
	done

clean:
	rm -rf $(EXTERNAL_EXT_LIBS) $(PROGS) $(SHARED_LIB) $(STATIC_LIBS) \
	       $(OBJECTS) $(GENERATED)
