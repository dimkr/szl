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

static enum szl_res szl_proc_eval_proc(struct szl_interp *interp,
                                       const int objc,
                                       struct szl_obj **objv,
                                       struct szl_obj **out)
{
	const char *s;
	size_t len;
	enum szl_res res;

	s = szl_obj_str(interp->current->exp, &len);
	if (!s)
		return SZL_ERR;

	res = szl_run_const(interp, out, s, len);

	/* the return status of return is SZL_BREAK */
	if (res == SZL_BREAK)
		return SZL_OK;

	return res;
}

static enum szl_res szl_proc_proc_proc(struct szl_interp *interp,
                                       const int objc,
                                       struct szl_obj **objv,
                                       struct szl_obj **ret)
{
	if (szl_new_proc(interp,
	                 szl_obj_str(objv[1], NULL),
	                 -1,
	                 -1,
	                 NULL,
	                 szl_proc_eval_proc,
	                 NULL,
	                 NULL,
	                 objv[2]))
		return SZL_OK;

	return SZL_ERR;
}

static enum szl_res szl_proc_proc_return(struct szl_interp *interp,
                                         const int objc,
                                         struct szl_obj **objv,
                                         struct szl_obj **ret)
{
	if (objc == 2)
		szl_set_result(ret, szl_obj_ref(objv[1]));

	return SZL_BREAK;
}

enum szl_res szl_init_proc(struct szl_interp *interp)
{
	if ((!szl_new_proc(interp,
	                   "proc",
	                   3,
	                   3,
	                   "proc name exp",
	                   szl_proc_proc_proc,
	                   NULL,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "return",
	                   1,
	                   2,
	                   "return ?obj?",
	                   szl_proc_proc_return,
	                   NULL,
	                   NULL,
	                   NULL)))
		return SZL_ERR;

	return SZL_OK;
}
