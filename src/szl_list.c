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
enum szl_res szl_list_proc_length(struct szl_interp *interp,
                                  const int objc,
                                  struct szl_obj **objv)
{
	size_t n;

	if (!szl_obj_list(interp, objv[1], &n))
		return SZL_ERR;

	return szl_set_result_int(interp, (szl_int)n);
}

static
enum szl_res szl_list_proc_append(struct szl_interp *interp,
                                  const int objc,
                                  struct szl_obj **objv)
{
	struct szl_obj *obj;
	const char *name, *s;
	size_t len, slen;

	s = szl_obj_str(objv[2], &slen);
	if (!s)
		return SZL_ERR;

	name = szl_obj_str(objv[1], &len);
	if (!name || !len)
		return SZL_ERR;

	obj = szl_get(interp, name);
	if (!obj || !szl_lappend_str(obj, s))
		return SZL_ERR;

	return SZL_OK;
}

static
enum szl_res szl_list_proc_extend(struct szl_interp *interp,
                                  const int objc,
                                  struct szl_obj **objv)
{
	struct szl_obj *obj;
	const char *name, **toks;
	size_t len, n, i;

	name = szl_obj_str(objv[1], &len);
	if (!name || !len)
		return SZL_ERR;

	if (!szl_obj_len(objv[2], &len))
		return SZL_ERR;

	obj = szl_get(interp, name);
	if (!obj)
		return SZL_ERR;

	if (!len)
		return SZL_OK;

	toks = szl_obj_list(interp, objv[2], &n);
	if (!toks || (n >= SZL_INT_MAX))
		return SZL_ERR;

	for (i = 0; i < n; ++i) {
		if (!szl_lappend_str(obj, toks[i])) {
			szl_obj_unref(obj);
			return SZL_ERR;
		}
	}

	return SZL_OK;
}

static
enum szl_res szl_list_proc_index(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv)
{
	const char **toks;
	szl_int i;
	size_t n, len;

	if ((!szl_obj_int(objv[2], &i)) || (i < 0))
		return SZL_ERR;

	toks = szl_obj_list(interp, objv[1], &n);
	if (!toks || (i >= n))
		return SZL_ERR;

	len = strlen(toks[i]);
	if ((len >= 2) &&
	    (toks[i][0] == '{') &&
	    (toks[i][len - 2] != '\\') &&
	    (toks[i][len - 1] == '}'))
		return szl_set_result_str(interp, toks[i] + 1, (int)len - 2);

	return szl_set_result_str(interp, toks[i], (int)len);
}

static
enum szl_res szl_list_proc_range(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv)
{
	struct szl_obj *obj;
	const char **toks;
	szl_int start, end, i;
	size_t n;

	toks = szl_obj_list(interp, objv[1], &n);
	if (!toks || (n >= SZL_INT_MAX))
		return SZL_ERR;

	if (!szl_obj_int(objv[2], &start) ||
	    (start < 0) ||
	    (start >= (szl_int)n) ||
	    !szl_obj_int(objv[3], &end) ||
	    (end < start) ||
	    (end >= (szl_int)n))
		return SZL_ERR;

	obj = szl_new_empty();
	if (!obj)
		return SZL_ERR;

	for (i = start; i <= end; ++i) {
		if (!szl_lappend_str(obj, toks[i])) {
			szl_obj_unref(obj);
			return SZL_ERR;
		}
	}

	return szl_set_result(interp, obj);
}

static
enum szl_res szl_list_proc_reverse(struct szl_interp *interp,
                                   const int objc,
                                   struct szl_obj **objv)
{
	struct szl_obj *obj;
	const char **toks;
	szl_int i;
	size_t n;

	toks = szl_obj_list(interp, objv[1], &n);
	if (!toks || (n >= SZL_INT_MAX))
		return SZL_ERR;

	obj = szl_new_empty();
	if (!obj)
		return SZL_ERR;

	for (i = (szl_int)n - 1; i >= 0; --i) {
		if (!szl_lappend_str(obj, toks[i])) {
			szl_obj_unref(obj);
			return SZL_ERR;
		}
	}

	return szl_set_result(interp, obj);
}

int szl_init_list(struct szl_interp *interp)
{
	return ((szl_new_proc(interp,
	                      "list.length",
	                      2,
	                      2,
	                      "list.length obj",
	                      szl_list_proc_length,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "list.append",
	                      3,
	                      3,
	                      "list.append name obj",
	                      szl_list_proc_append,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "list.extend",
	                      3,
	                      3,
	                      "list.extend name obj",
	                      szl_list_proc_extend,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "list.index",
	                      3,
	                      3,
	                      "list.index obj index",
	                      szl_list_proc_index,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "list.range",
	                      4,
	                      4,
	                      "list.range obj start end",
	                      szl_list_proc_range,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "list.reverse",
	                      2,
	                      2,
	                      "list.reverse obj",
	                      szl_list_proc_reverse,
	                      NULL,
	                      NULL)));
}
