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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <poll.h>

#include "szl.h"

#define SZL_DEFAULT_SERVER_SOCKET_BACKLOG 5

static
int szl_socket_connect(struct szl_interp *interp, void *priv)
{
	struct pollfd pfd;
	int err, fd = (int)(intptr_t)priv;
	socklen_t len = sizeof(err);

	pfd.fd = fd;
	pfd.events = POLLOUT | POLLERR | POLLHUP;
	if (poll(&pfd, 1, -1) < 0) {
		szl_set_last_str(interp, strerror(errno), -1);
		return 0;
	}

	if (pfd.revents == POLLOUT)
		return 1;

	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
		szl_set_last_str(interp, strerror(errno), -1);
	else
		szl_set_last_str(interp, strerror(err), -1);

	return 0;
}

static
ssize_t szl_socket_read(struct szl_interp *interp,
                        void *priv,
                        unsigned char *buf,
                        const size_t len,
                        int *more)
{
	ssize_t out;

	out = recv((int)(intptr_t)priv, buf, len, 0);
	if (out < 0) {
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			return 0;

		szl_set_last_str(interp, strerror(errno), -1);
	} else if (out == 0)
		*more = 0;

	return out;
}

static
ssize_t szl_socket_write(struct szl_interp *interp,
                         void *priv,
                         const unsigned char *buf,
                         const size_t len)
{
	ssize_t out;

	out = send((int)(intptr_t)priv, buf, len, 0);
	if (out < 0) {
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			return 0;

		szl_set_last_str(interp, strerror(errno), -1);
	}

	return out;
}

static
enum szl_res szl_socket_unblock(void *priv)
{
	int fl, fd = (int)(intptr_t)priv;

	fl = fcntl(fd, F_GETFL);
	if ((fl < 0) || (fcntl(fd, F_SETFL, fl | O_NONBLOCK) < 0))
		return SZL_ERR;

	return SZL_OK;
}

static
void szl_socket_close(void *priv)
{
	close((int)(intptr_t)priv);
}

static
struct szl_stream *szl_socket_new(const int fd,
                                  const struct szl_stream_ops *ops,
                                  const unsigned int flags)
{
	struct szl_stream *strm;

	strm = (struct szl_stream *)malloc(sizeof(struct szl_stream));
	if (!strm)
		return NULL;

	strm->priv = (void *)(intptr_t)fd;
	strm->ops = ops;
	strm->flags = flags;
	strm->buf = NULL;

	return strm;
}

static const struct szl_stream_ops szl_stream_client_ops;

static
int szl_socket_accept(struct szl_interp *interp,
                      void *priv,
                      struct szl_stream **strm)
{
	int fd;

	fd = accept((int)(intptr_t)priv, NULL, NULL);
	if (fd < 0) {
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
			*strm = NULL;
			return 1;
		}
	}
	else {
		*strm = szl_socket_new(fd, &szl_stream_client_ops, SZL_STREAM_BLOCKING);
		if (*strm)
			return 1;

		close(fd);
	}

	szl_set_last_str(interp, strerror(errno), -1);
	return 0;
}

static
szl_int szl_socket_handle(void *priv)
{
	return (szl_int)(intptr_t)priv;
}

static
int szl_socket_setopt(struct szl_interp *interp,
                      void *priv,
                      struct szl_obj *opt,
                      struct szl_obj *val)
{
	char *s;
	int b;

	if (!szl_as_str(interp, opt, &s, NULL))
		return 0;

	if (strcmp(s, "cork") == 0) {
		if (!szl_as_bool(val, &b))
			return 0;

		if (setsockopt((int)(intptr_t)priv,
		               SOL_TCP,
		               TCP_CORK,
		               &b,
		               sizeof(b)) == 0)
			return 1;

		szl_set_last_str(interp, strerror(errno), -1);
		return 0;
	}

	szl_set_last_fmt(interp, "bad opt: %s", s);
	return 0;
}

static
const struct szl_stream_ops szl_stream_server_ops = {
	.close = szl_socket_close,
	.accept = szl_socket_accept,
	.handle = szl_socket_handle,
	.unblock = szl_socket_unblock
};

static
const struct szl_stream_ops szl_stream_client_ops = {
	.connect = szl_socket_connect,
	.read = szl_socket_read,
	.write = szl_socket_write,
	.close = szl_socket_close,
	.handle = szl_socket_handle,
	.unblock = szl_socket_unblock,
	.setopt = szl_socket_setopt
};

static
const struct szl_stream_ops szl_dgram_server_ops = {
	.read = szl_socket_read,
	.close = szl_socket_close,
	.handle = szl_socket_handle,
	.unblock = szl_socket_unblock
};

static
const struct szl_stream_ops szl_dgram_client_ops = {
	.write = szl_socket_write,
	.close = szl_socket_close,
	.handle = szl_socket_handle,
	.unblock = szl_socket_unblock
};

static
struct szl_stream *szl_socket_new_client(struct szl_interp *interp,
                                         const char *host,
                                         const char *service,
                                         const int type,
                                         const int sflags,
                                         const struct szl_stream_ops *ops,
                                         const unsigned int flags)
{
	struct addrinfo hints, *res;
	struct szl_stream *strm;
	int fd, fl;

	hints.ai_flags = 0;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = type;
	hints.ai_protocol = 0;
	hints.ai_addrlen = 0;
	hints.ai_addr = NULL;
	hints.ai_canonname = NULL;
	hints.ai_next = NULL;

	if (getaddrinfo(host, service, &hints, &res) != 0)
		return NULL;

	fd = socket(res->ai_family, res->ai_socktype | sflags, res->ai_protocol);
	if (fd < 0) {
		freeaddrinfo(res);
		szl_set_last_str(interp, strerror(errno), -1);
		return NULL;
	}

	if (sflags & SOCK_NONBLOCK) {
		fl = fcntl(fd, F_GETFL);
		if (fl < 0) {
			freeaddrinfo(res);
			szl_set_last_str(interp, strerror(errno), -1);
			return NULL;
		}

		if (((connect(fd, res->ai_addr, res->ai_addrlen) < 0) &&
		     (errno != EINPROGRESS)) ||
		    (fcntl(fd, F_SETFL, fl & ~O_NONBLOCK) < 0)) {
			close(fd);
			freeaddrinfo(res);
			szl_set_last_str(interp, strerror(errno), -1);
			return NULL;
		}
	}
	else if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
		close(fd);
		freeaddrinfo(res);
		szl_set_last_str(interp, strerror(errno), -1);
		return NULL;
	}

	strm = szl_socket_new(fd, ops, flags);
	if (!strm)
		close(fd);

	freeaddrinfo(res);
	return strm;
}

static
int szl_socket_new_server(struct szl_interp *interp,
                          const char *host,
                          const char *service,
                          const int type)
{
	struct addrinfo hints, *res;
	int fd, one = 1;

	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = type;
	hints.ai_protocol = 0;
	hints.ai_addrlen = 0;
	hints.ai_addr = NULL;
	hints.ai_canonname = NULL;
	hints.ai_next = NULL;

	if (getaddrinfo(host, service, &hints, &res) != 0)
		return -1;

	fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (fd < 0) {
		freeaddrinfo(res);
		szl_set_last_str(interp, strerror(errno), -1);
		return -1;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
		close(fd);
		freeaddrinfo(res);
		return -1;
	}

	if (bind(fd, res->ai_addr, res->ai_addrlen) < 0) {
		close(fd);
		freeaddrinfo(res);
		szl_set_last_str(interp, strerror(errno), -1);
		return -1;
	}

	freeaddrinfo(res);
	return fd;
}

static
enum szl_res szl_socket_proc(struct szl_interp *interp,
                             const unsigned int objc,
                             struct szl_obj **objv,
                             const char *type,
                             const int listening,
                             struct szl_stream *(*cb)(struct szl_interp *,
                                                      const char *,
                                                      const char *,
                                                      const int))
{
	struct szl_obj *obj;
	char *host, *service;
	struct szl_stream *strm;
	szl_int backlog = SZL_DEFAULT_SERVER_SOCKET_BACKLOG;
	size_t len;

	if (!szl_as_str(interp, objv[2], &service, &len) ||
	    !len ||
	    !szl_as_str(interp, objv[1], &host, &len))
		return SZL_ERR;

	if (listening && (objc == 4) && (!szl_as_int(interp, objv[3], &backlog)))
		return SZL_ERR;

	if (!len)
		host = NULL;

	strm = cb(interp, host, service, backlog);
	if (!strm)
		return SZL_ERR;

	obj = szl_new_stream(interp, strm, type);
	if (!obj) {
		szl_stream_free(strm);
		return SZL_ERR;
	}

	return szl_set_last(interp, obj);
}

static
struct szl_stream *szl_socket_new_stream_client(struct szl_interp *interp,
                                                const char *host,
                                                const char *service,
                                                const int backlog)
{
	return szl_socket_new_client(interp,
	                             host,
	                             service,
	                             SOCK_STREAM,
	                             SOCK_NONBLOCK,
	                             &szl_stream_client_ops,
	                             SZL_STREAM_BLOCKING | SZL_STREAM_CONNECTING);
}

static
enum szl_res szl_socket_proc_stream_client(struct szl_interp *interp,
                                           const unsigned int objc,
                                           struct szl_obj **objv)
{
	return szl_socket_proc(interp,
	                       objc,
	                       objv,
	                       "stream.client",
	                       0,
	                       szl_socket_new_stream_client);
}

static
struct szl_stream *szl_socket_new_stream_server(struct szl_interp *interp,
                                                const char *host,
                                                const char *service,
                                                const int backlog)
{
	struct szl_stream *strm;
	int fd;

	fd = szl_socket_new_server(interp, host, service, SOCK_STREAM);
	if (fd < 0)
		return NULL;

	if (listen(fd, backlog) < 0) {
		close(fd);
		szl_set_last_str(interp, strerror(errno), -1);
		return NULL;
	}

	strm = szl_socket_new(fd, &szl_stream_server_ops, SZL_STREAM_BLOCKING);
	if (!strm)
		close(fd);

	return strm;
}

static
enum szl_res szl_socket_proc_stream_server(struct szl_interp *interp,
                                           const unsigned int objc,
                                           struct szl_obj **objv)
{
	return szl_socket_proc(interp,
	                       objc,
	                       objv,
	                       "stream.server",
	                       1,
	                       szl_socket_new_stream_server);
}

static
struct szl_stream *szl_socket_new_dgram_client(struct szl_interp *interp,
                                               const char *host,
                                               const char *service,
                                               const int backlog)
{
	return szl_socket_new_client(interp,
	                             host,
	                             service,
	                             SOCK_DGRAM,
	                             0,
	                             &szl_dgram_client_ops,
	                             SZL_STREAM_BLOCKING);
}

static
enum szl_res szl_socket_proc_dgram_client(struct szl_interp *interp,
                                          const unsigned int objc,
                                          struct szl_obj **objv)
{
	return szl_socket_proc(interp,
	                       objc,
	                       objv,
	                       "dgram.client",
	                       0,
	                       szl_socket_new_dgram_client);
}

static
struct szl_stream *szl_socket_new_dgram_server(struct szl_interp *interp,
                                               const char *host,
                                               const char *service,
                                               const int backlog)
{
	struct szl_stream *strm;
	int fd;

	fd = szl_socket_new_server(interp, host, service, SOCK_DGRAM);
	if (fd < 0)
		return NULL;

	strm = szl_socket_new(fd, &szl_dgram_server_ops, SZL_STREAM_BLOCKING);
	if (!strm)
		close(fd);

	return strm;
}

static
enum szl_res szl_socket_proc_dgram_server(struct szl_interp *interp,
                                          const unsigned int objc,
                                          struct szl_obj **objv)
{
	return szl_socket_proc(interp,
	                       objc,
	                       objv,
	                       "dgram.server",
	                       0,
	                       szl_socket_new_dgram_server);
}

static
const struct szl_ext_export socket_exports[] = {
	{
		SZL_PROC_INIT("stream.client",
		              "host service",
		              3,
		              3,
		              szl_socket_proc_stream_client,
		              NULL)
	},
	{
		SZL_PROC_INIT("dgram.client",
		              "host service",
		              3,
		              3,
		              szl_socket_proc_dgram_client,
		              NULL)
	},
	{
		SZL_PROC_INIT("stream.server",
		              "host service ?backlog?",
		              3,
		              4,
		              szl_socket_proc_stream_server,
		              NULL)
	},
	{
		SZL_PROC_INIT("dgram.server",
		              "host service",
		              3,
		              3,
		              szl_socket_proc_dgram_server,
		              NULL)
	}
};

int szl_init_socket(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "socket",
	                   socket_exports,
	                   sizeof(socket_exports) / sizeof(socket_exports[0]));
}
