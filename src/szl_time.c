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
#include <errno.h>
#include <math.h>

#include "szl.h"

static
enum szl_res szl_time_proc_sleep(struct szl_interp *interp,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	struct timespec req, rem;
	szl_float f;

	/* we assume sizeof(time_t) == 4 */
	if (!szl_as_float(interp, objv[1], &f) || (f < 0) || (f > INT_MAX))
		return SZL_ERR;

	req.tv_sec = (time_t)floor(f);
	req.tv_nsec = labs((long)(1000000000 * (f - (szl_float)req.tv_sec)));

	do {
		if (nanosleep(&req, &rem) == 0)
			break;

		if (errno != EINTR)
			return SZL_ERR;

		req.tv_sec = rem.tv_sec;
		req.tv_nsec = rem.tv_nsec;
	} while (1);

	return SZL_OK;
}

static
const struct szl_ext_export time_exports[] = {
	{
		SZL_PROC_INIT("sleep", "sec", 2, 2, szl_time_proc_sleep, NULL)
	}
};

int szl_init_time(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "time",
	                   time_exports,
	                   sizeof(time_exports) / sizeof(time_exports[0]));
}
