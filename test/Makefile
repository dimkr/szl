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

CFLAGS += -I../src

PROGS = test_szl_trim test_szl_split test_szl_split_lines
OBJECTS += szl.o

all: run_all

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

szl.o: ../src/szl.c
	$(CC) -c -o $@ $< $(CFLAGS) -DSZL_TEST

test_%: szl.o szl_test.o test_%.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS) $(ZLIB_LIBS)

run_all: $(PROGS)
	for i in $(PROGS); do ./$$i || exit 1; done

clean:
	rm -f $(PROGS) $(OBJECTS)
