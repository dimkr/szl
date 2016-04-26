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

#include "szl.h"

static enum szl_res szl_loop_proc_while(struct szl_interp *interp,
                                        const int objc,
                                        struct szl_obj **objv,
                                        struct szl_obj **ret)
{
	struct szl_obj *val;
	const char *cond, *body;
	size_t blen;
	enum szl_res res;
	int isfalse;

	cond = szl_obj_str(objv[1], NULL);
	if (!cond)
		return SZL_ERR;

	body = szl_obj_str(objv[2], &blen);
	if (!body)
		return SZL_ERR;

	do {
		szl_unset(ret);
		res = szl_eval(interp, &val, cond);
		if (res != SZL_OK) {
			if (val)
				szl_set_result(ret, val);
			break;
		}

		isfalse = szl_obj_isfalse(val);
		szl_obj_unref(val);
		if (isfalse) {
			res = SZL_OK;
			break;
		}

		/* we pass the block return value */
		res = szl_run_const(interp, ret, body, blen);

		/* stop if we encounter an error; pass the body evaluation return value
		 * because it the error */
		if (res == SZL_ERR)
			return SZL_ERR;

		/* if break was called, report success */
		if (res == SZL_BREAK)
			return SZL_OK;

		/*
		 * otherwise, in both cases we want to continue to the next iteration:
		 *   - if res is SZL_CONT, szl_run_lines() stopped the execution of the
		 *     loop body lines and we jump to the first one
		 *   - if res is SZL_OK, szl_run_lines() executed the entire loop body,
		 *     so we continue the loop as usual
		 */
	} while (1);

	return res;
}

static enum szl_res szl_loop_proc_break(struct szl_interp *interp,
                                        const int objc,
                                        struct szl_obj **objv,
                                        struct szl_obj **ret)
{
	return SZL_BREAK;
}

static enum szl_res szl_loop_proc_continue(struct szl_interp *interp,
                                           const int objc,
                                           struct szl_obj **objv,
                                           struct szl_obj **ret)
{
	return SZL_CONT;
}

enum szl_res szl_init_loop(struct szl_interp *interp)
{
	if ((!szl_new_proc(interp,
	                   "while",
	                   3,
	                   3,
	                   "while cond exp",
	                   szl_loop_proc_while,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "break",
	                   1,
	                   1,
	                   "break",
	                   szl_loop_proc_break,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "continue",
	                   1,
	                   1,
	                   "continue",
	                   szl_loop_proc_continue,
	                   NULL,
	                   NULL)))
		return SZL_ERR;

	return SZL_OK;
}
