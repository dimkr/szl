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

#include "szl.h"

static
enum szl_res szl_loop_check(struct szl_interp *interp,
                            struct szl_obj *cond,
                            int *b)
{
	enum szl_res res;

	res = szl_eval(interp, cond);
	if (res == SZL_OK)
		return szl_as_bool(interp->last, b) ? SZL_OK : SZL_ERR;

	return res;
}

static
enum szl_res szl_loop_do_while(struct szl_interp *interp,
                               struct szl_obj *cond,
                               struct szl_obj *body)
{
	enum szl_res res;
	int istrue;

	do {
		res = szl_loop_check(interp, cond, &istrue);
		if ((res != SZL_OK) || !istrue)
			return res;

		/* we pass the block return value */
		res = szl_run_obj(interp, body);
		switch (res) {
			case SZL_ERR:
			case SZL_EXIT:
			case SZL_RET:
				return res;

			/* do not propagate SZL_BREAK */
			case SZL_BREAK:
				return SZL_OK;

			default:
				break;
		}
	} while (1);

	return SZL_ERR;
}

static
enum szl_res szl_loop_proc_while(struct szl_interp *interp,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	int istrue;
	enum szl_res res;


	res = szl_loop_check(interp, objv[1], &istrue);
	if ((res == SZL_OK) && istrue)
		res = szl_loop_do_while(interp, objv[1], objv[2]);

	return res;
}

static
enum szl_res szl_loop_proc_do(struct szl_interp *interp,
                              const unsigned int objc,
                              struct szl_obj **objv)
{
	char *s;
	enum szl_res res;

	if (!szl_as_str(interp, objv[2], &s, NULL))
		return SZL_ERR;

	if (strcmp(s, "while") != 0)
		return szl_set_last_help(interp, objv[0]);

	res = szl_loop_do_while(interp, objv[3], objv[1]);
	return res;
}

static
enum szl_res szl_loop_proc_break(struct szl_interp *interp,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	return SZL_BREAK;
}

static
enum szl_res szl_loop_proc_continue(struct szl_interp *interp,
                                    const unsigned int objc,
                                    struct szl_obj **objv)
{
	return SZL_CONT;
}

static
enum szl_res szl_loop_proc_exit(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	if (objc == 2)
		szl_set_last(interp, szl_ref(objv[1]));

	return SZL_EXIT;
}

static
enum szl_res szl_loop_map(struct szl_interp *interp,
                           const unsigned int objc,
                           struct szl_obj **objv,
                           const int keep)
{
	char *exp;
	struct szl_obj **names, **toks, *obj = NULL;
	size_t elen, i, j, ntoks, nnames;
	enum szl_res res;

	if (!szl_as_list(interp, objv[1], &names, &nnames) || !nnames)
		return szl_set_last_help(interp, objv[0]);

	if (!szl_as_list(interp, objv[2], &toks, &ntoks))
		return SZL_ERR;

	if (ntoks % nnames != 0) {
		szl_set_last_fmt(interp, "bad number of values", -1);
		return SZL_ERR;
	}

	if (!ntoks)
		return SZL_OK;

	if (keep) {
		obj = szl_new_list(NULL, 0);
		if (!obj)
			return SZL_ERR;
	}

	for (i = 0; i <= ntoks - nnames; i += nnames) {
		for (j = 0; j < nnames; ++j) {
			if (!szl_set(interp, interp->current, names[j], toks[i + j])) {
				if (obj)
					szl_unref(obj);
				return SZL_ERR;
			}
		}

		res = szl_run_obj(interp, objv[3]);
		if (res == SZL_CONT)
			continue;

		if (res == SZL_BREAK)
			break;

		if (res == SZL_RET) {
			if (obj)
				szl_unref(obj);
			return SZL_RET;
		}

		if (res != SZL_OK) {
			if (obj)
				szl_unref(obj);
			return res;
		}

		if (obj) {
			if (!szl_list_append(interp, obj, interp->last)) {
				szl_unref(obj);
				return SZL_ERR;
			}
		}
	}

	if (obj)
		return szl_set_last(interp, obj);

	return SZL_OK;
}

static
enum szl_res szl_loop_proc_for(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	return szl_loop_map(interp, objc, objv, 0);
}

static
enum szl_res szl_loop_proc_map(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	return szl_loop_map(interp, objc, objv, 1);
}

static
enum szl_res szl_loop_proc_range(struct szl_interp *interp,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	struct szl_obj *obj;
	szl_int start = 0, end, i, n;

	if ((!szl_as_int(interp, objv[objc - 1], &end)) ||
	    ((objc == 3) && (!szl_as_int(interp, objv[1], &start))) ||
	    (start >= end))
		return SZL_ERR;

	n = end - start;
	if (n > INT_MAX)
		return SZL_ERR;

	obj = szl_new_list(NULL, 0);
	if (!obj)
		return SZL_ERR;

	for (i = 0; i < n; ++i) {
		if (!szl_list_append_int(interp, obj, start + i)) {
			szl_unref(obj);
			return SZL_ERR;
		}
	}

	return szl_set_last(interp, obj);
}

static
const struct szl_ext_export loop_exports[] = {
	{
		SZL_PROC_INIT("while",
		              "cond exp",
		              3,
		              3,
		              szl_loop_proc_while,
		              NULL)
	},
	{
		SZL_PROC_INIT("do",
		              "exp while cond",
		              4,
		              4,
		              szl_loop_proc_do,
		              NULL)
	},
	{
		SZL_PROC_INIT("break",
		              NULL,
		              1,
		              1,
		              szl_loop_proc_break,
		              NULL)
	},
	{
		SZL_PROC_INIT("continue",
		              NULL,
		              1,
		              1,
		              szl_loop_proc_continue,
		              NULL)
	},
	{
		SZL_PROC_INIT("exit", "?obj?", 1, 2, szl_loop_proc_exit, NULL)
	},
	{
		SZL_PROC_INIT("for",
		              "names list exp",
		              4,
		              4,
		              szl_loop_proc_for,
		              NULL)
	},
	{
		SZL_PROC_INIT("map",
		              "names list exp",
		              4,
		              4,
		              szl_loop_proc_map,
		              NULL)
	},
	{
		SZL_PROC_INIT("range",
		              "?start? end",
		              2,
		              3,
		              szl_loop_proc_range,
		              NULL)
	}
};

int szl_init_loop(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "loop",
	                   loop_exports,
	                   sizeof(loop_exports) / sizeof(loop_exports[0]));
}
