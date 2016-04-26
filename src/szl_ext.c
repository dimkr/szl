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

static enum szl_res szl_ext_proc_load(struct szl_interp *interp,
                                      const int objc,
                                      struct szl_obj **objv,
                                      struct szl_obj **ret)
{
	const char *name;
	size_t len;

	name = szl_obj_str(objv[1], &len);
	if (!name || !len)
		return SZL_ERR;

	return szl_load(interp, name);
}

static enum szl_res szl_ext_proc_source(struct szl_interp *interp,
                                        const int objc,
                                        struct szl_obj **objv,
                                        struct szl_obj **ret)
{
	const char *path;
	size_t len;

	path = szl_obj_str(objv[1], &len);
	if (!path || !len)
		return SZL_ERR;

	return szl_source(interp, path);
}

enum szl_res szl_init_ext(struct szl_interp *interp)
{
	if ((!szl_new_proc(interp,
	                   "load",
	                   2,
	                   2,
	                   "load name",
	                   szl_ext_proc_load,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "source",
	                   2,
	                   2,
	                   "source path",
	                   szl_ext_proc_source,
	                   NULL,
	                   NULL)))
		return SZL_ERR;

	return SZL_OK;
}
