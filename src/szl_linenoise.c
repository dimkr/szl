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

#include <string.h>
#include <sys/types.h>
#include <stdlib.h>

#include "linenoise/linenoise.h"

#include "szl.h"

#define SZL_LINENOISE_HISTORY_SIZE (64)

static
enum szl_res szl_linenoise_proc_read(struct szl_interp *interp,
                                     const unsigned int objc,
                                     struct szl_obj **objv)
{
	struct szl_obj *obj;
	char *prompt;
	char *s;

	if (!szl_as_str(interp, objv[1], &prompt, NULL))
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
	return szl_set_last(interp, obj);
}

static
enum szl_res szl_linenoise_proc_add(struct szl_interp *interp,
                                    const unsigned int objc,
                                    struct szl_obj **objv)
{
	char *line;
	size_t len;

	if (!szl_as_str(interp, objv[1], &line, &len) || !len)
		return SZL_ERR;

	return linenoiseHistoryAdd(line) ? SZL_OK : SZL_ERR;
}

static
enum szl_res szl_linenoise_proc_save(struct szl_interp *interp,
                                     const unsigned int objc,
                                     struct szl_obj **objv)
{
	char *path;
	size_t len;

	if (!szl_as_str(interp, objv[1], &path, &len) || !len)
		return SZL_ERR;

	return (linenoiseHistorySave(path) == 0) ? SZL_OK : SZL_ERR;
}

static
enum szl_res szl_linenoise_proc_load(struct szl_interp *interp,
                                     const unsigned int objc,
                                     struct szl_obj **objv)
{
	char *path;
	size_t len;

	if (!szl_as_str(interp, objv[1], &path, &len) || !len)
		return SZL_ERR;

	return (linenoiseHistoryLoad(path) == 0) ? SZL_OK : SZL_ERR;
}

static
const struct szl_ext_export linenoise_exports[] = {
	{
		SZL_PROC_INIT("linenoise.read",
		              "prompt",
		              2,
		              2,
		              szl_linenoise_proc_read,
		              NULL)
	},
	{
		SZL_PROC_INIT("linenoise.add",
		              "line",
		              2,
		              2,
		              szl_linenoise_proc_add,
		              NULL)
	},
	{
		SZL_PROC_INIT("linenoise.save",
		              "path",
		              2,
		              2,
		              szl_linenoise_proc_save,
		              NULL)
	},
	{
		SZL_PROC_INIT("linenoise.load",
		              "path",
		              2,
		              2,
		              szl_linenoise_proc_load,
		              NULL)
	}
};

int szl_init_linenoise(struct szl_interp *interp)
{
	return (linenoiseHistorySetMaxLen(SZL_LINENOISE_HISTORY_SIZE) &&
	        szl_new_ext(
	                 interp,
	                 "linenoise",
	                 linenoise_exports,
	                 sizeof(linenoise_exports) / sizeof(linenoise_exports[0])));
}
