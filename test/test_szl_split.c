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

#include <szl.h>

#include "szl_test.h"

int main()
{
	char buf[128];
	struct szl_interp *interp;
	struct szl_obj *out = NULL;
	char **argv;
	int argc;

	szl_test_begin();

	szl_test_set_name("happy flow");
	interp = szl_interp_new();
	szl_assert(interp != NULL);
	if (interp) {
		strcpy(buf, "puts hello world");
		argv = szl_split(interp, buf, &argc, &out);
		if (out)
			szl_obj_unref(out);
		szl_assert(argv != NULL);
		szl_assert(argc == 3);
		if (argv) {
			if (argc == 3) {
				szl_assert(strcmp(argv[0], "puts") == 0);
				szl_assert(strcmp(argv[1], "hello") == 0);
				szl_assert(strcmp(argv[2], "world") == 0);
			}
			free(argv);
		}
		szl_interp_free(interp);
	}

	szl_test_set_name("leading space");
	interp = szl_interp_new();
	szl_assert(interp != NULL);
	if (interp) {
		strcpy(buf, " puts hello world");
		argv = szl_split(interp, buf, &argc, &out);
		if (out)
			szl_obj_unref(out);
		szl_assert(argv != NULL);
		szl_assert(argc == 3);
		if (argv) {
			if (argc == 3) {
				szl_assert(strcmp(argv[0], "puts") == 0);
				szl_assert(strcmp(argv[1], "hello") == 0);
				szl_assert(strcmp(argv[2], "world") == 0);
			}
			free(argv);
		}
		szl_interp_free(interp);
	}

	szl_test_set_name("trailing space");
	interp = szl_interp_new();
	szl_assert(interp != NULL);
	if (interp) {
		strcpy(buf, "puts hello world ");
		argv = szl_split(interp, buf, &argc, &out);
		if (out)
			szl_obj_unref(out);
		szl_assert(argv != NULL);
		szl_assert(argc == 3);
		if (argv) {
			if (argc == 3) {
				szl_assert(strcmp(argv[0], "puts") == 0);
				szl_assert(strcmp(argv[1], "hello") == 0);
				szl_assert(strcmp(argv[2], "world") == 0);
			}
			free(argv);
		}
		szl_interp_free(interp);
	}

	szl_test_set_name("leading tab");
	interp = szl_interp_new();
	szl_assert(interp != NULL);
	if (interp) {
		strcpy(buf, "\tputs hello world");
		argv = szl_split(interp, buf, &argc, &out);
		if (out)
			szl_obj_unref(out);
		szl_assert(argv != NULL);
		szl_assert(argc == 3);
		if (argv) {
			if (argc == 3) {
				szl_assert(strcmp(argv[0], "puts") == 0);
				szl_assert(strcmp(argv[1], "hello") == 0);
				szl_assert(strcmp(argv[2], "world") == 0);
			}
			free(argv);
		}
		szl_interp_free(interp);
	}

	szl_test_set_name("trailing tab");
	interp = szl_interp_new();
	szl_assert(interp != NULL);
	if (interp) {
		strcpy(buf, "puts hello world\t");
		argv = szl_split(interp, buf, &argc, &out);
		if (out)
			szl_obj_unref(out);
		szl_assert(argv != NULL);
		szl_assert(argc == 3);
		if (argv) {
			if (argc == 3) {
				szl_assert(strcmp(argv[0], "puts") == 0);
				szl_assert(strcmp(argv[1], "hello") == 0);
				szl_assert(strcmp(argv[2], "world") == 0);
			}
			free(argv);
		}
		szl_interp_free(interp);
	}

	szl_test_set_name("multiple spaces");
	interp = szl_interp_new();
	szl_assert(interp != NULL);
	if (interp) {
		strcpy(buf, "puts  hello   world");
		argv = szl_split(interp, buf, &argc, &out);
		if (out)
			szl_obj_unref(out);
		szl_assert(argv != NULL);
		szl_assert(argc == 3);
		if (argv) {
			if (argc == 3) {
				szl_assert(strcmp(argv[0], "puts") == 0);
				szl_assert(strcmp(argv[1], "hello") == 0);
				szl_assert(strcmp(argv[2], "world") == 0);
			}
			free(argv);
		}
		szl_interp_free(interp);
	}

	szl_test_set_name("multiple tabs");
	interp = szl_interp_new();
	szl_assert(interp != NULL);
	if (interp) {
		strcpy(buf, "puts\t\thello\t\t\tworld");
		argv = szl_split(interp, buf, &argc, &out);
		if (out)
			szl_obj_unref(out);
		szl_assert(argv != NULL);
		szl_assert(argc == 3);
		if (argv) {
			if (argc == 3) {
				szl_assert(strcmp(argv[0], "puts") == 0);
				szl_assert(strcmp(argv[1], "hello") == 0);
				szl_assert(strcmp(argv[2], "world") == 0);
			}
			free(argv);
		}
		szl_interp_free(interp);
	}

	szl_test_set_name("multiple tabs and spaces");
	interp = szl_interp_new();
	szl_assert(interp != NULL);
	if (interp) {
		strcpy(buf, "\t puts\t \thello\t \t \tworld \t ");
		argv = szl_split(interp, buf, &argc, &out);
		if (out)
			szl_obj_unref(out);
		szl_assert(argv != NULL);
		szl_assert(argc == 3);
		if (argv) {
			if (argc == 3) {
				szl_assert(strcmp(argv[0], "puts") == 0);
				szl_assert(strcmp(argv[1], "hello") == 0);
				szl_assert(strcmp(argv[2], "world") == 0);
			}
			free(argv);
		}
		szl_interp_free(interp);
	}

	szl_test_end();

	return EXIT_SUCCESS;
}
