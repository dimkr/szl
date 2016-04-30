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

extern char **szl_split_lines(struct szl_interp *interp,
                              char *s,
                              const size_t len,
                              size_t *n);

int main()
{
	char buf[128];
	struct szl_interp *interp;
	struct szl_obj *out = NULL;
	char **argv;
	size_t argc;

	szl_test_begin();


	szl_test_set_name("two lines");
	interp = szl_interp_new();
	szl_assert(interp != NULL);
	if (interp) {
		strcpy(buf, "while 1 {puts 1}\nwhile 2 {puts 2}");
		argv = szl_split_lines(interp, buf, 33, &argc);
		if (out)
			szl_obj_unref(out);
		szl_assert(argv != NULL);
		szl_assert(argc == 2);
		if (argv) {
			if (argc == 2) {
				szl_assert(strcmp(argv[0], "while 1 {puts 1}") == 0);
				szl_assert(strcmp(argv[1], "while 2 {puts 2}") == 0);
			}
			free(argv);
		}
		szl_interp_free(interp);
	}

	szl_test_set_name("one idented line");
	interp = szl_interp_new();
	szl_assert(interp != NULL);
	if (interp) {
		strcpy(buf, "while 1 {puts 1} {\n" \
		            "\tputs a\n" \
		            "}");
		argv = szl_split_lines(interp, buf, 28, &argc);
		if (out)
			szl_obj_unref(out);
		szl_assert(argv != NULL);
		szl_assert(argc == 1);
		if (argv) {
			if (argc == 1)
				szl_assert(strcmp(argv[0], "while 1 {puts 1} {\n" \
		                                   "\tputs a\n" \
		                                   "}" ) == 0);
			free(argv);
		}
		szl_interp_free(interp);
	}

	szl_test_set_name("two idented lines");
	interp = szl_interp_new();
	szl_assert(interp != NULL);
	if (interp) {
		strcpy(buf, "while 1 {puts 1} {\n" \
		            "\tputs a\n" \
		            "\tputs b\n" \
		            "}");
		argv = szl_split_lines(interp, buf, 36, &argc);
		if (out)
			szl_obj_unref(out);
		szl_assert(argv != NULL);
		szl_assert(argc == 1);
		if (argv) {
			if (argc == 1)
				szl_assert(strcmp(argv[0], "while 1 {puts 1} {\n" \
		                                   "\tputs a\n" \
		                                   "\tputs b\n" \
		                                   "}" ) == 0);
			free(argv);
		}
		szl_interp_free(interp);
	}

	szl_test_set_name("multi-line string");
	interp = szl_interp_new();
	szl_assert(interp != NULL);
	if (interp) {
		strcpy(buf, "puts \"abc\n" \
		            "def\n" \
		            "ghi\"");
		argv = szl_split_lines(interp, buf, 18, &argc);
		if (out)
			szl_obj_unref(out);
		szl_assert(argv != NULL);
		szl_assert(argc == 1);
		if (argv) {
			if (argc == 1)
				szl_assert(strcmp(argv[0], "puts \"abc\n" \
		                                   "def\n" \
		                                   "ghi\"") == 0);
			free(argv);
		}
		szl_interp_free(interp);
	}

	szl_test_set_name("brackets");
	interp = szl_interp_new();
	szl_assert(interp != NULL);
	if (interp) {
		strcpy(buf, "puts [set a 5]");
		argv = szl_split_lines(interp, buf, 14, &argc);
		if (out)
			szl_obj_unref(out);
		szl_assert(argv != NULL);
		szl_assert(argc == 1);
		if (argv) {
			if (argc == 1)
				szl_assert(strcmp(argv[0], "puts [set a 5]") == 0);
			free(argv);
		}
		szl_interp_free(interp);
	}

	szl_test_set_name("idented brackets");
	interp = szl_interp_new();
	szl_assert(interp != NULL);
	if (interp) {
		strcpy(buf, "puts [ set a 5 ]");
		argv = szl_split_lines(interp, buf, 16, &argc);
		if (out)
			szl_obj_unref(out);
		szl_assert(argv != NULL);
		szl_assert(argc == 1);
		if (argv) {
			if (argc == 1)
				szl_assert(strcmp(argv[0], "puts [ set a 5 ]") == 0);
			free(argv);
		}
		szl_interp_free(interp);
	}

	szl_test_set_name("multi-line brackets");
	interp = szl_interp_new();
	szl_assert(interp != NULL);
	if (interp) {
		strcpy(buf, "puts [\nset a 5\n]");
		argv = szl_split_lines(interp, buf, 16, &argc);
		if (out)
			szl_obj_unref(out);
		szl_assert(argv != NULL);
		szl_assert(argc == 1);
		if (argv) {
			if (argc == 1)
				szl_assert(strcmp(argv[0], "puts [\nset a 5\n]") == 0);
			free(argv);
		}
		szl_interp_free(interp);
	}

	szl_test_end();

	return EXIT_SUCCESS;
}
