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

struct szl_timer {
	int fd;
	struct itimerspec its;
};

static
enum szl_res szl_timer_unblock(void *priv)
{
	int fl, fd = ((struct szl_timer *)priv)->fd;

	fl = fcntl(fd, F_GETFL);
	if ((fl < 0) || (fcntl(fd, F_SETFL, fl | O_NONBLOCK) < 0))
		return SZL_ERR;

	return SZL_OK;
}

static
enum szl_res szl_timer_rewind(void *priv)
{
	struct szl_timer *timer = (struct szl_timer *)priv;

	return (timerfd_settime(timer->fd,
	                        0,
	                        &timer->its,
	                        NULL) < 0) ? SZL_ERR : SZL_OK;
}

static
void szl_timer_close(void *priv)
{
	close(((struct szl_timer *)priv)->fd);
	free(priv);
}

static
szl_int szl_timer_handle(void *priv)
{
	return ((struct szl_timer *)priv)->fd;
}

static
const struct szl_stream_ops szl_timer_ops = {
	.unblock = szl_timer_unblock,
	.rewind = szl_timer_rewind,
	.close = szl_timer_close,
	.handle = szl_timer_handle
};

static
enum szl_res szl_timer_proc_timer(struct szl_interp *interp,
                                  const unsigned int objc,
                                  struct szl_obj **objv)
{
	struct szl_timer *timer;
	struct szl_stream *strm;
	struct szl_obj *obj;
	szl_float timeout;

	if (!szl_as_float(interp, objv[1], &timeout))
		return SZL_ERR;

	timer = (struct szl_timer *)malloc(sizeof(*timer));
	if (!timer)
		return SZL_ERR;

	timer->fd = timerfd_create(CLOCK_MONOTONIC, 0);
	if (timer->fd < 0) {
		free(timer);
		return SZL_ERR;
	}

	timer->its.it_value.tv_sec = (time_t)floor(timeout);
	timer->its.it_value.tv_nsec = labs(
	    (long)(1000000000 * (timeout - (szl_float)timer->its.it_value.tv_sec)));
	timer->its.it_interval.tv_sec = timer->its.it_value.tv_sec;
	timer->its.it_interval.tv_nsec = timer->its.it_value.tv_nsec;
	if (timerfd_settime(timer->fd, 0, &timer->its, NULL) < 0) {
		close(timer->fd);
		free(timer);
		return SZL_ERR;
	}

	strm = (struct szl_stream *)malloc(sizeof(struct szl_stream));
	if (!strm) {
		close(timer->fd);
		free(timer);
		return SZL_ERR;
	}

	strm->ops = &szl_timer_ops;
	strm->keep = 0;
	strm->closed = 0;
	strm->priv = timer;
	strm->buf = NULL;
	strm->blocking = 1;

	obj = szl_new_stream(interp, strm, "timer");
	if (!obj) {
		szl_stream_free(strm);
		return SZL_ERR;
	}

	return szl_set_last(interp, obj);
}

static
const struct szl_ext_export timer_exports[] = {
	{
		SZL_PROC_INIT("timer",
		              "timeout",
		              2,
		              2,
		              szl_timer_proc_timer,
		              NULL)
	}
};

int szl_init_timer(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "timer",
	                   timer_exports,
	                   sizeof(timer_exports) / sizeof(timer_exports[0]));
}
