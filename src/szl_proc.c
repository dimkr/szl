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

#include <stdio.h>

#include "szl.h"

static enum szl_res szl_proc_eval_proc(struct szl_interp *interp,
                                       const int objc,
                                       struct szl_obj **objv,
                                       struct szl_obj **out)
{
	char buf[SZL_MAX_OBJC_DIGITS + 1];
	struct szl_obj *argc_obj;
	const char *s;
	size_t len, i;
	enum szl_res res;

	s = szl_obj_str((struct szl_obj *)objv[0]->priv, &len);
	if (!s)
		return SZL_ERR;

	/* create the $# object */
	argc_obj = szl_new_int(objc);
	if (!argc_obj)
		return SZL_ERR;

	res = szl_local(interp, objv[0], SZL_OBJC_OBJECT_NAME, argc_obj);
	szl_obj_unref(argc_obj);
	if (res != SZL_OK)
		return res;

	/* create the argument objects ($0, $1, ...) */
	for (i = 0; i < objc; ++i) {
		sprintf(buf, "%d", i);
		res = szl_local(interp, objv[0], buf, objv[i]);
		if (res != SZL_OK)
			return res;
	}

	res = szl_run_const(interp, out, s, len);

	/* the return status of return is SZL_BREAK */
	if (res == SZL_BREAK)
		return SZL_OK;

	return res;
}

static void szl_proc_del(void *priv)
{
	szl_obj_unref((struct szl_obj *)priv);
}

static enum szl_res szl_proc_proc_proc(struct szl_interp *interp,
                                       const int objc,
                                       struct szl_obj **objv,
                                       struct szl_obj **ret)
{
	const char *name;

	name = szl_obj_str(objv[1], NULL);
	if (!name)
		return SZL_ERR;

	if (szl_new_proc(interp,
	                 name,
	                 -1,
	                 -1,
	                 NULL,
	                 szl_proc_eval_proc,
	                 szl_proc_del,
	                 objv[2])) {
		szl_obj_ref(objv[2]);
		return SZL_OK;
	}

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
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "return",
	                   1,
	                   2,
	                   "return ?obj?",
	                   szl_proc_proc_return,
	                   NULL,
	                   NULL)))
		return SZL_ERR;

	return SZL_OK;
}
