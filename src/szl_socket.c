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

#include "szl.h"

#define SZL_DEFAULT_SERVER_SOCKET_BACKLOG 5

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

		szl_set_result_str(interp, strerror(errno), -1);
		return -1;
	} else if (out == 0)
		*more = 0;

	return out;
}

static
ssize_t szl_socket_write(struct szl_interp *interp,
                         void *priv,
                         const unsigned char *buf,
                         size_t len)
{
	ssize_t out;

	out = send((int)(intptr_t)priv, buf, len, 0);
	if (out < 0) {
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			return 0;

		szl_set_result_str(interp, strerror(errno), -1);
		return -1;
	}

	return (ssize_t)out;
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
                                  const struct szl_stream_ops *ops)
{
	struct szl_stream *strm;

	strm = (struct szl_stream *)malloc(sizeof(struct szl_stream));
	if (!strm)
		return NULL;

	strm->priv = (void *)(intptr_t)fd;
	strm->ops = ops;
	strm->keep = 0;
	strm->closed = 0;
	strm->buf = NULL;
	strm->blocking = 1;

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
		*strm = szl_socket_new(fd, &szl_stream_client_ops);
		if (*strm)
			return 1;

		close(fd);
	}

	szl_set_result_str(interp, strerror(errno), -1);
	return 0;
}

static
szl_int szl_socket_handle(void *priv)
{
	return (szl_int)(intptr_t)priv;
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
	.read = szl_socket_read,
	.write = szl_socket_write,
	.close = szl_socket_close,
	.handle = szl_socket_handle,
	.unblock = szl_socket_unblock
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
                                         const struct szl_stream_ops *ops)
{
	struct addrinfo hints, *res;
	struct szl_stream *strm;
	int fd;

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

	fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (fd < 0) {
		freeaddrinfo(res);
		szl_set_result_str(interp, strerror(errno), -1);
		return NULL;
	}

	if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
		close(fd);
		freeaddrinfo(res);
		szl_set_result_str(interp, strerror(errno), -1);
		return NULL;
	}

	strm = szl_socket_new(fd, ops);
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
		szl_set_result_str(interp, strerror(errno), -1);
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
		szl_set_result_str(interp, strerror(errno), -1);
		return -1;
	}

	freeaddrinfo(res);
	return fd;
}

static
enum szl_res szl_socket_proc(struct szl_interp *interp,
                             const int objc,
                             struct szl_obj **objv,
                             const char *type,
                             const int listening,
                             struct szl_stream *(*cb)(struct szl_interp *,
                                                      const char *,
                                                      const char *,
                                                      const int))
{
	struct szl_obj *obj;
	const char *host, *service;
	struct szl_stream *strm;
	szl_int backlog = SZL_DEFAULT_SERVER_SOCKET_BACKLOG;
	size_t len;

	service = szl_obj_str(interp, objv[2], &len);
	if (!service || !len)
		return SZL_ERR;

	host = szl_obj_str(interp, objv[1], &len);
	if (!host)
		return SZL_ERR;

	if (listening && (objc == 4) && (!szl_obj_int(interp, objv[3], &backlog)))
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

	return szl_set_result(interp, obj);
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
	                             &szl_stream_client_ops);
}

static
enum szl_res szl_socket_proc_stream_client(struct szl_interp *interp,
                                           const int objc,
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
		szl_set_result_str(interp, strerror(errno), -1);
		return NULL;
	}

	strm = szl_socket_new(fd, &szl_stream_server_ops);
	if (!strm)
		close(fd);

	return strm;
}

static
enum szl_res szl_socket_proc_stream_server(struct szl_interp *interp,
                                           const int objc,
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
	                             &szl_dgram_client_ops);
}

static
enum szl_res szl_socket_proc_dgram_client(struct szl_interp *interp,
                                          const int objc,
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

	strm = szl_socket_new(fd, &szl_dgram_server_ops);
	if (!strm)
		close(fd);

	return strm;
}

static
enum szl_res szl_socket_proc_dgram_server(struct szl_interp *interp,
                                          const int objc,
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
enum szl_res szl_socket_proc_issocket(struct szl_interp *interp,
                                      const int objc,
                                      struct szl_obj **objv)
{
	struct stat stbuf;
	szl_int fd;

	if ((!szl_obj_int(interp, objv[1], &fd)) || (fd < 0) || (fd > INT_MAX))
		return SZL_ERR;

	if (fstat((int)fd, &stbuf) < 0)
		return SZL_ERR;

	return szl_set_result_bool(interp, S_ISSOCK(stbuf.st_mode) ? 1 : 0);
}

static
enum szl_res szl_socket_proc_socket_fdopen(struct szl_interp *interp,
                                           const int objc,
                                           struct szl_obj **objv)
{
	struct szl_obj *obj;
	struct szl_stream *strm;
	szl_int fd;
	socklen_t type, len;

	if ((!szl_obj_int(interp, objv[1], &fd)) || (fd < 0) || (fd > INT_MAX))
		return SZL_ERR;

	len = sizeof(type);
	if ((getsockopt((int)fd, SOL_SOCKET, SO_TYPE, &type, &len) < 0) ||
		(len != sizeof(type)))
		return SZL_ERR;

	if (type == SOCK_STREAM)
		strm = szl_socket_new((int)fd, &szl_stream_client_ops);
	else if (type == SOCK_DGRAM)
		strm = szl_socket_new((int)fd, &szl_dgram_client_ops);
	else
		return SZL_ERR;

	if (!strm)
		return SZL_ERR;

	obj = szl_new_stream(
	                  interp,
	                  strm,
	                  (type == SOCK_STREAM) ? "stream.client" : "dgram.client");
	if (!obj) {
		szl_stream_free(strm);
		return SZL_ERR;
	}

	return szl_set_result(interp, obj);
}

int szl_init_socket(struct szl_interp *interp)
{
	return ((szl_new_proc(interp,
	                      "stream.client",
	                      3,
	                      3,
	                      "stream.client host service",
	                      szl_socket_proc_stream_client,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "dgram.client",
	                      3,
	                      3,
	                      "dgram.client host service",
	                      szl_socket_proc_dgram_client,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "stream.server",
	                      3,
	                      4,
	                      "stream.server host service ?backlog?",
	                      szl_socket_proc_stream_server,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "dgram.server",
	                      3,
	                      3,
	                      "dgram.server host service",
	                      szl_socket_proc_dgram_server,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "_socket.issocket",
	                      2,
	                      2,
	                      "_socket.issocket handles",
	                      szl_socket_proc_issocket,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "_socket.fdopen",
	                      2,
	                      2,
	                      "_socket.fdopen handle",
	                      szl_socket_proc_socket_fdopen,
	                      NULL,
	                      NULL)));
}
