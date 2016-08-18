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

#include "szl.h"

static
enum szl_res szl_env_proc_get(struct szl_interp *interp,
                              const unsigned int objc,
                              struct szl_obj **objv)
{
	char *name, *val;
	size_t len;

	if (!szl_as_str(interp, objv[1], &name, &len) || !len)
		return SZL_ERR;

	val = getenv(name);
	if (!val)
		return SZL_ERR;

	return szl_set_last_str(interp, val, -1);
}

static
const struct szl_ext_export env_exports[] = {
	{
		SZL_STR_INIT("env.ver", SZL_VERSION)
	},
	{
		SZL_INT_INIT("env.intmax", INT_MAX)
	},
	{
		SZL_PROC_INIT("env.get", "name", 2, 2, szl_env_proc_get, NULL)
	}
};

int szl_init_env(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "env",
	                   env_exports,
	                   sizeof(env_exports) / sizeof(env_exports[0]));
}
