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

#define SZL_SWITCH_DEFAULT "*"

static
enum szl_res szl_logic_proc_eq(struct szl_interp *interp,
                               const int objc,
                               struct szl_obj **objv)
{
	int eq;

	if (!szl_obj_eq(interp, objv[1], objv[2], &eq))
		return SZL_ERR;

	return szl_set_result_bool(interp, (szl_int)eq);
}

static
enum szl_res szl_logic_proc_ne(struct szl_interp *interp,
                               const int objc,
                               struct szl_obj **objv)
{
	int eq;

	if (!szl_obj_eq(interp, objv[1], objv[2], &eq))
		return SZL_ERR;

	return szl_set_result_bool(interp, (szl_int)!eq);
}

static
enum szl_res szl_logic_proc_gt(struct szl_interp *interp,
                               const int objc,
                               struct szl_obj **objv)
{
	szl_double m, n;

	if (!szl_obj_double(interp, objv[1], &m) ||
		!szl_obj_double(interp, objv[2], &n))
		return SZL_ERR;

	return szl_set_result_bool(interp, (m > n));
}

static
enum szl_res szl_logic_proc_ge(struct szl_interp *interp,
                               const int objc,
                               struct szl_obj **objv)
{
	szl_double m, n;

	if (!szl_obj_double(interp, objv[1], &m) ||
		!szl_obj_double(interp, objv[2], &n))
		return SZL_ERR;

	return szl_set_result_bool(interp, (m >= n));
}

static
enum szl_res szl_logic_proc_lt(struct szl_interp *interp,
                               const int objc,
                               struct szl_obj **objv)
{
	szl_double m, n;

	if (!szl_obj_double(interp, objv[1], &m) ||
		!szl_obj_double(interp, objv[2], &n))
		return SZL_ERR;

	return szl_set_result_bool(interp, (m < n));
}

static
enum szl_res szl_logic_proc_le(struct szl_interp *interp,
                               const int objc,
                               struct szl_obj **objv)
{
	szl_double m, n;

	if (!szl_obj_double(interp, objv[1], &m) ||
		!szl_obj_double(interp, objv[2], &n))
		return SZL_ERR;

	return szl_set_result_bool(interp, (m <= n));
}

static
enum szl_res szl_logic_proc_and(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	szl_int out;
	int i;

	out = szl_obj_istrue(objv[1]);
	for (i = 2; out && (i < objc); ++i)
		out &= szl_obj_istrue(objv[i]);

	return szl_set_result_bool(interp, out);
}

static
enum szl_res szl_logic_proc_or(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	szl_int out;
	int i;

	out = szl_obj_istrue(objv[1]);
	for (i = 2; !out && (i < objc); ++i)
		out |= szl_obj_istrue(objv[i]);

	return szl_set_result_bool(interp, out);
}

static
enum szl_res szl_logic_proc_xor(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	return szl_set_result_bool(interp,
	                           (szl_obj_istrue(objv[1]) ^
	                            szl_obj_istrue(objv[2])));
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
			s = szl_obj_str(interp, objv[3], NULL);
			if (!s)
				return SZL_ERR;

			if (strcmp("else", s) == 0)
				break;
			/* fall through */

		default:
			return szl_usage(interp, objv[0]);
	}

	if (szl_obj_istrue(objv[1])) {
		s = szl_obj_str(interp, objv[2], &len);
		if (!s)
			return SZL_ERR;

		return szl_run(interp, s, len);
	}

	if (objc == 5) {
		s = szl_obj_str(interp, objv[4], &len);
		if (!s)
			return SZL_ERR;

		return szl_run(interp, s, len);
	}

	return SZL_OK;
}

static
enum szl_res szl_logic_proc_switch(struct szl_interp *interp,
                                   const int objc,
                                   struct szl_obj **objv)
{
	const char *s, *exp;
	size_t len;
	int i, eq;

	if (objc % 2 == 1)
		return szl_usage(interp, objv[0]);

	for (i = 2; i < objc; i += 2) {
		if (!szl_obj_eq(interp, objv[1], objv[i], &eq))
			return SZL_ERR;

		if (!eq) {
			s = szl_obj_str(interp, objv[i], &len);
			if (!s)
				return SZL_ERR;

			if (strcmp(SZL_SWITCH_DEFAULT, s) == 0)
				eq = 1;
		}

		if (eq) {
			exp = szl_obj_str(interp, objv[i + 1], &len);
			if (!exp || !len)
				return SZL_ERR;

			return szl_run(interp, exp, len);
		}
	}

	return SZL_OK;
}

int szl_init_logic(struct szl_interp *interp)
{
	return ((szl_new_proc(interp,
	                      "==",
	                      3,
	                      3,
	                      "== obj obj",
	                      szl_logic_proc_eq,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "!=",
	                      3,
	                      3,
	                      "!= obj obj",
	                      szl_logic_proc_ne,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      ">",
	                      3,
	                      3,
	                      "> m n",
	                      szl_logic_proc_gt,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      ">=",
	                      3,
	                      3,
	                      ">= m n",
	                      szl_logic_proc_ge,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "<",
	                      3,
	                      3,
	                      "< m n",
	                      szl_logic_proc_lt,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "<=",
	                      3,
	                      3,
	                      "<= m n",
	                      szl_logic_proc_le,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "&&",
	                      3,
	                      -1,
	                      "&& obj obj...",
	                      szl_logic_proc_and,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "||",
	                      3,
	                      -1,
	                      "|| obj obj...",
	                      szl_logic_proc_or,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "^",
	                      3,
	                      3,
	                      "^ obj obj",
	                      szl_logic_proc_xor,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "!",
	                      2,
	                      2,
	                      "! obj",
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
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "switch",
	                      5,
	                      -1,
	                      "switch obj val exp val exp...",
	                      szl_logic_proc_switch,
	                      NULL,
	                      NULL)));
}
