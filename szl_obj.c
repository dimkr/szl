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

static enum szl_res szl_obj_proc_set(struct szl_interp *interp,
                                     const int objc,
                                     struct szl_obj **objv,
                                     struct szl_obj **ret)
{
	enum szl_res res;
	const char *s;

	s = szl_obj_str(objv[1], NULL);
	if (!s)
		return SZL_ERR;

	res = szl_set(interp, s, objv[2]);
	/* return the value upon success - useful for one-liners */
	if (res == SZL_OK) {
		szl_set_result(ret, szl_obj_ref(objv[2]));
		if (!*ret)
			return SZL_ERR;
	}

	return res;
}

static enum szl_res szl_obj_proc_local(struct szl_interp *interp,
                                       const int objc,
                                       struct szl_obj **objv,
                                       struct szl_obj **ret)
{
	enum szl_res res;
	const char *name;

	name = szl_obj_str(objv[1], NULL);
	if (!name)
		return SZL_ERR;

	szl_obj_ref(interp->caller);
	res = szl_local(interp, interp->caller, name, objv[2]);
	szl_obj_unref(interp->caller);

	/* see the comment in szl_obj_proc_set() */
	if (res == SZL_OK) {
		szl_set_result(ret, szl_obj_ref(objv[2]));
		if (!*ret)
			return SZL_ERR;
	}

	return res;
}

static enum szl_res szl_obj_proc_length(struct szl_interp *interp,
                                        const int objc,
                                        struct szl_obj **objv,
                                        struct szl_obj **ret)
{
	size_t len;
	enum szl_res res;

	res = szl_obj_len(objv[1], &len);
	if ((res != SZL_OK) || (len > INT_MAX))
		return res;

	szl_set_result_int(interp, ret, (szl_int)len);
	return SZL_OK;
}

static enum szl_res szl_obj_proc_append(struct szl_interp *interp,
                                        const int objc,
                                        struct szl_obj **objv,
                                        struct szl_obj **ret)
{
	struct szl_obj *obj;
	const char *name;
	const char *s;
	size_t len;
	enum szl_res res;

	name = szl_obj_str(objv[1], NULL);
	if (!name)
		return SZL_ERR;

	if (szl_get(interp, &obj, name) != SZL_OK)
		return SZL_ERR;

	s = szl_obj_str(objv[2], &len);
	if (!s) {
		szl_obj_unref(obj);
		return SZL_ERR;
	}

	if (!len) {
		szl_obj_unref(obj);
		return SZL_OK;
	}

	res = szl_append(obj, s, len);
	szl_obj_unref(obj);

	return res;
}

static enum szl_res szl_obj_proc_join(struct szl_interp *interp,
                                      const int objc,
                                      struct szl_obj **objv,
                                      struct szl_obj **ret)
{
	struct szl_obj *obj;
	const char *delim;
	const char *s;
	size_t slen, dlen;
	enum szl_res res;
	int i, last = objc - 1;

	delim = szl_obj_str(objv[1], &dlen);
	if (!delim)
		return SZL_ERR;

	obj = szl_new_str(interp, "", 0);
	if (!obj)
		return SZL_ERR;

	for (i = 2; i < objc; ++i) {
		s = szl_obj_str(objv[i], &slen);
		if (!s) {
			szl_obj_unref(obj);
			return SZL_ERR;
		}

		res = szl_append(obj, s, slen);
		if (res != SZL_OK) {
			szl_obj_unref(obj);
			return res;
		}

		if (dlen && (i != last)) {
			res = szl_append(obj, delim, dlen);
			if (res != SZL_OK) {
				szl_obj_unref(obj);
				return res;
			}
		}
	}

	szl_set_result(ret, obj);
	return SZL_OK;
}

enum szl_res szl_init_obj(struct szl_interp *interp)
{
	if ((!szl_new_proc(interp,
	                   "set",
	                   3,
	                   3,
	                   "set name val",
	                   szl_obj_proc_set,
	                   NULL,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "local",
	                   3,
	                   3,
	                   "local name val",
	                   szl_obj_proc_local,
	                   NULL,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "length",
	                   2,
	                   2,
	                   "length obj",
	                   szl_obj_proc_length,
	                   NULL,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "append",
	                   3,
	                   3,
	                   "append name obj",
	                   szl_obj_proc_append,
	                   NULL,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "join",
	                   4,
	                   -1,
	                   "join delim obj obj ?...?",
	                   szl_obj_proc_join,
	                   NULL,
	                   NULL,
	                   NULL)))
		return SZL_ERR;

	return SZL_OK;
}
