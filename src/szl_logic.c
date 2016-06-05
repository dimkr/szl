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
enum szl_res szl_logic_proc_test(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv)
{
	const char *a, *b;
	const char *op;
	szl_double n, m;
	size_t alen, blen;

	op = szl_obj_str(objv[2], NULL);
	if (!op)
		return SZL_ERR;

	if (strcmp("==", op) == 0) {
		a = szl_obj_str(objv[1], &alen);
		if (!a)
			return SZL_ERR;

		b = szl_obj_str(objv[3], &blen);
		if (!b)
			return SZL_ERR;

		return szl_set_result_bool(interp,
		                           ((alen == blen) && (strcmp(a, b) == 0)));
	}
	else if (strcmp("!=", op) == 0) {
		a = szl_obj_str(objv[1], &alen);
		if (!a)
			return SZL_ERR;

		b = szl_obj_str(objv[3], &blen);
		if (!b)
			return SZL_ERR;

		return szl_set_result_bool(interp,
		                           ((alen != blen) || (strcmp(a, b) != 0)));
	}
	else if (strcmp(">", op) == 0) {
		if (!szl_obj_double(objv[1], &m) || !szl_obj_double(objv[3], &n))
			return SZL_ERR;

		return szl_set_result_bool(interp, (n > m));
	}
	else if (strcmp("<", op) == 0) {
		if (!szl_obj_double(objv[1], &m) || !szl_obj_double(objv[3], &n))
			return SZL_ERR;

		return szl_set_result_bool(interp, (n < m));
	}
	else if (strcmp(">=", op) == 0) {
		if (!szl_obj_double(objv[1], &m) || !szl_obj_double(objv[3], &n))
			return SZL_ERR;

		return szl_set_result_bool(interp, (n >= m));
	}
	else if (strcmp("<=", op) == 0) {
		if (!szl_obj_double(objv[1], &m) || !szl_obj_double(objv[3], &n))
			return SZL_ERR;

		return szl_set_result_bool(interp, (n <= m));
	}
	else if (strcmp("&&", op) == 0) {
		return szl_set_result_bool(interp,
		                           (szl_obj_istrue(objv[1]) &&
		                            szl_obj_istrue(objv[3])));
	}
	else if (strcmp("||", op) == 0) {
		return szl_set_result_bool(interp,
		                           (szl_obj_istrue(objv[1]) ||
		                            szl_obj_istrue(objv[3])));
	}
	else if (strcmp("^", op) == 0) {
		return szl_set_result_bool(interp,
		                           (szl_obj_istrue(objv[1]) ^
		                            szl_obj_istrue(objv[3])));
	}
	else
		return szl_usage(interp, objv[0]);

	return SZL_OK;
}

static
enum szl_res szl_logic_proc_not(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	return szl_set_result_bool(interp, szl_obj_isfalse(objv[1]));
}

static
enum szl_res szl_logic_proc_if(struct szl_interp *interp,
                               const int objc,
                               struct szl_obj **objv)
{
	const char *s;
	size_t len;

	switch (objc) {
		case 3:
			break;

		case 5:
			s = szl_obj_str(objv[3], NULL);
			if (!s)
				return SZL_ERR;

			if (strcmp("else", s) == 0)
				break;
			/* fall through */

		default:
			return szl_usage(interp, objv[0]);
	}

	if (szl_obj_istrue(objv[1])) {
		s = szl_obj_str(objv[2], &len);
		if (!s)
			return SZL_ERR;

		return szl_run_const(interp, s, len);
	}

	if (objc == 5) {
		s = szl_obj_str(objv[4], &len);
		if (!s)
			return SZL_ERR;

		return szl_run_const(interp, s, len);
	}

	return SZL_OK;
}

int szl_init_logic(struct szl_interp *interp)
{
	return ((szl_new_proc(interp,
	                      "test",
	                      4,
	                      4,
	                      "test obj op obj",
	                      szl_logic_proc_test,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "not",
	                      2,
	                      2,
	                      "not obj",
	                      szl_logic_proc_not,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "if",
	                      3,
	                      5,
	                      "if cond exp else exp",
	                      szl_logic_proc_if,
	                      NULL,
	                      NULL)));
}
