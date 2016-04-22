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

#include <string.h>

#include "szl.h"

static enum szl_res szl_math_proc_calc(struct szl_interp *interp,
                                       const int objc,
                                       struct szl_obj **objv,
                                       struct szl_obj **ret)
{
	const char *op;
	szl_double n, m;
	enum szl_res res;

	op = szl_obj_str(objv[2], NULL);
	if (!op)
		return SZL_ERR;

	res = szl_obj_double(objv[1], &n);
	if (res != SZL_OK)
		return res;

	res = szl_obj_double(objv[3], &m);
	if (res != SZL_OK)
		return res;

	*ret = NULL;
	if (strcmp("+", op) == 0)
		szl_set_result_double(interp, ret, m + n);
	else if (strcmp("-", op) == 0)
		szl_set_result_double(interp, ret, m - n);
	else if (strcmp("*", op) == 0)
		szl_set_result_double(interp, ret, m * n);
	else if (strcmp("/", op) == 0)
		szl_set_result_double(interp, ret, m / n);
	else
		szl_usage(interp, ret, objv[0]);

	if (!*ret)
		return SZL_ERR;

	return SZL_OK;
}

enum szl_res szl_init_math(struct szl_interp *interp)
{
	if (!szl_new_proc(interp,
	                  "calc",
	                  4,
	                  4,
	                  "calc obj op obj",
	                  szl_math_proc_calc,
	                  NULL,
	                  NULL,
	                  NULL))
		return SZL_ERR;

	return SZL_OK;
}
