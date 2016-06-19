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
	struct szl_obj **items;
	size_t n;

	if (!szl_obj_list(interp, objv[1], &items, &n))
		return SZL_ERR;

	return szl_set_result_int(interp, (szl_int)n);
}

static
enum szl_res szl_list_proc_append(struct szl_interp *interp,
                                  const int objc,
                                  struct szl_obj **objv)
{
	struct szl_obj *list;
	const char *name;
	size_t len;

	name = szl_obj_str(interp, objv[1], &len);
	if (!name)
		return SZL_ERR;

	if (!len) {
		szl_set_result_str(interp, "empty list name", -1);
		return SZL_ERR;
	}

	list = szl_get(interp, name);
	if (!list || !szl_lappend(interp, list, objv[2])) {
		szl_obj_unref(list);
		return SZL_ERR;
	}

	szl_obj_unref(list);
	return SZL_OK;
}

static
enum szl_res szl_list_proc_extend(struct szl_interp *interp,
                                  const int objc,
                                  struct szl_obj **objv)
{
	const char *name;
	struct szl_obj *list, **items;
	size_t len, n, i;

	name = szl_obj_str(interp, objv[1], &len);
	if (!name || !len)
		return SZL_ERR;

	if (!szl_obj_len(interp, objv[2], &len))
		return SZL_ERR;

	list = szl_get(interp, name);
	if (!list)
		return SZL_ERR;

	if (!len) {
		szl_obj_unref(list);
		return SZL_OK;
	}

	if (!szl_obj_list(interp, objv[2], &items, &n) || (n > SIZE_MAX)) {
		szl_obj_unref(list);
		return SZL_ERR;
	}

	for (i = 0; i < n; ++i) {
		if (!szl_lappend(interp, list, items[i])) {
			szl_obj_unref(list);
			return SZL_ERR;
		}
	}

	szl_obj_unref(list);
	return SZL_OK;
}

static
enum szl_res szl_list_proc_index(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv)
{
	struct szl_obj **items;
	szl_int i;
	size_t n;

	if (!szl_obj_int(interp, objv[2], &i))
		return SZL_ERR;

	if (i < 0) {
		szl_set_result_fmt(interp, "bad index: "SZL_INT_FMT, i);
		return SZL_ERR;
	}

	if (!szl_obj_list(interp, objv[1], &items, &n))
		return SZL_ERR;

	if (i >= n) {
		szl_set_result_fmt(interp, "bad index: "SZL_INT_FMT, i);
		return SZL_ERR;
	}

	return szl_set_result(interp, szl_obj_ref(items[i]));
}

static
enum szl_res szl_list_proc_range(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv)
{
	struct szl_obj *list, **items;
	szl_int start, end, i;
	size_t n;

	if (!szl_obj_list(interp, objv[1], &items, &n) || (n > SIZE_MAX))
		return SZL_ERR;

	if (!szl_obj_int(interp, objv[2], &start))
		return SZL_ERR;

	if ((start < 0) || (start >= (szl_int)n)) {
		szl_set_result_fmt(interp, "bad start index: "SZL_INT_FMT, start);
		return SZL_ERR;
	}

	if (!szl_obj_int(interp, objv[3], &end))
		return SZL_ERR;

	if ((end < start) || (end >= (szl_int)n)) {
		szl_set_result_fmt(interp, "bad end index: "SZL_INT_FMT, end);
		return SZL_ERR;
	}

	list = szl_new_empty();
	if (!list)
		return SZL_ERR;

	for (i = start; i <= end; ++i) {
		if (!szl_lappend(interp, list, items[i])) {
			szl_obj_unref(list);
			return SZL_ERR;
		}
	}

	return szl_set_result(interp, list);
}

static
enum szl_res szl_list_proc_reverse(struct szl_interp *interp,
                                   const int objc,
                                   struct szl_obj **objv)
{
	struct szl_obj *list, **items;
	szl_int i;
	size_t n;

	if (!szl_obj_list(interp, objv[1], &items, &n) || (n > SIZE_MAX))
		return SZL_ERR;

	list = szl_new_empty();
	if (!list)
		return SZL_ERR;

	for (i = (szl_int)n - 1; i >= 0; --i) {
		if (!szl_lappend(interp, list, items[i])) {
			szl_obj_unref(list);
			return SZL_ERR;
		}
	}

	return szl_set_result(interp, list);
}

int szl_init_list(struct szl_interp *interp)
{
	return ((szl_new_proc(interp,
	                      "list.length",
	                      2,
	                      2,
	                      "list.length list",
	                      szl_list_proc_length,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "list.append",
	                      3,
	                      3,
	                      "list.append name item",
	                      szl_list_proc_append,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "list.extend",
	                      3,
	                      3,
	                      "list.extend name list",
	                      szl_list_proc_extend,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "list.index",
	                      3,
	                      3,
	                      "list.index list index",
	                      szl_list_proc_index,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "list.range",
	                      4,
	                      4,
	                      "list.range list start end",
	                      szl_list_proc_range,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "list.reverse",
	                      2,
	                      2,
	                      "list.reverse list",
	                      szl_list_proc_reverse,
	                      NULL,
	                      NULL)));
}
