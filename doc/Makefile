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

DOCS = szlsh.1 szl.html

TEMPLATES = $(wildcard *.in)
GENERATED = $(TEMPLATES:.in=) szl_builtin.c $(DOCS) api_doc

all: $(DOCS)

%.1: %.1.in
	sed -e s~@EXT_DIR@~$(EXT_DIR)~ -e s~@DOC_DIR@~$(DOC_DIR)~ $^ > $@

%.html: %.txt
	asciidoc -a toc -a toclevels=4 -o $@ $^

api_doc: doxygen.conf ../src/szl.h
	doxygen $^

install: all
	$(INSTALL) -D -m 644 szlsh.1 $(DESTDIR)/$(MAN_DIR)/szlsh.1
	$(INSTALL) -D -m 644 szl.html $(DESTDIR)/$(DOC_DIR)/szl/szl.html
	$(INSTALL) -m 644 ../README $(DESTDIR)/$(DOC_DIR)/szl/README
	$(INSTALL) -m 644 ../AUTHORS $(DESTDIR)/$(DOC_DIR)/szl/AUTHORS
	$(INSTALL) -m 644 ../COPYING $(DESTDIR)/$(DOC_DIR)/szl/COPYING

clean:
	rm -rf $(GENERATED)
