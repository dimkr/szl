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
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	int eq;

	if (!szl_eq(interp, objv[1], objv[2], &eq))
		return SZL_ERR;

	return szl_set_last_bool(interp, eq);
}

static
enum szl_res szl_logic_proc_ne(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	int eq;

	if (!szl_eq(interp, objv[1], objv[2], &eq))
		return SZL_ERR;

	return szl_set_last_bool(interp, !eq);
}

static
enum szl_res szl_logic_proc_gt(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	szl_float m, n;

	if (!szl_as_float(interp, objv[1], &m) ||
		!szl_as_float(interp, objv[2], &n))
		return SZL_ERR;

	return szl_set_last_bool(interp, (m > n));
}

static
enum szl_res szl_logic_proc_ge(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	szl_float m, n;

	if (!szl_as_float(interp, objv[1], &m) ||
		!szl_as_float(interp, objv[2], &n))
		return SZL_ERR;

	return szl_set_last_bool(interp, (m >= n));
}

static
enum szl_res szl_logic_proc_lt(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	szl_float m, n;

	if (!szl_as_float(interp, objv[1], &m) ||
		!szl_as_float(interp, objv[2], &n))
		return SZL_ERR;

	return szl_set_last_bool(interp, (m < n));
}

static
enum szl_res szl_logic_proc_le(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	szl_float m, n;

	if (!szl_as_float(interp, objv[1], &m) ||
		!szl_as_float(interp, objv[2], &n))
		return SZL_ERR;

	return szl_set_last_bool(interp, (m <= n));
}

static
enum szl_res szl_logic_proc_and(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	int i, b;

	for (i = 1; i < objc; ++i) {
		if (!szl_as_bool(objv[i], &b))
			return SZL_ERR;

		if (!b)
			return szl_set_last_bool(interp, 0);
	}

	return szl_set_last_bool(interp, 1);
}

static
enum szl_res szl_logic_proc_or(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	int i, b;

	for (i = 1; i < objc; ++i) {
		if (!szl_as_bool(objv[i], &b))
			return SZL_ERR;

		if (b)
			return szl_set_last_bool(interp, 1);
	}

	return szl_set_last_bool(interp, 0);
}

static
enum szl_res szl_logic_proc_xor(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	int m, n;

	if (!szl_as_bool(objv[1], &m) || !szl_as_bool(objv[2], &n))
		return SZL_ERR;

	return szl_set_last_bool(interp, m ^ n);
}

static
enum szl_res szl_logic_proc_not(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	int b;

	if (!szl_as_bool(objv[1], &b))
		return SZL_ERR;

	return szl_set_last_bool(interp, b ? 0 : 1);
}

static
enum szl_res szl_logic_proc_if(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	char *s;
	int b;

	switch (objc) {
		case 3:
			break;

		case 5:
			if (!szl_as_str(interp, objv[3], &s, NULL))
				return SZL_ERR;

			if (strcmp("else", s) == 0)
				break;
			/* fall through */

		default:
			return szl_set_last_help(interp, objv[0]);
	}

	if (!szl_as_bool(objv[1], &b))
		return SZL_ERR;

	if (b)
		return szl_run_obj(interp, objv[2]);

	if (objc == 5)
		return szl_run_obj(interp, objv[4]);

	return SZL_OK;
}

static
enum szl_res szl_logic_proc_switch(struct szl_interp *interp,
                                   const unsigned int objc,
                                   struct szl_obj **objv)
{
	char *s;
	size_t len;
	int i, eq;

	if (objc % 2 == 1)
		return szl_set_last_help(interp, objv[0]);

	for (i = 2; i < objc; i += 2) {
		if (!szl_eq(interp, objv[1], objv[i], &eq))
			return SZL_ERR;

		if (!eq) {
			if (!szl_as_str(interp, objv[i], &s, &len))
				return SZL_ERR;

			if (strcmp(SZL_SWITCH_DEFAULT, s) == 0)
				eq = 1;
		}

		if (eq)
			return szl_run_obj(interp, objv[i + 1]);
	}

	return SZL_OK;
}

static
const struct szl_ext_export logic_exports[] = {
	{
		SZL_PROC_INIT("==",
		              "obj obj",
		              3,
		              3,
		              szl_logic_proc_eq,
		              NULL)
	},
	{
		SZL_PROC_INIT("!=",
		              "obj obj",
		              3,
		              3,
		              szl_logic_proc_ne,
		              NULL)
	},
	{
		SZL_PROC_INIT(">",
		              "m n",
		              3,
		              3,
		              szl_logic_proc_gt,
		              NULL)
	},
	{
		SZL_PROC_INIT(">=",
		              "m n",
		              3,
		              3,
		              szl_logic_proc_ge,
		              NULL)
	},
	{
		SZL_PROC_INIT("<",
		              "m n",
		              3,
		              3,
		              szl_logic_proc_lt,
		              NULL)
	},
	{
		SZL_PROC_INIT("<=",
		              "m n",
		              3,
		              3,
		              szl_logic_proc_le,
		              NULL)
	},
	{
		SZL_PROC_INIT("&&",
		              "obj obj...",
		              3,
		              -1,
		              szl_logic_proc_and,
		              NULL)
	},
	{
		SZL_PROC_INIT("||",
		              "obj obj...",
		              3,
		              -1,
		              szl_logic_proc_or,
		              NULL)
	},
	{
		SZL_PROC_INIT("^^",
		              "obj obj",
		              3,
		              3,
		              szl_logic_proc_xor,
		              NULL)
	},
	{
		SZL_PROC_INIT("!",
		              "obj",
		              2,
		              2,
		              szl_logic_proc_not,
		              NULL)
	},
	{
		SZL_PROC_INIT("if",
		              "cond exp else exp",
		              3,
		              5,
		              szl_logic_proc_if,
		              NULL)
	},
	{
		SZL_PROC_INIT("switch",
		              "obj val exp val exp...",
		              5,
		              -1,
		              szl_logic_proc_switch,
		              NULL)
	}
};

int szl_init_logic(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "logic",
	                   logic_exports,
	                   sizeof(logic_exports) / sizeof(logic_exports[0]));
}
