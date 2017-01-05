/*
 * this file is part of szl.
 *
 * Copyright (c) 2017 Dima Krasner
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

#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <string.h>

#include "szl.h"

static
enum szl_res szl_syscall_proc_syscall(struct szl_interp *interp,
                                      const unsigned int objc,
                                      struct szl_obj **objv)
{
	szl_int args[7];
	long ret;
	unsigned int i, j = 0;

	for (i = 1; i < objc; ++i) {
		if (!szl_as_int(interp, objv[i], &args[j]) ||
		    (args[j] < LONG_MIN) ||
		    (args[j] > LONG_MAX))
			return SZL_ERR;
		++j;
	}

	for (; j < 7; ++j)
		args[j] = 0;

	ret = syscall((long)args[0],
	              (long)args[1],
	              (long)args[2],
	              (long)args[3],
	              (long)args[4],
	              (long)args[5],
	              (long)args[6]);
	if (ret < 0) {
		szl_set_last_str(interp, strerror(errno), -1);
		return SZL_ERR;
	}

	return szl_set_last_int(interp, (szl_int)ret);
}

static
const struct szl_ext_export syscall_exports[] = {
	{
		SZL_PROC_INIT("syscall",
		              "num arg...",
		              2,
		              8,
		              szl_syscall_proc_syscall,
		              NULL)
	}
};

int szl_init_syscall(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "syscall",
	                   syscall_exports,
	                   sizeof(syscall_exports) / sizeof(syscall_exports[0]));
}
