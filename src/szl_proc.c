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
#include <sys/types.h>
#include <unistd.h>

#include "szl.h"

struct szl_proc {
	struct szl_obj *body;
	struct szl_obj *priv;
};

static
enum szl_res szl_proc_proc_getpid(struct szl_interp *interp,
                                  const unsigned int objc,
                                  struct szl_obj **objv)
{
	return szl_set_last_int(interp, (szl_int)getpid());
}

static
enum szl_res szl_proc_eval_proc(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	struct szl_proc *proc = (struct szl_proc *)objv[0]->priv;
	enum szl_res res;

	if (!szl_set_args(interp, interp->current, interp->current->args))
		return SZL_ERR;

	if (!szl_set(interp, interp->current, interp->priv, proc->priv))
		return SZL_ERR;

	res = szl_run_obj(interp, proc->body);
	return (res == SZL_RET) ? SZL_OK : res;
}

static
void szl_proc_del(void *priv)
{
	struct szl_proc *proc = (struct szl_proc *)priv;

	szl_unref(proc->priv);
	szl_unref(proc->body);
	free(priv);
}

static
enum szl_res szl_proc_proc_proc(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	struct szl_proc *proc;
	struct szl_obj *obj;

	proc = (struct szl_proc *)malloc(sizeof(*proc));
	if (!proc)
		return SZL_ERR;

	obj = szl_new_proc(interp,
	                   objv[1],
	                   -1,
	                   -1,
	                   NULL,
	                   szl_proc_eval_proc,
	                   szl_proc_del,
	                   proc);
	if (!obj) {
		free(proc);
		return SZL_ERR;
	}

	proc->body = szl_ref(objv[2]);
	proc->priv = (objc == 4) ? szl_ref(objv[3]) : szl_ref(interp->empty);
	return szl_set_last(interp, obj);
}

static
enum szl_res szl_proc_proc_return(struct szl_interp *interp,
                                  const unsigned int objc,
                                  struct szl_obj **objv)
{
	if (objc == 2)
		szl_set_last(interp, szl_ref(objv[1]));

	return SZL_RET;
}

static
enum szl_res szl_proc_proc_stack(struct szl_interp *interp,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	struct szl_obj *stack;
	struct szl_frame *call;
	szl_int lim = SZL_INT_MAX, i;

	if (objc == 2) {
		if (!szl_as_int(interp, objv[1], &lim))
			return SZL_ERR;

		if (lim <= 0) {
			szl_set_last_fmt(interp, "bad limit: "SZL_INT_FMT"d", lim);
			return SZL_ERR;
		}
	}

	stack = szl_new_empty();
	if (!stack)
		return SZL_ERR;

	call = interp->current;
	for (i = 0; (call != interp->global) && (i < lim); ++i) {
		if (!szl_list_append(interp, stack, call->args)) {
			szl_unref(stack);
			return SZL_ERR;
		}

		call = call->caller;
	}

	return szl_set_last(interp, stack);
}

static
const struct szl_ext_export proc_exports[] = {
	{
		SZL_PROC_INIT("getpid", NULL, 1, 1, szl_proc_proc_getpid, NULL)
	},
	{
		SZL_PROC_INIT("proc", "name exp ?priv?", 3, 4, szl_proc_proc_proc, NULL)
	},
	{
		SZL_PROC_INIT("return", "?obj?", 1, 2, szl_proc_proc_return, NULL)
	},
	{
		SZL_PROC_INIT("stack", "?lim?", 1, 2, szl_proc_proc_stack, NULL)
	}
};

int szl_init_proc(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "proc",
	                   proc_exports,
	                   sizeof(proc_exports) / sizeof(proc_exports[0]));
}
