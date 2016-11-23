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

static
enum szl_res szl_obj_proc_global(struct szl_interp *interp,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	/* return the value upon success - useful for one-liners */
	if (szl_set(interp, interp->global, objv[1], objv[2]))
		return szl_set_last(interp, szl_ref(objv[2]));

	return SZL_ERR;
}

static
enum szl_res szl_obj_proc_local(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	if (szl_set(interp, szl_caller(interp), objv[1], objv[2]))
		return szl_set_last(interp, szl_ref(objv[2]));

	return SZL_ERR;
}

static
enum szl_res szl_obj_proc_export(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	struct szl_obj *val;
	int resi, ref = 0;

	if (objc == 2) {
		if (!szl_get(interp, objv[1], &val) || !val)
			return SZL_ERR;

		ref = 1;
	}
	else
		val = objv[2];

	if (!interp->current->caller->caller) {
		szl_set_last_str(interp, "cannot export from global scope", -1);
		if (ref)
			szl_unref(val);
		return SZL_ERR;
	}

	resi = szl_set(interp, interp->current->caller->caller, objv[1], val);
	if (ref)
		szl_unref(val);

	return resi ? SZL_OK : SZL_ERR;
}

static
enum szl_res szl_obj_proc_eval(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	return szl_run_obj(interp, objv[1]);
}

static
enum szl_res szl_obj_proc_echo(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	return szl_set_last(interp, szl_ref(objv[1]));
}

static
enum szl_res szl_obj_proc_get(struct szl_interp *interp,
                              const unsigned int objc,
                              struct szl_obj **objv)
{
	struct szl_obj *obj;

	if (!szl_get(interp, objv[1], &obj) || !obj)
		return SZL_ERR;

	return szl_set_last(interp, obj);
}

static
enum szl_res szl_obj_proc_hex(struct szl_interp *interp,
                              const unsigned int objc,
                              struct szl_obj **objv)
{
	char *s;
	struct szl_obj *obj;
	szl_int i;
	int len;

	if (!szl_as_int(interp, objv[1], &i))
		return SZL_ERR;

	len = asprintf(&s, SZL_INT_FMT"x", i);
	if (len < 0)
		return SZL_ERR;

	obj = szl_new_str_noalloc(s, (size_t)len);
	if (!obj) {
		free(s);
		return SZL_ERR;
	}

	return szl_set_last(interp, obj);
}

static
enum szl_res szl_obj_proc_is(struct szl_interp *interp,
                             const unsigned int objc,
                             struct szl_obj **objv)
{
	return szl_set_last_bool(interp, objv[1] == objv[2]);
}

static
enum szl_res szl_obj_proc_id(struct szl_interp *interp,
                             const unsigned int objc,
                             struct szl_obj **objv)
{
	return szl_set_last_fmt(interp, "%x", rand_r(&interp->seed));
}

static
enum szl_res szl_obj_proc_call(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	return szl_run_stmt(interp, objv[1]);
}

static
const struct szl_ext_export obj_exports[] = {
	{
		SZL_PROC_INIT("global", "name val", 3, 3, szl_obj_proc_global, NULL)
	},
	{
		SZL_PROC_INIT("local", "name val", 3, 3, szl_obj_proc_local, NULL)
	},
	{
		SZL_PROC_INIT("export", "name ?val?", 2, 3, szl_obj_proc_export, NULL)
	},
	{
		SZL_PROC_INIT("eval", "exp", 2, 2, szl_obj_proc_eval, NULL)
	},
	{
		SZL_PROC_INIT("echo", "obj", 2, 2, szl_obj_proc_echo, NULL)
	},
	{
		SZL_PROC_INIT("get", "name", 2, 2, szl_obj_proc_get, NULL)
	},
	{
		SZL_PROC_INIT("hex", "val", 2, 2, szl_obj_proc_hex, NULL)
	},
	{
		SZL_PROC_INIT("is", "obj obj", 3, 3, szl_obj_proc_is, NULL)
	},
	{
		SZL_PROC_INIT("id", NULL, 1, 1, szl_obj_proc_id, NULL)
	},
	{
		SZL_PROC_INIT("call", "stmt", 2, 2, szl_obj_proc_call, NULL)
	}
};

int szl_init_obj(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "obj",
	                   obj_exports,
	                   sizeof(obj_exports) / sizeof(obj_exports[0]));
}
