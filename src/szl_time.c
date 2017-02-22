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
#include <string.h>
#include <time.h>
#include <errno.h>
#include <math.h>

#include "szl.h"

#define SZL_STRFTIME_BUFSIZ 128

struct szl_timestamp {
	struct tm tm;
	time_t t;
};

static
enum szl_res szl_time_proc_sleep(struct szl_interp *interp,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	struct timespec req, rem;
	szl_float f;

	/* we assume sizeof(time_t) == 4 */
	if (!szl_as_float(interp, objv[1], &f) || (f < 0) || (f > UINT_MAX))
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
enum szl_res szl_time_proc_now(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	time_t t;

	if (time(&t) == (time_t)-1)
		return SZL_ERR;

	return szl_set_last_float(interp, (szl_float)t);
}

static
enum szl_res szl_timestamp_proc(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	struct szl_timestamp *ts = (struct szl_timestamp *)objv[0]->priv;
	struct szl_obj *obj;
	char *s, *buf;
	size_t len;

	if (!szl_as_str(interp, objv[1], &s, NULL))
		return SZL_ERR;

	if (strcmp(s, "format") != 0)
		return szl_set_last_help(interp, objv[0]);

	if (!szl_as_str(interp, objv[2], &s, &len))
		return SZL_ERR;

	if (!len)
		return SZL_OK;

	buf = (char *)malloc(SZL_STRFTIME_BUFSIZ);
	if (!buf)
		return SZL_ERR;

	buf[0] = '\0';
	len = strftime(buf, SZL_STRFTIME_BUFSIZ, s, &ts->tm);
	if (!len && !buf[0]) {
		free(buf);
		return SZL_ERR;
	}

	obj = szl_new_str_noalloc(buf, len);
	if (!obj) {
		free(buf);
		return SZL_ERR;
	}

	return szl_set_last(interp, obj);
}

static
enum szl_res szl_time_proc_timestamp(struct szl_interp *interp,
                                     const unsigned int objc,
                                     struct szl_obj **objv)
{
	struct szl_timestamp *ts;
	struct szl_obj *name, *proc;
	szl_float f;

	if (!szl_as_float(interp, objv[1], &f) || (f < 0) || (f > UINT_MAX))
		return SZL_ERR;

	ts = (struct szl_timestamp *)malloc(sizeof(*ts));
	if (!ts)
		return SZL_ERR;

	ts->t = (time_t)round(f);
	if (!gmtime_r(&ts->t, &ts->tm)) {
		free(ts);
		return SZL_ERR;
	}

	name = szl_new_str_fmt("timestamp:%p", ts);
	if (!name) {
		free(ts);
		return SZL_ERR;
	}

	proc = szl_new_proc(interp,
	                    name,
	                    3,
	                    3,
	                    "format fmt",
	                    szl_timestamp_proc,
	                    free,
	                    ts);
	if (!proc) {
		szl_free(name);
		free(ts);
		return SZL_ERR;
	}

	szl_unref(name);
	return szl_set_last(interp, proc);
}


static
const struct szl_ext_export time_exports[] = {
	{
		SZL_PROC_INIT("sleep", "sec", 2, 2, szl_time_proc_sleep, NULL)
	},
	{
		SZL_PROC_INIT("time.now", NULL, 1, 1, szl_time_proc_now, NULL)
	},
	{
		SZL_PROC_INIT("time.timestamp",
		              "sec",
		              2,
		              2,
		              szl_time_proc_timestamp,
		              NULL)
	}
};

int szl_init_time(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "time",
	                   time_exports,
	                   sizeof(time_exports) / sizeof(time_exports[0]));
}
