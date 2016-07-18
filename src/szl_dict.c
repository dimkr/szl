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
#include <signal.h>
#include <string.h>

#include "szl.h"

static
enum szl_res szl_dict_proc_new(struct szl_interp *interp,
                               const int objc,
                               struct szl_obj **objv)
{
	struct szl_obj *list;
	int i;

	if (objc % 2 == 0)
		return szl_usage(interp, objv[0]);

	list = szl_new_empty();
	if (!list)
		return SZL_ERR;

	for (i = 1; i < objc; i += 2) {
		if (!szl_lappend(interp, list, objv[i]) ||
		    !szl_lappend(interp, list, objv[i + 1])) {
			szl_obj_unref(list);
			return SZL_ERR;
		}
	}

	return szl_set_result(interp, list);
}

static
enum szl_res szl_dict_proc_get(struct szl_interp *interp,
                               const int objc,
                               struct szl_obj **objv)
{
	struct szl_obj **items;
	size_t n, i;
	int eq;

	if (!szl_obj_list(interp, objv[1], &items, &n))
		return SZL_ERR;

	if (n % 2 == 1) {
		szl_set_result_str(interp, "bad dict", -1);
		return SZL_ERR;
	}

	for (i = 0; i < n; i += 2) {
		if (!szl_obj_eq(interp, items[i], objv[2], &eq))
			return SZL_ERR;

		if (eq)
			return szl_set_result(interp, szl_obj_ref(items[i + 1]));
	}

	if (objc == 4)
		return szl_set_result(interp, szl_obj_ref(objv[3]));

	return SZL_ERR;
}

static
enum szl_res szl_dict_proc_set(struct szl_interp *interp,
                               const int objc,
                               struct szl_obj **objv)
{
	struct szl_obj **items;
	size_t n, i;
	int eq;

	if (!szl_obj_list(interp, objv[1], &items, &n))
		return SZL_ERR;

	if (n % 2 == 1) {
		szl_set_result_str(interp, "bad dict", -1);
		return SZL_ERR;
	}

	for (i = 0; i < n; i += 2) {
		if (!szl_obj_eq(interp, items[i], objv[2], &eq))
			return SZL_ERR;

		if (eq) {
			if (!szl_lset(interp, objv[1], (szl_int)i + 1, objv[3]))
				return SZL_ERR;

			return SZL_OK;
		}
	}

	if (!szl_lappend(interp, objv[1], objv[2]) ||
	    !szl_lappend(interp, objv[1], objv[3]))
		return SZL_ERR;

	return SZL_OK;
}

int szl_init_dict(struct szl_interp *interp)
{
	return (szl_new_proc(interp,
	                     "dict.new",
	                     0,
	                     -1,
	                     "dict.new ?k v?...",
	                     szl_dict_proc_new,
	                     NULL,
	                     NULL) &&
	        szl_new_proc(interp,
	                     "dict.get",
	                     3,
	                     4,
	                     "dict.get dict k ?v?",
	                     szl_dict_proc_get,
	                     NULL,
	                     NULL) &&
	        szl_new_proc(interp,
	                     "dict.set",
	                     4,
	                     4,
	                     "dict.get dict k v",
	                     szl_dict_proc_set,
	                     NULL,
	                     NULL));
}
