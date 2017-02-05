/*
 * this file is part of szl.
 *
 * Copyright (c) 2016, 2017 Dima Krasner
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
#include <time.h>

#include "szl.h"

static
enum szl_res szl_list_proc_new(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	struct szl_obj *list;

	list = szl_new_list(&objv[1], objc - 1);
	if (!list)
		return SZL_ERR;

	return szl_set_last(interp, list);
}

static
enum szl_res szl_list_proc_len(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	struct szl_obj **items;
	size_t n;

	if (!szl_as_list(interp, objv[1], &items, &n))
		return SZL_ERR;

	return szl_set_last_int(interp, (szl_int)n);
}

static
enum szl_res szl_list_proc_append(struct szl_interp *interp,
                                  const unsigned int objc,
                                  struct szl_obj **objv)
{
	return szl_list_append(interp, objv[1], objv[2]) ? SZL_OK : SZL_ERR;
}

static
enum szl_res szl_list_proc_set(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	szl_int index;

	if (!szl_as_int(interp, objv[2], &index) ||
	    !szl_list_set(interp, objv[1], index, objv[3]))
		return SZL_ERR;

	return SZL_OK;
}

static
enum szl_res szl_list_proc_extend(struct szl_interp *interp,
                                  const unsigned int objc,
                                  struct szl_obj **objv)
{
	if (!szl_list_extend(interp, objv[1], objv[2]))
		return SZL_ERR;

	return szl_set_last(interp, szl_ref(objv[1]));
}

static
enum szl_res szl_list_proc_index(struct szl_interp *interp,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	struct szl_obj **items;
	szl_int i;
	size_t n;

	if (!szl_as_int(interp, objv[2], &i))
		return SZL_ERR;

	if (i < 0) {
		szl_set_last_fmt(interp, "bad index: "SZL_INT_FMT"d", i);
		return SZL_ERR;
	}

	if (!szl_as_list(interp, objv[1], &items, &n))
		return SZL_ERR;

	if (i >= n) {
		szl_set_last_fmt(interp, "bad index: "SZL_INT_FMT"d", i);
		return SZL_ERR;
	}

	return szl_set_last(interp, szl_ref(items[i]));
}

static
enum szl_res szl_list_proc_range(struct szl_interp *interp,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	struct szl_obj *list, **items;
	szl_int start, end, i;
	size_t n;

	if (!szl_as_list(interp, objv[1], &items, &n) || (n > SIZE_MAX))
		return SZL_ERR;

	if (!szl_as_int(interp, objv[2], &start))
		return SZL_ERR;

	if ((start < 0) || (start >= (szl_int)n)) {
		szl_set_last_fmt(interp, "bad start index: "SZL_INT_FMT"d", start);
		return SZL_ERR;
	}

	if (!szl_as_int(interp, objv[3], &end))
		return SZL_ERR;

	if ((end < start) || (end >= (szl_int)n)) {
		szl_set_last_fmt(interp, "bad end index: "SZL_INT_FMT"d", end);
		return SZL_ERR;
	}

	list = szl_new_list(NULL, 0);
	if (!list)
		return SZL_ERR;

	for (i = start; i <= end; ++i) {
		if (!szl_list_append(interp, list, items[i])) {
			szl_unref(list);
			return SZL_ERR;
		}
	}

	return szl_set_last(interp, list);
}

static
enum szl_res szl_list_proc_in(struct szl_interp *interp,
                              const unsigned int objc,
                              struct szl_obj **objv)
{
	int found = 0;

	if (!szl_list_in(interp, objv[2], objv[1], &found))
		return SZL_ERR;

	return szl_set_last_bool(interp, found);
}

static
enum szl_res szl_list_proc_reverse(struct szl_interp *interp,
                                   const unsigned int objc,
                                   struct szl_obj **objv)
{
	struct szl_obj *list, **items;
	szl_int i;
	size_t n;

	if (!szl_as_list(interp, objv[1], &items, &n) || (n > SIZE_MAX))
		return SZL_ERR;

	list = szl_new_list(NULL, 0);
	if (!list)
		return SZL_ERR;

	for (i = (szl_int)n - 1; i >= 0; --i) {
		if (!szl_list_append(interp, list, items[i])) {
			szl_unref(list);
			return SZL_ERR;
		}
	}

	return szl_set_last(interp, list);
}

static
enum szl_res szl_list_proc_join(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	struct szl_obj *str, **items;
	char *delim, *s;
	size_t i, n, len, dlen;

	if (!szl_as_str(interp, objv[1], &delim, &dlen) ||
	    !szl_as_list(interp, objv[2], &items, &n) ||
	    (n > SIZE_MAX))
		return SZL_ERR;

	str = szl_new_empty();
	if (!str)
		return SZL_ERR;

	for (i = 0; i < n; ++i) {
		if (!szl_as_str(interp, items[i], &s, &len) ||
		    !szl_str_append_str(interp, str, s, len) ||
		   ((i < n - 1) && !szl_str_append_str(interp, str, delim, dlen))) {
			szl_unref(str);
			return SZL_ERR;
		}
	}

	return szl_set_last(interp, str);
}

static
enum szl_res szl_list_proc_zip(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	struct szl_obj *list, *item, ***items;
	size_t *ns, j;
	int n, i;

	n = objc - 1;

	items = (struct szl_obj ***)malloc(sizeof(struct szl_obj **) * n);
	if (!items)
		return SZL_ERR;

	ns = (size_t *)malloc(sizeof(size_t) * n);
	if (!ns) {
		free(items);
		return SZL_ERR;
	}

	for (i = 1; i < objc; ++i) {
	    if (!szl_as_list(interp, objv[i], &items[i - 1], &ns[i - 1])) {
			free(ns);
			free(items);
			return SZL_ERR;
		}
	}

	for (i = 1; i < n; ++i) {
		if (ns[i] != ns[0]) {
			free(ns);
			free(items);
			szl_set_last_str(interp, "cannot zip lists of different len", -1);
			return SZL_ERR;
		}
	}

	list = szl_new_empty();
	if (!list) {
		free(ns);
		free(items);
		return SZL_ERR;
	}

	for (j = 0; j < ns[0]; ++j) {
		item = szl_new_empty();
		if (!item) {
			szl_unref(list);
			free(ns);
			free(items);
			return SZL_ERR;
		}

		for (i = 0; i < n; ++i) {
			if (!szl_list_append(interp, item, items[i][j])) {
				szl_unref(item);
				szl_unref(list);
				free(ns);
				free(items);
				return SZL_ERR;
			}
		}

		if (!szl_list_append(interp, list, item)) {
			szl_unref(item);
			szl_unref(list);
			free(ns);
			free(items);
			return SZL_ERR;
		}

		szl_unref(item);
	}

	free(ns);
	free(items);
	return szl_set_last(interp, list);
}

static
enum szl_res szl_list_proc_uniq(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	struct szl_obj **items, *uniq;
	size_t len, i;
	int in;

	if (!szl_as_list(interp, objv[1], &items, &len))
		return SZL_ERR;

	uniq = szl_new_list(items, len ? 1 : 0);
	if (!uniq)
		return SZL_ERR;

	for (i = 1; i < len; ++i) {
		if (!szl_list_in(interp, items[i], uniq, &in) ||
		    (!in && !szl_list_append(interp, uniq, items[i]))) {
			szl_unref(uniq);
			return SZL_ERR;
		}
	}

	return szl_set_last(interp, uniq);
}

static
const struct szl_ext_export list_exports[] = {
	{
		SZL_PROC_INIT("list.new", "?item...?", 1, -1, szl_list_proc_new, NULL)
	},
	{
		SZL_PROC_INIT("list.len", "list", 2, 2, szl_list_proc_len, NULL)
	},
	{
		SZL_PROC_INIT("list.append",
		              "list item",
		              3,
		              3,
		              szl_list_proc_append,
		              NULL)
	},
	{
		SZL_PROC_INIT("list.set",
		              "list index item",
		              4,
		              4,
		              szl_list_proc_set,
		              NULL)
	},
	{
		SZL_PROC_INIT("list.extend",
		              "list list",
		              3,
		              3,
		              szl_list_proc_extend,
		              NULL)
	},
	{
		SZL_PROC_INIT("list.index",
		              "list index",
		              3,
		              3,
		              szl_list_proc_index,
		              NULL)
	},
	{
		SZL_PROC_INIT("list.range",
		              "list start end",
		              4,
		              4,
		              szl_list_proc_range,
		              NULL)
	},
	{
		SZL_PROC_INIT("list.in",
		              "list item",
		              3,
		              3,
		              szl_list_proc_in,
		              NULL)
	},
	{
		SZL_PROC_INIT("list.reverse",
		              "list",
		              2,
		              2,
		              szl_list_proc_reverse,
		              NULL)
	},
	{
		SZL_PROC_INIT("list.join",
		              "delim list",
		              3,
		              3,
		              szl_list_proc_join,
		              NULL)
	},
	{
		SZL_PROC_INIT("zip",
		              "list list...",
		              3,
		              -1,
		              szl_list_proc_zip,
		              NULL)
	},
	{
		SZL_PROC_INIT("uniq",
		              "list",
		              2,
		              2,
		              szl_list_proc_uniq,
		              NULL)
	}
};

int szl_init_list(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "list",
	                   list_exports,
	                   sizeof(list_exports) / sizeof(list_exports[0]));
}
