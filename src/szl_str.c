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

static enum szl_res szl_str_proc_length(struct szl_interp *interp,
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

static enum szl_res szl_str_proc_append(struct szl_interp *interp,
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

static enum szl_res szl_str_proc_join(struct szl_interp *interp,
                                      const int objc,
                                      struct szl_obj **objv,
                                      struct szl_obj **ret)
{
	return szl_join(interp, objc - 2, objv[1], &objv[2], ret);
}

static enum szl_res szl_str_proc_ltrim(struct szl_interp *interp,
                                       const int objc,
                                       struct szl_obj **objv,
                                       struct szl_obj **ret)
{
	const char *s;
	size_t len, i;

	s = szl_obj_str(objv[1], &len);
	if (!s)
		return SZL_ERR;

	*ret = szl_obj_ref(objv[1]);

	for (i = 0; i < len; ++i) {
		if (!szl_isspace(s[i])) {
			szl_set_result(ret, szl_new_str(&s[i], len - i));
			if (!*ret)
				return SZL_ERR;
			break;
		}
	}

	return SZL_OK;
}

static enum szl_res szl_str_proc_rtrim(struct szl_interp *interp,
                                       const int objc,
                                       struct szl_obj **objv,
                                       struct szl_obj **ret)
{
	const char *s;
	size_t len;
	ssize_t i;

	s = szl_obj_str(objv[1], &len);
	if (!s)
		return SZL_ERR;

	*ret = szl_obj_ref(objv[1]);

	for (i = (ssize_t)len - 1; i >= 0; --i) {
		if (!szl_isspace(s[i])) {
			szl_set_result(ret, szl_new_str(s, (size_t)i + 1));
			if (!*ret)
				return SZL_ERR;
			break;
		}
	}

	return SZL_OK;
}

enum szl_res szl_init_str(struct szl_interp *interp)
{
	if ((!szl_new_proc(interp,
	                   "length",
	                   2,
	                   2,
	                   "length obj",
	                   szl_str_proc_length,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "append",
	                   3,
	                   3,
	                   "append name obj",
	                   szl_str_proc_append,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "join",
	                   4,
	                   -1,
	                   "join delim obj obj ?...?",
	                   szl_str_proc_join,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "ltrim",
	                   2,
	                   2,
	                   "ltrim obj",
	                   szl_str_proc_ltrim,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "rtrim",
	                   2,
	                   2,
	                   "rtrim obj",
	                   szl_str_proc_rtrim,
	                   NULL,
	                   NULL)))
		return SZL_ERR;

	return SZL_OK;
}
