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
#include <time.h>
#include <math.h>
#include <errno.h>
#include <dlfcn.h>

#include "szl.h"

static enum szl_res szl_posix_proc_sleep(struct szl_interp *interp,
                                         const int objc,
                                         struct szl_obj **objv,
                                         struct szl_obj **ret)
{
	struct timespec req, rem;
	szl_double d;

	/* we assume sizeof(time_t) == 4 */
	if ((szl_obj_double(objv[1], &d) != SZL_OK) || (d < 0) || (d > INT_MAX))
		return SZL_ERR;

	req.tv_sec = (time_t)floor(d);
	req.tv_nsec = labs((long)(1000000000 * (d - (szl_double)req.tv_sec)));

	do {
		if (nanosleep(&req, &rem) == 0)
			break;

		if (errno != EINTR)
			return SZL_ERR;

		req.tv_sec = rem.tv_sec;
		req.tv_nsec = rem.tv_nsec;
	} while(1);

	return SZL_OK;
}

enum szl_res szl_init_posix(struct szl_interp *interp)
{
	if (!szl_new_proc(interp,
	                  "sleep",
	                  2,
	                  2,
	                  "sleep sec",
	                  szl_posix_proc_sleep,
	                  NULL,
	                  NULL))
		return SZL_ERR;

	return SZL_OK;
}
