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

#include "szl.h"

static const char szl_loop_inc[] = {
#include "szl_loop.inc"
};

static
enum szl_res szl_loop_proc_while(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv)
{
	struct szl_block block;
	struct szl_obj *val;
	const char *cond;
	char *body;
	size_t blen;
	enum szl_res res;
	int isfalse;

	cond = szl_obj_str(interp, objv[1], NULL);
	if (!cond)
		return SZL_ERR;

	body = szl_obj_strdup(interp, objv[2], &blen);
	if (!body)
		return SZL_ERR;

	if (!szl_parse_block(interp, &block, body, blen)) {
		free(body);
		return SZL_ERR;
	}

	do {
		res = szl_eval(interp, &val, cond);
		if (res != SZL_OK) {
			if (val)
				szl_set_result(interp, val);
			szl_free_block(&block);
			free(body);
			return res;
		}

		isfalse = szl_obj_isfalse(val);
		szl_obj_unref(val);
		if (isfalse) {
			szl_free_block(&block);
			free(body);
			return SZL_OK;
		}

		/* we pass the block return value */
		res = szl_run_block(interp, &block);
		switch (res) {
			case SZL_ERR:
			case SZL_EXIT:
			case SZL_RET:
				szl_free_block(&block);
				free(body);
				return res;

			/* do not propagate SZL_BREAK */
			case SZL_BREAK:
				free(body);
				szl_free_block(&block);
				return SZL_OK;

			default:
				break;
		}
	} while (1);

	szl_free_block(&block);
	free(body);
	return SZL_ERR;
}

static
enum szl_res szl_loop_proc_break(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv)
{
	return SZL_BREAK;
}

static
enum szl_res szl_loop_proc_continue(struct szl_interp *interp,
                                    const int objc,
                                    struct szl_obj **objv)
{
	return SZL_CONT;
}

static
enum szl_res szl_loop_proc_exit(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	if (objc == 2)
		szl_set_result(interp, szl_obj_ref(objv[1]));

	return SZL_EXIT;
}

static
enum szl_res szl_loop_map(struct szl_interp *interp,
                           const int objc,
                           struct szl_obj **objv,
                           const int keep)
{
	struct szl_block body;
	char *exp;
	const char *name;
	struct szl_obj **names, **toks, *obj = NULL;
	size_t elen, nlen, i, j, ntoks, nnames;
	enum szl_res res, resi;

	if (!szl_obj_list(interp, objv[1], &names, &nnames) || !nnames)
		return SZL_ERR;

	exp = szl_obj_strdup(interp, objv[3], &elen);
	if (!exp || !elen)
		return SZL_ERR;

	if (!szl_parse_block(interp, &body, exp, elen)) {
		free(exp);
		return SZL_ERR;
	}

	if (!szl_obj_list(interp, objv[2], &toks, &ntoks)) {
		szl_free_block(&body);
		free(exp);
		return SZL_ERR;
	}

	if (ntoks % nnames != 0) {
		szl_free_block(&body);
		free(exp);
		return szl_usage(interp, objv[0]);
	}

	if (keep) {
		obj = szl_new_empty();
		if (!obj) {
			szl_free_block(&body);
			free(exp);
			return SZL_ERR;
		}
	}

	for (i = 0; i <= ntoks - nnames; i += nnames) {
		for (j = 0; j < nnames; ++j) {
			name = szl_obj_str(interp, names[j], &nlen);
			if (!name || !nlen) {
				if (keep)
					szl_obj_unref(obj);
				szl_free_block(&body);
				free(exp);
				return SZL_ERR;
			}

			resi = szl_local(interp, interp->current, name, toks[i + j]);
			if (!resi) {
				if (keep)
					szl_obj_unref(obj);
				szl_free_block(&body);
				free(exp);
				return SZL_ERR;
			}
		}

		res = szl_run_block(interp, &body);
		if (res == SZL_CONT)
			continue;

		if (res == SZL_BREAK)
			break;

		if (res == SZL_RET) {
			if (keep)
				szl_obj_unref(obj);
			szl_free_block(&body);
			free(exp);
			return SZL_RET;
		}

		if (res != SZL_OK) {
			if (keep)
				szl_obj_unref(obj);
			szl_free_block(&body);
			free(exp);
			return res;
		}

		if (keep) {
			if (!szl_lappend(interp, obj, interp->last)) {
				if (keep)
					szl_obj_unref(obj);
				szl_free_block(&body);
				free(exp);
				return SZL_ERR;
			}
		}
	}

	szl_free_block(&body);
	free(exp);

	if (keep)
		return szl_set_result(interp, obj);

	szl_empty_result(interp);
	return SZL_OK;
}

static
enum szl_res szl_loop_proc_for(struct szl_interp *interp,
                               const int objc,
                               struct szl_obj **objv)
{
	return szl_loop_map(interp, objc, objv, 0);
}

static
enum szl_res szl_loop_proc_map(struct szl_interp *interp,
                               const int objc,
                               struct szl_obj **objv)
{
	return szl_loop_map(interp, objc, objv, 1);
}

static
enum szl_res szl_loop_proc_range(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv)
{
	struct szl_obj *obj;
	szl_int start = 0, end, i, n;

	if ((!szl_obj_int(interp, objv[objc - 1], &end)) ||
	    ((objc == 3) && (!szl_obj_int(interp, objv[1], &start))) ||
	    (start >= end))
		return SZL_ERR;

	n = end - start;
	if (n > INT_MAX)
		return SZL_ERR;

	obj = szl_new_empty();
	if (!obj)
		return SZL_ERR;

	for (i = 0; i < n; ++i) {
		if (!szl_lappend_int(interp, obj, start + i)) {
			szl_obj_unref(obj);
			return SZL_ERR;
		}
	}

	return szl_set_result(interp, obj);
}

int szl_init_loop(struct szl_interp *interp)
{
	return ((szl_new_proc(interp,
	                      "while",
	                      3,
	                      3,
	                      "while cond exp",
	                      szl_loop_proc_while,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "break",
	                      1,
	                      1,
	                      "break",
	                      szl_loop_proc_break,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "continue",
	                      1,
	                      1,
	                      "continue",
	                      szl_loop_proc_continue,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "exit",
	                      1,
	                      2,
	                      "exit ?obj?",
	                      szl_loop_proc_exit,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "for",
	                      -1,
	                      -1,
	                      "for names list exp",
	                      szl_loop_proc_for,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "map",
	                      4,
	                      4,
	                      "map names list exp",
	                      szl_loop_proc_map,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "range",
	                      2,
	                      3,
	                      "range ?start? end",
	                      szl_loop_proc_range,
	                      NULL,
	                      NULL)) &&
	        (szl_run(interp,
	                 szl_loop_inc,
	                 sizeof(szl_loop_inc) - 1) == SZL_OK));
}
