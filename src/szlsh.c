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
#include <stdio.h>

#include "szl.h"

static const char szlsh_inc[] = {
#include "szlsh.inc"
};

int main(int argc, char *argv[])
{
	struct szl_interp *interp;
	FILE *strm;
	const char *s;
	size_t len;
	enum szl_res res = SZL_ERR;

	interp = szl_interp_new();
	if (!interp)
		return SZL_ERR;

	switch (argc) {
		case 1:
			res = szl_run_const(interp, szlsh_inc, sizeof(szlsh_inc) - 1);
			break;

		case 2:
			res = szl_source(interp, argv[1]);
			break;

		case 3:
			if (strcmp("-c", argv[1]) == 0) {
				res = szl_run_const(interp, argv[2], strlen(argv[2]));
				break;
			}
	}

	if (res != SZL_EXIT) {
		s = szl_obj_str(interp->last, &len);
		strm = res == SZL_ERR ? stderr : stdout;
		if (s && len && (fwrite(s, 1, len, strm) > 0))
			fflush(strm);
	}

	szl_interp_free(interp);

	return ((res == SZL_OK) || (res == SZL_EXIT)) ? EXIT_SUCCESS : EXIT_FAILURE;
}
