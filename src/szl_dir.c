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

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "szl.h"

static const char szl_dir_inc[] = {
#include "szl_dir.inc"
};

static
enum szl_res szl_dir_proc_create(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv)
{
	const char *path;
	size_t len;

	path = szl_obj_str(interp, objv[1], &len);
	if (!path || !len)
		return SZL_ERR;

	if (mkdir(path, 0755) < 0)
		return SZL_ERR;

	return SZL_OK;
}

static
enum szl_res szl_dir_proc_delete(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv)
{
	const char *path;
	size_t len;

	path = szl_obj_str(interp, objv[1], &len);
	if (!path || !len)
		return SZL_ERR;

	if (rmdir(path) < 0)
		return SZL_ERR;

	return SZL_OK;
}

static
enum szl_res szl_dir_proc_cd(struct szl_interp *interp,
                             const int objc,
                             struct szl_obj **objv)
{
	const char *path;
	size_t len;

	path = szl_obj_str(interp, objv[1], &len);
	if (!path || !len)
		return SZL_ERR;

	if (chdir(path) < 0)
		return SZL_ERR;

	return SZL_OK;
}

static
enum szl_res szl_dir_proc_list(struct szl_interp *interp,
                               const int objc,
                               struct szl_obj **objv)
{
	struct dirent ent;
	struct szl_obj *names[2], *obj;
	const char *s;
	struct dirent *entp;
	DIR *dir;
	const char *path;
	size_t len;
	int i, out;

	path = szl_obj_str(interp, objv[1], &len);
	if (!path || !len)
		return SZL_ERR;

	names[0] = szl_new_empty();
	if (!names[0])
		return SZL_ERR;

	names[1] = szl_new_empty();
	if (!names[1])
		return SZL_ERR;

	obj = szl_new_empty();
	if (!obj)
		return SZL_ERR;

	dir = opendir(path);
	if (!dir)
		return SZL_ERR;

	do {
		if (readdir_r(dir, &ent, &entp) != 0) {
			szl_obj_unref(obj);
			szl_obj_unref(names[1]);
			szl_obj_unref(names[0]);
			closedir(dir);
			return SZL_ERR;
		}

		if (!entp)
			break;

		if (entp->d_type == DT_DIR) {
			if (((entp->d_name[0] == '.') && (entp->d_name[1] == '\0')) ||
			    ((entp->d_name[0] == '.') &&
			     (entp->d_name[1] == '.') &&
			     (entp->d_name[2] == '\0')))
				continue;

			out = szl_lappend_str(interp, names[0], entp->d_name);
		}
		else
			out = szl_lappend_str(interp, names[1], entp->d_name);

		if (!out) {
			szl_obj_unref(obj);
			szl_obj_unref(names[1]);
			szl_obj_unref(names[0]);
			closedir(dir);
			return SZL_ERR;
		}
	} while (1);

	for (i = 0; i < 2; ++i) {
		s = szl_obj_str(interp, names[i], NULL);
		if (!s || !szl_lappend_str(interp, obj, s)) {
			szl_obj_unref(obj);
			szl_obj_unref(names[1]);
			szl_obj_unref(names[0]);
			closedir(dir);
			return SZL_ERR;
		}
	}

	closedir(dir);
	szl_obj_unref(names[1]);
	szl_obj_unref(names[0]);
	return szl_set_result(interp, obj);
}

int szl_init_dir(struct szl_interp *interp)
{
	return ((szl_new_proc(interp,
	                      "dir.create",
	                      2,
	                      2,
	                      "dir.create path",
	                      szl_dir_proc_create,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "dir.delete",
	                      2,
	                      2,
	                      "dir.delete path",
	                      szl_dir_proc_delete,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "cd",
	                      2,
	                      2,
	                      "cd path",
	                      szl_dir_proc_cd,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "dir.list",
	                      2,
	                      2,
	                      "dir.list path",
	                      szl_dir_proc_list,
	                      NULL,
	                      NULL)) &&
	        (szl_run(interp, szl_dir_inc, sizeof(szl_dir_inc) - 1) == SZL_OK));
}
