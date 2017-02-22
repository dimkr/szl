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
#include <signal.h>
#include <string.h>

#include "szl.h"

static
enum szl_res szl_dict_proc_new(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	struct szl_obj *dict;
	unsigned int i;

	if (objc % 2 == 0)
		return szl_set_last_help(interp, objv[0]);

	dict = szl_new_list(NULL, 0);
	if (!dict)
		return SZL_ERR;

	for (i = 1; i < objc; i += 2) {
		if (!szl_dict_set(interp, dict, objv[i], objv[i + 1])) {
			szl_free(dict);
			return SZL_ERR;
		}
	}

	return szl_set_last(interp, dict);
}

static
enum szl_res szl_dict_proc_get(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	char *s;
	size_t len;
	struct szl_obj *v;

	if (szl_dict_get(interp, objv[1], objv[2], &v)) {
		if (v)
			return szl_set_last(interp, v);

		if (objc == 4)
			return szl_set_last(interp, szl_ref(objv[3]));

		if (szl_as_str(interp, objv[2], &s, &len))
			szl_set_last_fmt(interp, "bad key: %s", s);
		else
			szl_set_last_str(interp, "bad key", sizeof("bad key") - 1);
	}

	return SZL_ERR;
}

static
enum szl_res szl_dict_proc_set(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	return szl_dict_set(interp, objv[1], objv[2], objv[3]) ? SZL_OK : SZL_ERR;
}

static
const struct szl_ext_export dict_exports[] = {
	{
		SZL_PROC_INIT("dict.new", "?k v?...", 1, -1, szl_dict_proc_new, NULL)
	},
	{
		SZL_PROC_INIT("dict.get", "dict k ?v?", 3, 4, szl_dict_proc_get, NULL)
	},
	{
		SZL_PROC_INIT("dict.set", "dict k v", 4, 4, szl_dict_proc_set, NULL)
	}
};

int szl_init_dict(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "dict",
	                   dict_exports,
	                   sizeof(dict_exports) / sizeof(dict_exports[0]));
}
