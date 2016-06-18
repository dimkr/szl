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

#include <string.h>
#include <sys/types.h>

#include "linenoise/linenoise.h"

#include "szl.h"

#define SZL_LINENOISE_HISTORY_SIZE (64)

static
enum szl_res szl_linenoise_proc_read(struct szl_interp *interp,
                                     const int objc,
                                     struct szl_obj **objv)
{
	struct szl_obj *obj;
	const char *prompt;
	char *s;

	prompt = szl_obj_str(interp, objv[1], NULL);
	if (!prompt)
		return SZL_ERR;

	s = linenoise(prompt);
	if (!s)
		return SZL_ERR;

	obj = szl_new_str_noalloc(s, strlen(s));
	if (!obj) {
		linenoiseFree(s);
		return SZL_ERR;
	}

	/* we assume linenoiseFree() calls free(), since we don't use a custom
	 * allocator */
	return szl_set_result(interp, obj);
}

static
enum szl_res szl_linenoise_proc_add(struct szl_interp *interp,
                                    const int objc,
                                    struct szl_obj **objv)
{
	const char *line;
	size_t len;

	line = szl_obj_str(interp, objv[1], &len);
	if (!line || !len)
		return SZL_ERR;

	return linenoiseHistoryAdd(line) ? SZL_OK : SZL_ERR;
}

static
enum szl_res szl_linenoise_proc_save(struct szl_interp *interp,
                                     const int objc,
                                     struct szl_obj **objv)
{
	const char *path;
	size_t len;

	path = szl_obj_str(interp, objv[1], &len);
	if (!path || !len)
		return SZL_ERR;

	return (linenoiseHistorySave(path) == 0) ? SZL_OK : SZL_ERR;
}

static
enum szl_res szl_linenoise_proc_load(struct szl_interp *interp,
                                     const int objc,
                                     struct szl_obj **objv)
{
	const char *path;
	size_t len;

	path = szl_obj_str(interp, objv[1], &len);
	if (!path || !len)
		return SZL_ERR;

	return (linenoiseHistoryLoad(path) == 0) ? SZL_OK : SZL_ERR;
}

int szl_init_linenoise(struct szl_interp *interp)
{
	if (!linenoiseHistorySetMaxLen(SZL_LINENOISE_HISTORY_SIZE))
		return SZL_ERR;

	return ((szl_new_proc(interp,
	                      "linenoise.read",
	                      2,
	                      2,
	                      "linenoise.read prompt",
	                      szl_linenoise_proc_read,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "linenoise.add",
	                      2,
	                      2,
	                      "linenoise.add line",
	                      szl_linenoise_proc_add,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "linenoise.save",
	                      2,
	                      2,
	                      "linenoise.save path",
	                      szl_linenoise_proc_save,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "linenoise.load",
	                      2,
	                      2,
	                      "linenoise.load path",
	                      szl_linenoise_proc_load,
	                      NULL,
	                      NULL)));
}
