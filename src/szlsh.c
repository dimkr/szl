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
#include <locale.h>

#include "szl.h"

static const char szlsh_inc[] = {
#include "szlsh.inc"
};

int main(int argc, char *argv[])
{
	struct szl_interp *interp;
	enum szl_res res;
	szl_int code;
	int ret;

	setlocale(LC_ALL, "");

	interp = szl_new_interp(argc, argv);
	if (!interp)
		return SZL_ERR;

	res = szl_run(interp, szlsh_inc, sizeof(szlsh_inc) - 1);

	if (res == SZL_EXIT)
		ret = (szl_as_int(interp, interp->last, &code) &&
		       (code >= INT_MIN) &&
		       (code <= INT_MAX)) ? (int)code : EXIT_SUCCESS;
	else
		ret = (res == SZL_OK) ? EXIT_SUCCESS : EXIT_FAILURE;

	szl_free_interp(interp);
	return ret;
}
