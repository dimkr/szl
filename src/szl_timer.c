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
#include <unistd.h>
#include <fcntl.h>
#include <sys/timerfd.h>
#include <math.h>

#include "szl.h"


static
enum szl_res szl_timer_unblock(void *priv)
{
	int fl, fd = (int)(intptr_t)priv;

	fl = fcntl(fd, F_GETFL);
	if ((fl < 0) || (fcntl(fd, F_SETFL, fl | O_NONBLOCK) < 0))
		return SZL_ERR;

	return SZL_OK;
}

static
void szl_timer_close(void *priv)
{
	close((int)(intptr_t)priv);
}

static
szl_int szl_timer_handle(void *priv)
{
	return (szl_int)(intptr_t)priv;
}

static
const struct szl_stream_ops szl_timer_ops = {
	.unblock = szl_timer_unblock,
	.close = szl_timer_close,
	.handle = szl_timer_handle
};

static
enum szl_res szl_timer_proc_timer(struct szl_interp *interp,
                                  const int objc,
                                  struct szl_obj **objv)
{
	struct itimerspec its;
	struct szl_stream *strm;
	struct szl_obj *obj;
	szl_double timeout;
	int fd;

	if (!szl_obj_double(interp, objv[1], &timeout))
		return SZL_ERR;

	fd = timerfd_create(CLOCK_MONOTONIC, 0);
	if (fd < 0)
		return SZL_ERR;

	its.it_value.tv_sec = (time_t)floor(timeout);
	its.it_value.tv_nsec = labs(
	          (long)(1000000000 * (timeout - (szl_double)its.it_value.tv_sec)));
	its.it_interval.tv_sec = its.it_value.tv_sec;
	its.it_interval.tv_nsec = its.it_value.tv_nsec;
	if (timerfd_settime(fd, 0, &its, NULL) < 0) {
		close(fd);
		return SZL_ERR;
	}

	strm = (struct szl_stream *)malloc(sizeof(struct szl_stream));
	if (!strm) {
		close(fd);
		return SZL_ERR;
	}

	strm->ops = &szl_timer_ops;
	strm->keep = 0;
	strm->closed = 0;
	strm->priv = (void *)(intptr_t)fd;
	strm->buf = NULL;
	strm->blocking = 1;

	obj = szl_new_stream(interp, strm, "timer");
	if (!obj) {
		szl_stream_free(strm);
		return SZL_ERR;
	}

	return szl_set_result(interp, obj);
}

int szl_init_timer(struct szl_interp *interp)
{
	return szl_new_proc(interp,
	                    "timer",
	                    2,
	                    2,
	                    "timer timeout",
	                    szl_timer_proc_timer,
	                    NULL,
	                    NULL) ? 1 : 0;
}
