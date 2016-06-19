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

static
enum szl_res szl_math_proc_calc(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	const char *op;
	szl_int ni, mi;
	szl_double nd, md;

	op = szl_obj_str(interp, objv[2], NULL);
	if (!op)
		return SZL_ERR;

	if (!szl_obj_double(interp, objv[1], &md) ||
	    !szl_obj_double(interp, objv[3], &nd) ||
	    !szl_obj_int(interp, objv[1], &mi) ||
	    !szl_obj_int(interp, objv[1], &ni))
		return SZL_ERR;

	if (strcmp("+", op) == 0) {
		if ((md != (szl_double)mi) || (nd != (szl_double)ni))
			return szl_set_result_double(interp, md + nd);

		return szl_set_result_int(interp, mi + ni);
	}
	else if (strcmp("-", op) == 0) {
		if ((md != (szl_double)mi) || (nd != (szl_double)ni))
			return szl_set_result_double(interp, md - nd);

		return szl_set_result_int(interp, mi - ni);
	}
	else if (strcmp("*", op) == 0) {
		if ((md != (szl_double)mi) || (nd != (szl_double)ni))
			return szl_set_result_double(interp, md * nd);

		return szl_set_result_int(interp, mi * ni);
	}
	else if (strcmp("/", op) == 0) {
		if (nd == 0) {
			szl_set_result_str(interp, "division by 0", -1);
			return SZL_ERR;
		}

		if ((md != (szl_double)mi) || (nd != (szl_double)ni))
			return szl_set_result_double(interp, md / nd);

		return szl_set_result_int(interp, mi / ni);
	}

	return szl_usage(interp, objv[0]);
}

int szl_init_math(struct szl_interp *interp)
{
	return szl_new_proc(interp,
	                    "calc",
	                    4,
	                    4,
	                    "calc m op n",
	                    szl_math_proc_calc,
	                    NULL,
	                    NULL) ? 1 : 0;
}
