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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>

#include "szl.h"

#define _szl_socket_to_int(x) x
#define _szl_int_to_socket(x) (int)x

#define SZL_SERVER_SOCKET_BACKLOG 5

static
ssize_t szl_socket_read(void *priv, unsigned char *buf, const size_t len)
{
	size_t total = 0;
	ssize_t chunk;

	do {
		chunk = recv((int)(intptr_t)priv, buf + total, len - total, 0);
		if (chunk < 0)
			return -1;
		if (chunk == 0)
			break;
		total += (ssize_t)chunk;
	} while (total < len);

	return (ssize_t)total;
}

static
ssize_t szl_socket_write(void *priv, const unsigned char *buf, const size_t len)
{
	size_t total = 0;
	ssize_t chunk;

	do {
		chunk = send((int)(intptr_t)priv, buf + total, len - total, 0);
		if (chunk < 0)
			return -1;
		total += (ssize_t)chunk;
	} while (total < len);

	return (ssize_t)total;
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

	return strm;
}

static const struct szl_stream_ops szl_stream_client_ops;

static
struct szl_stream *szl_socket_accept(void *priv)
{
	struct szl_stream *strm;
	int fd;

	fd = accept((int)(intptr_t)priv, NULL, NULL);
	if (fd < 0)
		return NULL;

	strm = szl_socket_new(fd, &szl_stream_client_ops);
	if (!strm)
		close(fd);

	return strm;
}

static
szl_int szl_socket_fileno(void *priv)
{
	return (szl_int)(intptr_t)priv;
}

static const struct szl_stream_ops szl_stream_server_ops = {
	NULL,
	NULL,
	NULL,
	szl_socket_close,
	szl_socket_accept,
	szl_socket_fileno
};

static const struct szl_stream_ops szl_stream_client_ops = {
	szl_socket_read,
	szl_socket_write,
	NULL,
	szl_socket_close,
	NULL,
	szl_socket_fileno
};

static const struct szl_stream_ops szl_dgram_server_ops = {
	szl_socket_read,
	NULL,
	NULL,
	szl_socket_close,
	NULL,
	szl_socket_fileno
};

static const struct szl_stream_ops szl_dgram_client_ops = {
	NULL,
	szl_socket_write,
	NULL,
	szl_socket_close,
	NULL,
	szl_socket_fileno
};

static
struct szl_stream *szl_socket_new_client(const char *host,
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
		return NULL;
	}

	if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
		close(fd);
		freeaddrinfo(res);
		return NULL;
	}

	strm = szl_socket_new(fd, ops);
	if (!strm)
		close(fd);

	freeaddrinfo(res);
	return strm;
}

static
struct szl_stream *szl_socket_new_stream_client(const char *host,
                                                const char *service)
{
	return szl_socket_new_client(host,
	                             service,
	                             SOCK_STREAM,
	                             &szl_stream_client_ops);

}

static
struct szl_stream *szl_socket_new_dgram_client(const char *host,
                                               const char *service)
{
	return szl_socket_new_client(host,
	                             service,
	                             SOCK_DGRAM,
	                             &szl_dgram_client_ops);

}

static
int szl_socket_new_server(const char *host, const char *service, const int type)
{
	struct addrinfo hints, *res;
	int fd;

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
		return -1;
	}

	freeaddrinfo(res);

	if (bind(fd, res->ai_addr, res->ai_addrlen) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

static
struct szl_stream *szl_socket_new_stream_server(const char *host,
                                                const char *service)
{
	struct szl_stream *strm;
	int fd;

	fd = szl_socket_new_server(host, service, SOCK_STREAM);
	if (fd < 0)
		return NULL;

	if (listen(fd, SZL_SERVER_SOCKET_BACKLOG) < 0) {
		close(fd);
		return NULL;
	}

	strm = szl_socket_new(fd, &szl_stream_server_ops);
	if (!strm)
		close(fd);

	return strm;
}

static
struct szl_stream *szl_socket_new_dgram_server(const char *host,
                                               const char *service)
{
	struct szl_stream *strm;
	int fd;

	fd = szl_socket_new_server(host, service, SOCK_DGRAM);
	if (fd < 0)
		return NULL;

	strm = szl_socket_new(fd, &szl_dgram_server_ops);
	if (!strm)
		close(fd);

	return strm;
}

static
 enum szl_res szl_socket_proc_socket(struct szl_interp *interp,
                                     const int objc,
                                     struct szl_obj **objv)
{
	struct szl_obj *obj;
	const char *type, *host, *service;
	struct szl_stream *strm;
	size_t len;

	type = szl_obj_str(objv[1], &len);
	if (!type || !len)
		return SZL_ERR;

	service = szl_obj_str(objv[3], &len);
	if (!service || !len)
		return SZL_ERR;

	host = szl_obj_str(objv[2], &len);
	if (!host)
		return SZL_ERR;

	if (!len)
		host = NULL;

	if (strcmp("stream.client", type) == 0)
		strm = szl_socket_new_stream_client(host, service);
	else if (strcmp("stream.server", type) == 0)
		strm = szl_socket_new_stream_server(host, service);
	else if (strcmp("dgram.client", type) == 0)
		strm = szl_socket_new_dgram_client(host, service);
	else if (strcmp("dgram.server", type) == 0)
		strm = szl_socket_new_dgram_server(host, service);
	else
		return SZL_ERR;

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
enum szl_res szl_socket_proc_select(struct szl_interp *interp,
                                    const int objc,
                                    struct szl_obj **objv)
{
	fd_set fds[3];
	struct szl_obj *fdl[3], *obj;
	const char **toks;
	const char *l;
	szl_int *fdi;
	size_t len, n;
	int nfds, i, j, ready;

	toks = szl_obj_list(interp, objv[1], &n);
	if (!toks)
		return SZL_ERR;

	if (!n)
		return szl_usage(interp, objv[0]);

	for (i = 0; i < 3; ++i) {
		fdl[i] = szl_new_empty();
		if (!fdl[i]) {
			for (j = 0; j < i; ++j)
				szl_obj_unref(fdl[j]);
			return SZL_ERR;
		}
	}

	obj = szl_new_empty();
	if (!obj) {
		for (j = 0; j < 3; ++j)
			szl_obj_unref(fdl[j]);
		return SZL_ERR;
	}

	fdi = (szl_int *)malloc(sizeof(szl_int) * n);
	if (!fdi) {
		szl_obj_unref(obj);
		for (j = 0; j < 3; ++j)
			szl_obj_unref(fdl[j]);
		return SZL_ERR;
	}

	FD_ZERO(&fds[0]);
	for (i = 0; i < n; ++i) {
		if ((sscanf(toks[i], SZL_INT_FMT, &fdi[i]) != 1) ||
		    (fdi[i] < 0) ||
		    (fdi[i] > INT_MAX)) {
			szl_obj_unref(obj);
			for (j = 0; j < 3; ++j)
				szl_obj_unref(fdl[j]);
			free(fdi);
			return SZL_ERR;
		}

		FD_SET(_szl_int_to_socket(fdi[i]), &fds[0]);

		if ((i == 0) || ((int)fdi[i] > nfds))
			nfds = (int)fdi[i];
	}

	memcpy(&fds[1], &fds[0], sizeof(fd_set));
	memcpy(&fds[2], &fds[0], sizeof(fd_set));

	ready = select(nfds + 1, &fds[0], &fds[1], &fds[2], NULL);
	if (ready < 0) {
		szl_obj_unref(obj);
		for (j = 0; j < 3; ++j)
			szl_obj_unref(fdl[j]);
		free(fdi);
		return SZL_ERR;
	}

	if (ready) {
		for (i = 0; i < n; ++i) {
			if (((FD_ISSET(_szl_int_to_socket(fdi[i]), &fds[0])) &&
				 (!szl_lappend_int(fdl[0], _szl_socket_to_int(fdi[i])))) ||
				((FD_ISSET(_szl_int_to_socket(fdi[i]), &fds[1])) &&
				 (!szl_lappend_int(fdl[1], _szl_socket_to_int(fdi[i])))) ||
				((FD_ISSET(_szl_int_to_socket(fdi[i]), &fds[2])) &&
				 (!szl_lappend_int(fdl[2], _szl_socket_to_int(fdi[i]))))) {
				szl_obj_unref(obj);
				for (j = 0; j < 3; ++j)
					szl_obj_unref(fdl[j]);
				free(fdi);
				return SZL_ERR;
			}
		}
	}

	free(fdi);

	for (i = 0; i < 3; ++i) {
		l = szl_obj_str(fdl[i], &len);
		if (!l) {
			szl_obj_unref(obj);
			for (j = 0; j < 3; ++j)
				szl_obj_unref(fdl[j]);
			free(fdi);
			return SZL_ERR;
		}

		if (!szl_lappend_str(obj, l)) {
			szl_obj_unref(obj);
			for (j = 0; j < 3; ++j)
				szl_obj_unref(fdl[j]);
			free(fdi);
			return SZL_ERR;
		}
	}

	for (j = 0; j < 3; ++j)
		szl_obj_unref(fdl[j]);

	return szl_set_result(interp, obj);
}

static
enum szl_res szl_socket_proc_issocket(struct szl_interp *interp,
                                      const int objc,
                                      struct szl_obj **objv)
{
	struct stat stbuf;
	szl_int fd;

	if ((!szl_obj_int(objv[1], &fd)) || (fd < 0) || (fd > INT_MAX))
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

	if ((!szl_obj_int(objv[1], &fd)) || (fd < 0) || (fd > INT_MAX))
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
	                      "socket",
	                      4,
	                      4,
	                      "socket type host service",
	                      szl_socket_proc_socket,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "select",
	                      2,
	                      2,
	                      "select handles",
	                      szl_socket_proc_select,
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
