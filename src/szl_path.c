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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "szl.h"

static
enum szl_res szl_path_proc_exists(struct szl_interp *interp,
                                  const unsigned int objc,
                                  struct szl_obj **objv)
{
	struct stat stbuf;
	char *path;
	size_t len;

	if (!szl_as_str(interp, objv[1], &path, &len) || !len)
		return SZL_ERR;

	if (stat(path, &stbuf) < 0) {
		if (errno == ENOENT)
			return szl_set_last_bool(interp, 0);

		return SZL_ERR;
	}

	return szl_set_last_bool(interp, 1);
}

static
enum szl_res szl_path_proc_isdir(struct szl_interp *interp,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	struct stat stbuf;
	char *path;
	size_t len;

	if (!szl_as_str(interp, objv[1], &path, &len) || !len)
		return SZL_ERR;

	if (stat(path, &stbuf) < 0) {
		if (errno == ENOENT)
			return szl_set_last_bool(interp, 0);

		return SZL_ERR;
	}

	return szl_set_last_bool(interp, S_ISDIR(stbuf.st_mode));
}

static
enum szl_res szl_path_proc_realpath(struct szl_interp *interp,
                                    const unsigned int objc,
                                    struct szl_obj **objv)
{
	struct szl_obj *obj;
	char *rpath, *path;
	size_t len;

	if (!szl_as_str(interp, objv[1], &path, &len) || !len)
		return SZL_ERR;

	rpath = realpath(path, NULL);
	if (!rpath)
		return SZL_ERR;

	obj = szl_new_str_noalloc(rpath, strlen(rpath));
	if (!obj) {
		free(rpath);
		return SZL_ERR;
	}

	return szl_set_last(interp, obj);
}

static
enum szl_res szl_path_proc_join(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	struct szl_obj *path;

	path = szl_join(interp, interp->sep, &objv[1], (size_t)objc - 1, 0);
	if (!path)
		return SZL_ERR;

	return szl_set_last(interp, path);
}

static
const struct szl_ext_export path_exports[] = {
	{
		SZL_PROC_INIT("path.exists", "path", 2, 2, szl_path_proc_exists, NULL)
	},
	{
		SZL_PROC_INIT("path.isdir", "path", 2, 2, szl_path_proc_isdir, NULL)
	},
	{
		SZL_PROC_INIT("path.realpath",
		              "path",
		              2,
		              2,
		              szl_path_proc_realpath,
		              NULL)
	},
	{
		SZL_PROC_INIT("path.join",
		              "path path...",
		              3,
		              -1,
		              szl_path_proc_join,
		              NULL)
	},
	{
		SZL_STR_INIT("path.sep", "/")
	}
};

int szl_init_path(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "path",
	                   path_exports,
	                   sizeof(path_exports) / sizeof(path_exports[0]));
}
