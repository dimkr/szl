/*
 * this file is part of szl.
 *
 * Copyright (c) 2016 Dima Krasner
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include "szl_test.h"

extern void szl_trim(char *s, char **start, char **end);

int main()
{
	char buf[64];
	char *start = NULL, *end = NULL;

	szl_test_begin();

	szl_test_set_name("happy flow");
	strcpy(buf, "hello world");
	szl_trim(buf, &start, &end);
	szl_assert(start == buf);
	szl_assert(end == start + 11);
	szl_assert(strcmp(start, "hello world") == 0);

	szl_test_set_name("leading whitespace");
	strcpy(buf, " hello world");
	szl_trim(buf, &start, &end);
	szl_assert(start == buf + 1);
	szl_assert(end == start + 11);
	szl_assert(strcmp(start, "hello world") == 0);

	szl_test_set_name("trailing whitespace");
	strcpy(buf, "hello world ");
	szl_trim(buf, &start, &end);
	szl_assert(start == buf);
	szl_assert(end == start + 11);
	szl_assert(strcmp(start, "hello world") == 0);

	szl_test_set_name("leading and trailing whitespace");
	strcpy(buf, " hello world ");
	szl_trim(buf, &start, &end);
	szl_assert(start == buf + 1);
	szl_assert(end == start + 11);
	szl_assert(strcmp(start, "hello world") == 0);

	szl_test_set_name("more leading and trailing whitespace");
	strcpy(buf, "  hello world  ");
	szl_trim(buf, &start, &end);
	szl_assert(start == buf + 2);
	szl_assert(end == start + 11);
	szl_assert(strcmp(start, "hello world") == 0);

	szl_test_set_name("even more leading and trailing whitespace");
	strcpy(buf, "        hello world   ");
	szl_trim(buf, &start, &end);
	szl_assert(start == buf + 8);
	szl_assert(end == start + 11);
	szl_assert(strcmp(start, "hello world") == 0);

	szl_test_end();

	return EXIT_SUCCESS;
}
