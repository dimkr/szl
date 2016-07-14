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

static
enum szl_res szl_obj_proc_global(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv)
{
	const char *s;

	s = szl_obj_str(interp, objv[1], NULL);
	if (!s)
		return SZL_ERR;

	/* return the value upon success - useful for one-liners */
	if (szl_local(interp, interp->global, s, objv[2]))
		return szl_set_result(interp, szl_obj_ref(objv[2]));

	return SZL_ERR;
}

static
enum szl_res szl_obj_proc_local(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	const char *name;

	name = szl_obj_str(interp, objv[1], NULL);
	if (!name)
		return SZL_ERR;

	/* see the comment in szl_obj_proc_set() */
	if (szl_local(interp, interp->current->caller, name, objv[2]))
		return szl_set_result(interp, szl_obj_ref(objv[2]));

	return SZL_ERR;
}

static
enum szl_res szl_obj_proc_export(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	struct szl_obj *val;
	const char *name;
	int resi, ref = 0;

	name = szl_obj_str(interp, objv[1], NULL);
	if (!name)
		return SZL_ERR;

	if (objc == 2) {
		val = szl_get(interp, objv[1]);
		if (!val)
			return SZL_ERR;
		ref = 1;
	}
	else
		val = objv[2];

	if (!interp->current->caller->caller) {
		szl_set_result_str(interp, "cannot export from global scope", -1);
		return SZL_ERR;
	}

	resi = szl_local(interp, interp->current->caller->caller, name, val);
	if (ref)
		szl_obj_unref(val);

	return resi ? SZL_OK : SZL_ERR;
}

static
enum szl_res szl_obj_proc_eval(struct szl_interp *interp,
                               const int objc,
                               struct szl_obj **objv)
{
	const char *s;
	size_t len;

	s = szl_obj_str(interp, objv[1], &len);
	if (!s || !len)
		return SZL_ERR;

	return szl_run(interp, s, len);
}

static
enum szl_res szl_obj_proc_echo(struct szl_interp *interp,
                               const int objc,
                               struct szl_obj **objv)
{
	return szl_set_result(interp, szl_obj_ref(objv[1]));
}

static
enum szl_res szl_obj_proc_list_vars(struct szl_interp *interp,
                                    struct szl_obj *proc)
{
	struct szl_obj *list;
	size_t i;

	list = szl_new_empty();
	if (!list)
		return SZL_ERR;

	for (i = 0; i < proc->nlocals; ++i) {
		if (!szl_lappend(interp, list, proc->locals[i]->obj)) {
			szl_obj_unref(list);
			return SZL_ERR;
		}
	}

	return szl_set_result(interp, list);
}

static
enum szl_res szl_obj_proc_locals(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv)
{
	return szl_obj_proc_list_vars(interp, szl_caller(interp));
}

static
enum szl_res szl_obj_proc_globals(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv)
{
	return szl_obj_proc_list_vars(interp, interp->global);
}

int szl_init_obj(struct szl_interp *interp)
{
	return ((szl_new_proc(interp,
	                      "global",
	                      3,
	                      3,
	                      "global name val",
	                      szl_obj_proc_global,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "local",
	                      3,
	                      3,
	                      "local name val",
	                      szl_obj_proc_local,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "export",
	                      2,
	                      3,
	                      "export name ?val?",
	                      szl_obj_proc_export,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "eval",
	                      2,
	                      2,
	                      "eval exp",
	                      szl_obj_proc_eval,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "echo",
	                      2,
	                      2,
	                      "echo obj",
	                      szl_obj_proc_echo,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "locals",
	                      1,
	                      1,
	                      "locals",
	                      szl_obj_proc_locals,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "globals",
	                      1,
	                      1,
	                      "globals",
	                      szl_obj_proc_globals,
	                      NULL,
	                      NULL)));
}
