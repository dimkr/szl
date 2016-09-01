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

CC ?= cc
AR ?= ar
CFLAGS ?= -O2 -pipe
LIBS ?=
LDFLAGS ?= -Wl,-s

CFLAGS += -std=gnu99 \
          -Wall \
          -pedantic \
          -Wno-overlength-strings \
          -D_GNU_SOURCE \
          -DSZL_VERSION=\"$(SZL_VERSION)\"
LDFLAGS += -L.
LIBS += -lm -ldl

ZLIB_CFLAGS = $(shell pkg-config --cflags zlib)
ZLIB_LIBS = $(shell pkg-config --libs zlib)

SZL_VERSION = "0.1 $(shell git rev-parse HEAD | cut -c 1-8)"

OPENSSL_CFLAGS = $(shell pkg-config --cflags openssl)
OPENSSL_LIBS = $(shell pkg-config --libs openssl)

LIBFFI_CFLAGS = $(shell pkg-config --cflags libffi)
LIBFFI_LIBS = $(shell pkg-config --libs libffi)

CURL_CFLAGS = $(shell pkg-config --cflags libcurl)
CURL_LIBS = $(shell pkg-config --libs libcurl)

LIBARCHIVE_CFLAGS = $(shell pkg-config --cflags libarchive)
LIBARCHIVE_LIBS = $(shell pkg-config --libs libarchive)

EXT_LIBS = -lszl
LIBSZL_CFLAGS =
LIBSZL_LIBS =
LIBSZL_DEPS =
SZLSH_LIBS =

STATIC_LIB = libszl.a
STATIC_LIBS = $(STATIC_LIB) liblinenoise.a libed25519.a liblzfse.a libzstd.a
SHARED_LIB = libszl.so
PROGS = szlsh

BUILTIN_EXT_NAMES = obj proc exec str exc socket null logic file dir io list \
                    math loop ext time env path signal poll timer dict szl
EXTERNAL_EXT_NAMES =

# SZL_LIB is the library szlsh and extensions are linked against
ifeq ($(STATIC_ALL), 1)
	SZL_LIB = $(STATIC_LIB)
	STATIC_TLS = 1
	STATIC_ZLIB = 1
	STATIC_TEST = 1
	STATIC_FFI = 1
	STATIC_CURL = 1
	STATIC_ARCHIVE = 1
	STATIC_LINENOISE = 1
	STATIC_ED25519 = 1
	STATIC_LZFSE = 1
	STATIC_ZSTD = 1
	STATIC_SERVER = 1
	STATIC_HTTP = 1
else
	SZL_LIB = $(SHARED_LIB)
endif

ifeq ($(STATIC_TLS), 1)
	BUILTIN_EXT_NAMES += tls
	LIBSZL_CFLAGS += $(OPENSSL_CFLAGS)
	LIBSZL_LIBS += $(OPENSSL_LIBS)
else
	EXTERNAL_EXT_NAMES += tls
endif

ifeq ($(STATIC_ZLIB), 1)
	BUILTIN_EXT_NAMES += zlib
	LIBSZL_CFLAGS += $(ZLIB_CFLAGS)
	LIBSZL_LIBS += $(ZLIB_LIBS)
else
	EXTERNAL_EXT_NAMES += zlib
endif

ifeq ($(STATIC_TEST), 1)
	BUILTIN_EXT_NAMES += test
else
	EXTERNAL_EXT_NAMES += test
endif

ifeq ($(STATIC_FFI), 1)
	BUILTIN_EXT_NAMES += ffi
	LIBSZL_CFLAGS += $(LIBFFI_CFLAGS)
	LIBSZL_LIBS += $(LIBFFI_LIBS)
else
	EXTERNAL_EXT_NAMES += ffi
endif

ifeq ($(STATIC_CURL), 1)
	BUILTIN_EXT_NAMES += curl
	LIBSZL_CFLAGS += $(CURL_CFLAGS)
	LIBSZL_LIBS += $(CURL_LIBS)
else
	EXTERNAL_EXT_NAMES += curl
endif

ifeq ($(STATIC_ARCHIVE), 1)
	BUILTIN_EXT_NAMES += archive
	LIBSZL_CFLAGS += $(LIBARCHIVE_CFLAGS)
	LIBSZL_LIBS += $(LIBARCHIVE_LIBS)
else
	EXTERNAL_EXT_NAMES += archive
endif

ifeq ($(STATIC_LINENOISE), 1)
	BUILTIN_EXT_NAMES += linenoise
	LIBSZL_DEPS += liblinenoise.a
	LIBSZL_LIBS += -llinenoise
else
	EXTERNAL_EXT_NAMES += linenoise
endif

ifeq ($(STATIC_ED25519), 1)
	BUILTIN_EXT_NAMES += ed25519
	LIBSZL_DEPS += libed25519.a
	LIBSZL_LIBS += -led25519
else
	EXTERNAL_EXT_NAMES += ed25519
endif

ifeq ($(STATIC_LZFSE), 1)
	BUILTIN_EXT_NAMES += lzfse
	LIBSZL_DEPS += liblzfse.a
	LIBSZL_LIBS += -llzfse
else
	EXTERNAL_EXT_NAMES += lzfse
endif

ifeq ($(STATIC_ZSTD), 1)
	BUILTIN_EXT_NAMES += zstd
	LIBSZL_DEPS += libzstd.a
	LIBSZL_LIBS += -lzstd
else
	EXTERNAL_EXT_NAMES += zstd
endif

ifeq ($(STATIC_SERVER), 1)
	BUILTIN_EXT_NAMES += server
else
	EXTERNAL_EXT_NAMES += server
endif

ifeq ($(STATIC_HTTP), 1)
	BUILTIN_EXT_NAMES += http
else
	EXTERNAL_EXT_NAMES += http
endif

ifeq ($(STATIC_ALL), 1)
	SZLSH_LIBS = $(LIBSZL_LIBS)
endif

BUILTIN_EXT_SRCS = $(addsuffix .c,$(addprefix szl_,$(BUILTIN_EXT_NAMES)))
BUILTIN_EXT_OBJECTS = $(BUILTIN_EXT_SRCS:.c=.o)

EXTERNAL_EXT_SRCS = $(addsuffix .c,$(addprefix szl_,$(EXTERNAL_EXT_NAMES)))
EXTERNAL_EXT_OBJECTS = $(EXTERNAL_EXT_SRCS:.c=.o)
EXTERNAL_EXT_LIBS = $(EXTERNAL_EXT_SRCS:.c=.so)

TEMPLATES = $(wildcard *.in)
SCRIPTS = $(wildcard *.szl)
GENERATED = $(TEMPLATES:.in=) $(SCRIPTS:.szl=.inc) szl_builtin.c

SRCS = $(wildcard *.c)
OBJECTS = $(SRCS:.c=.o)

all: $(PROGS) $(EXTERNAL_EXT_LIBS)

szl_builtin.c:
	echo "#include <stddef.h>" > $@
	echo "#include \"szl.h\"" >> $@
	for i in $(BUILTIN_EXT_NAMES); \
	do \
		echo "extern int szl_init_$$i(struct szl_interp *);"; \
	done >> $@
	echo "char *szl_builtin_exts[] = {" >> $@
	for i in $(BUILTIN_EXT_NAMES); \
	do \
		echo -n "\"$$i\", "; \
	done >> $@
	echo " \
NULL};" >> $@
	echo "int szl_init_builtin_exts(struct szl_interp *interp) \
{" >> $@
	for i in $(BUILTIN_EXT_NAMES); \
	do \
		echo "	if (!szl_init_$$i(interp)) return 0;"; \
	done >> $@
	echo "	return 1; \
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

szl.o: szl.c szl.h szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS) $(ZLIB_CFLAGS)

stream.o: stream.c szl.h szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS) $(ZLIB_CFLAGS)

szl_io.o: szl_io.c szl_io.inc szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS)

szl_dir.o: szl_dir.c szl_dir.inc szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS)

szl_szl.o: szl_szl.c szl_szl.inc szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS)

szlsh.o: szlsh.c szlsh.inc szl.h szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS)

$(STATIC_LIB): szl.o szl_builtin.o $(BUILTIN_EXT_OBJECTS) $(LIBSZL_DEPS)
	$(AR) rcs $@ $^

$(SHARED_LIB): szl.o szl_builtin.o $(BUILTIN_EXT_OBJECTS) $(LIBSZL_DEPS)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LIBS) $(LIBSZL_LIBS)

szlsh: szlsh.o $(SZL_LIB)
	$(CC) -o $@ $< $(LDFLAGS) -lszl $(LIBS) $(SZLSH_LIBS)

%.so: %.o $(SZL_LIB)
	$(CC) -o $@ $< $(LDFLAGS) -shared $(EXT_LIBS) $(OPENSSL_LIBS)

szl_zlib.o: szl_zlib.c szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS) $(ZLIB_CFLAGS)

szl_test.o: szl_test.c szl_test.inc szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS)

szl_server.o: szl_server.c szl_server.inc szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS)

szl_http.o: szl_http.c szl_http.inc szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS)

szl_tls.o: szl_tls.c szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS) $(OPENSSL_CFLAGS)

szl_tls.so: szl_tls.o $(SZL_LIB)
	$(CC) -o $@ $< $(LDFLAGS) -shared $(EXT_LIBS) $(OPENSSL_LIBS)

szl_zlib.so: szl_zlib.o $(SZL_LIB)
	$(CC) -o $@ $< $(LDFLAGS) -shared $(EXT_LIBS) $(ZLIB_LIBS)

szl_ffi.o: szl_ffi.c szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS) $(LIBFFI_CFLAGS)

szl_ffi.so: szl_ffi.o $(SZL_LIB)
	$(CC) -o $@ $< $(LDFLAGS) -shared $(EXT_LIBS) $(LIBFFI_LIBS)

szl_curl.o: szl_curl.c szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS) $(CURL_CFLAGS)

szl_curl.so: szl_curl.o $(SZL_LIB)
	$(CC) -o $@ $< $(LDFLAGS) -shared $(EXT_LIBS) $(CURL_LIBS)

szl_archive.o: szl_archive.c szl_conf.h
	$(CC) -c -o $@ $< $(CFLAGS) $(LIBARCHIVE_CFLAGS)

szl_archive.so: szl_archive.o $(SZL_LIB)
	$(CC) -o $@ $< $(LDFLAGS) -shared $(EXT_LIBS) $(LIBARCHIVE_LIBS)

liblinenoise.a: linenoise/linenoise.o
	$(AR) rcs $@ $^

szl_linenoise.so: szl_linenoise.o liblinenoise.a $(SZL_LIB)
	$(CC) -o $@ $< $(LDFLAGS) -shared $(EXT_LIBS) -llinenoise

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

szl_ed25519.so: szl_ed25519.o libed25519.a $(SZL_LIB)
	$(CC) -o $@ $< $(LDFLAGS) -shared $(EXT_LIBS) -led25519

liblzfse.a: lzfse/src/lzfse_decode.o \
            lzfse/src/lzfse_decode_base.o \
            lzfse/src/lzfse_encode.o \
            lzfse/src/lzfse_encode_base.o \
            lzfse/src/lzfse_fse.o \
            lzfse/src/lzvn_decode_base.o \
            lzfse/src/lzvn_encode_base.o
	$(AR) rcs $@ $^

szl_lzfse.so: szl_lzfse.o liblzfse.a $(SZL_LIB)
	$(CC) -o $@ $< $(LDFLAGS) -shared $(EXT_LIBS) -llzfse


zstd/lib/%.o: zstd/lib/%.c
	$(CC) -c -o $@ $< $(CFLAGS) -Izstd/lib/common -Izstd/lib

libzstd.a: zstd/lib/compress/huf_compress.o \
           zstd/lib/compress/zstd_compress.o \
           zstd/lib/compress/fse_compress.o \
           zstd/lib/compress/zbuff_compress.o \
           zstd/lib/common/zstd_common.o \
           zstd/lib/common/fse_decompress.o \
           zstd/lib/common/entropy_common.o \
           zstd/lib/common/xxhash.o \
           zstd/lib/decompress/zbuff_decompress.o \
           zstd/lib/decompress/zstd_decompress.o \
           zstd/lib/decompress/huf_decompress.o
	$(AR) rcs $@ $^

szl_zstd.so: szl_zstd.o libzstd.a $(SZL_LIB)
	$(CC) -o $@ $< $(LDFLAGS) -shared $(EXT_LIBS) -lzstd

install: all
	$(INSTALL) -D -m 755 szlsh $(DESTDIR)/$(BIN_DIR)/szlsh
	[ -f $(STATIC_LIB) ] && $(INSTALL) -D -m 644 \
	                        $(STATIC_LIB) \
	                        $(DESTDIR)/$(LIB_DIR)/$(STATIC_LIB) || :
	[ -f $(SHARED_LIB) ] && $(INSTALL) -D -m 644 \
	                        $(SHARED_LIB) \
	                        $(DESTDIR)/$(LIB_DIR)/$(SHARED_LIB) || :
	$(INSTALL) -D -d -m 755 $(DESTDIR)/$(EXT_DIR)
	for i in $(EXTERNAL_EXT_LIBS); \
	do \
		$(INSTALL) -m 755 $$i $(DESTDIR)/$(EXT_DIR)/$$i; \
	done

clean:
	rm -rf $(EXTERNAL_EXT_LIBS) $(PROGS) $(SZL_LIB) $(STATIC_LIBS) \
	       $(OBJECTS) $(GENERATED)
