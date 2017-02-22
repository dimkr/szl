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
#include <string.h>
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>

#include "szl.h"

static
enum szl_res szl_poll_poll_proc(struct szl_interp *interp,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	struct epoll_event ev = {0}, *evs;
	char *op, *evtype;
	struct szl_obj *list, *r, *w, *e;
	szl_int fd, n, i, j;
	unsigned int k;
	int out, efd = (int)(intptr_t)objv[0]->priv;

	if (!szl_as_str(interp, objv[1], &op, NULL))
		return SZL_ERR;

	if ((objc >= 4) && (strcmp("add", op) == 0)) {
		if (!szl_as_int(interp, objv[2], &fd))
			return SZL_ERR;

		if ((fd < 0) || (fd > INT_MAX)) {
			szl_set_last_str(interp, "bad fd", -1);
			return SZL_ERR;
		}

		ev.events = 0;
		for (k = 3; k < objc; ++k) {
			if (!szl_as_str(interp, objv[k], &evtype, NULL))
				return SZL_ERR;

			if ((strcmp("in", evtype) == 0) && !(ev.events & EPOLLIN))
				ev.events |= EPOLLIN;
			else if ((strcmp("out", evtype) == 0) && !(ev.events & EPOLLOUT))
				ev.events |= EPOLLOUT;
			else
				return szl_set_last_help(interp, objv[0]);
		}

		ev.data.fd = (int)fd;
		if ((epoll_ctl(efd, EPOLL_CTL_ADD, (int)fd, &ev) < 0) &&
			(errno != EEXIST))
			return SZL_ERR;

		return SZL_OK;
	}
	else if (objc == 3) {
		if (strcmp("remove", op) == 0) {
			if (!szl_as_int(interp, objv[2], &fd))
				return SZL_ERR;

			if ((fd < 0) || (fd > INT_MAX)) {
				szl_set_last_str(interp, "bad fd", -1);
				return SZL_ERR;
			}

			if ((epoll_ctl(efd, EPOLL_CTL_DEL, (int)fd, NULL) < 0) &&
			    (errno != ENOENT))
				return SZL_ERR;

			return SZL_OK;
		} else if (strcmp("wait", op) == 0) {
			if (!szl_as_int(interp, objv[2], &n))
				return SZL_ERR;

			if ((n < 0) || (n > INT_MAX)) {
				szl_set_last_str(interp, "bad event list size", -1);
				return SZL_ERR;
			}

			list = szl_new_empty();
			if (!list)
				return SZL_ERR;

			r = szl_new_list(NULL, 0);
			if (!r) {
				szl_free(list);
				return SZL_ERR;
			}

			w = szl_new_list(NULL, 0);
			if (!w) {
				szl_free(r);
				szl_free(list);
				return SZL_ERR;
			}

			e = szl_new_list(NULL, 0);
			if (!e) {
				szl_free(w);
				szl_free(r);
				szl_free(list);
				return SZL_ERR;
			}

			if (!szl_list_append(interp, list, r) ||
			    !szl_list_append(interp, list, w) ||
			    !szl_list_append(interp, list, e)) {
				szl_unref(e);
				szl_unref(w);
				szl_free(r);
				szl_free(list);
				return SZL_ERR;
			}

			szl_unref(e);
			szl_unref(w);
			szl_unref(r);

			evs = (struct epoll_event *)malloc(sizeof(struct epoll_event) * n);
			if (!evs) {
				szl_free(list);
				return SZL_ERR;
			}

			out = epoll_wait(efd, evs, (int)n, -1);
			if (out < 0) {
				free(evs);
				szl_free(list);
				return SZL_ERR;
			}

			j = 0;
			for (i = 0; (j < out) && (i < n); ++i) {
				if (!evs[i].events)
					continue;

				if ((evs[i].events & EPOLLIN) &&
				    !szl_list_append_int(interp, r, (szl_int)evs[i].data.fd))
					goto err;

				if ((evs[i].events & EPOLLOUT) &&
				    !szl_list_append_int(interp, w, (szl_int)evs[i].data.fd))
					goto err;

				if ((evs[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) &&
				    !szl_list_append_int(interp, e, (szl_int)evs[i].data.fd))
					goto err;

				++j;
				continue;

err:
				free(evs);
				szl_free(list);
				return SZL_ERR;
			}

			free(evs);
			return szl_set_last(interp, list);
		}
	}

	return szl_set_last_help(interp, objv[0]);
}

static
void szl_poll_poll_del(void *priv)
{
	close((int)(intptr_t)priv);
}

static
enum szl_res szl_poll_proc_create(struct szl_interp *interp,
                                  const unsigned int objc,
                                  struct szl_obj **objv)
{
	struct szl_obj *name, *proc;
	int fd;

	fd = epoll_create1(0);
	if (fd < 0)
		return SZL_ERR;

	name = szl_new_str_fmt("poll:%d", fd);
	if (!name) {
		close(fd);
		return SZL_ERR;
	}

	proc = szl_new_proc(interp,
	                    name,
	                    3,
	                    5,
	                    "poll add|remove|wait handle|lim|event...",
	                    szl_poll_poll_proc,
	                    szl_poll_poll_del,
	                    (void *)(intptr_t)fd);
	if (!proc) {
		szl_free(name);
		close(fd);
		return SZL_ERR;
	}

	szl_unref(name);
	return szl_set_last(interp, proc);
}

static
const struct szl_ext_export poll_exports[] = {
	{
		SZL_PROC_INIT("poll.create",
		              NULL,
		              1,
		              1,
		              szl_poll_proc_create,
		              NULL)
	}
};

int szl_init_poll(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "poll",
	                   poll_exports,
	                   sizeof(poll_exports) / sizeof(poll_exports[0]));
}
