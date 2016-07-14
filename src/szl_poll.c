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

#include "szl.h"

static
enum szl_res szl_poll_poll_proc(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv)
{
	struct epoll_event ev = {0}, *evs;
	const char *op;
	struct szl_obj *list, *r, *w, *e;
	szl_int fd, n, i, j;
	int out, efd = (int)(intptr_t)objv[0]->priv;

	op = szl_obj_str(interp, objv[1], NULL);
	if (!op)
		return SZL_ERR;

	if (objc == 3) {
		if (strcmp("add", op) == 0) {
			if (!szl_obj_int(interp, objv[2], &fd))
				return SZL_ERR;

			if ((fd < 0) || (fd > INT_MAX)) {
				szl_set_result_str(interp, "bad fd", -1);
				return SZL_ERR;
			}

			ev.events = EPOLLIN | EPOLLOUT;
			ev.data.fd = (int)fd;
			if (epoll_ctl(efd, EPOLL_CTL_ADD, (int)fd, &ev) < 0)
				return SZL_ERR;

			return SZL_OK;
		} else if (strcmp("remove", op) == 0) {
			if (!szl_obj_int(interp, objv[2], &fd))
				return SZL_ERR;

			if ((fd < 0) || (fd > INT_MAX)) {
				szl_set_result_str(interp, "bad fd", -1);
				return SZL_ERR;
			}

			if (epoll_ctl(efd, EPOLL_CTL_DEL, (int)fd, NULL) < 0)
				return SZL_ERR;

			return SZL_OK;
		} else if (strcmp("wait", op) == 0) {
			if (!szl_obj_int(interp, objv[2], &n))
				return SZL_ERR;

			if ((n < 0) || (n > INT_MAX)) {
				szl_set_result_str(interp, "bad event list size", -1);
				return SZL_ERR;
			}

			list = szl_new_empty();
			if (!list)
				return SZL_ERR;

			r = szl_new_empty();
			if (!r) {
				szl_obj_unref(list);
				return SZL_ERR;
			}

			w = szl_new_empty();
			if (!w) {
				szl_obj_unref(r);
				szl_obj_unref(list);
				return SZL_ERR;
			}

			e = szl_new_empty();
			if (!e) {
				szl_obj_unref(w);
				szl_obj_unref(r);
				szl_obj_unref(list);
				return SZL_ERR;
			}

			if (!szl_lappend(interp, list, r) ||
			    !szl_lappend(interp, list, w) ||
			    !szl_lappend(interp, list, e)) {
				szl_obj_unref(e);
				szl_obj_unref(w);
				szl_obj_unref(r);
				szl_obj_unref(list);
				return SZL_ERR;
			}

			szl_obj_unref(e);
			szl_obj_unref(w);
			szl_obj_unref(r);

			evs = (struct epoll_event *)malloc(sizeof(struct epoll_event) * n);
			if (!evs) {
				szl_obj_unref(list);
				return SZL_ERR;
			}

			out = epoll_wait(efd, evs, (int)n, -1);
			if (out < 0) {
				free(evs);
				szl_obj_unref(list);
				return SZL_ERR;
			}

			j = 0;
			for (i = 0; (j < out) && (i < n); ++i) {
				if (!evs[i].events)
					continue;

				if ((evs[i].events & EPOLLIN) &&
				    !szl_lappend_int(interp, r, (szl_int)evs[i].data.fd))
					goto err;

				if ((evs[i].events & EPOLLOUT) &&
				    !szl_lappend_int(interp, w, (szl_int)evs[i].data.fd))
					goto err;

				if ((evs[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) &&
				    !szl_lappend_int(interp, e, (szl_int)evs[i].data.fd))
					goto err;

				++j;
				continue;

err:
				free(evs);
				szl_obj_unref(list);
				return SZL_ERR;
			}

			free(evs);
			return szl_set_result(interp, list);
		}
	}

	return szl_usage(interp, objv[0]);
}

static
void szl_poll_poll_del(void *priv)
{
	close((int)(intptr_t)priv);
}

static
enum szl_res szl_poll_proc_create(struct szl_interp *interp,
                                  const int objc,
                                  struct szl_obj **objv)
{
	char name[sizeof("poll:"SZL_PASTE(SZL_INT_MIN))];
	struct szl_obj *proc;
	int fd;

	fd = epoll_create1(0);
	if (fd < 0)
		return SZL_ERR;

	szl_new_obj_name(interp, "poll", name, sizeof(name), (void *)(intptr_t)fd);
	proc = szl_new_proc(interp,
	                    name,
	                    3,
	                    3,
	                    "poll add|remove|wait handle|lim",
	                    szl_poll_poll_proc,
	                    szl_poll_poll_del,
	                    (void *)(intptr_t)fd);
	if (!proc) {
		close(fd);
		return SZL_ERR;
	}

	return szl_set_result(interp, szl_obj_ref(proc));
}

int szl_init_poll(struct szl_interp *interp)
{
	return szl_new_proc(interp,
	                    "poll.create",
	                    1,
	                    1,
	                    "poll.create",
	                    szl_poll_proc_create,
	                    NULL,
	                    NULL) ? 1 : 0;
}

