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
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "szl.h"

#define SZL_SERVER_SOCKET_BACKLOG 5

struct szl_stream;

struct szl_stream_ops {
	ssize_t (*read)(void *, unsigned char *, const size_t);
	ssize_t (*write)(void *, const unsigned char *, const size_t);
	enum szl_res (*flush)(void *);
	void (*close)(void *);
	struct szl_stream *(*accept)(void *);
	szl_int (*handle)(void *);
};

struct szl_stream {
	const struct szl_stream_ops *ops;
	int closed;
	void *priv;
};

static const struct szl_stream_ops szl_stream_server_ops;
static const struct szl_stream_ops szl_stream_client_ops;
static const struct szl_stream_ops szl_dgram_server_ops;
static const struct szl_stream_ops szl_dgram_client_ops;

static enum szl_res szl_stream_proc(struct szl_interp *interp,
                                    const int objc,
                                    struct szl_obj **objv,
                                    struct szl_obj **ret);
static void szl_stream_del(void *priv);

static ssize_t szl_file_read(void *priv, unsigned char *buf, const size_t len)
{
	size_t ret;

	ret = fread(buf, 1, len, (FILE *)priv);
	if (!ret) {
		if (ferror((FILE *)priv))
			return -1;
	}

	return (ssize_t)ret;
}

static ssize_t szl_file_write(void *priv,
                              const unsigned char *buf,
                              const size_t len)
{
	size_t total = 0;
	size_t chunk;

	do {
		chunk = fwrite(buf + total, 1, len - total, (FILE *)priv);
		if (chunk == 0) {
			if (ferror((FILE *)priv))
				return -1;
			break;
		}
		total += chunk;
	} while (total < len);

	return total;
}

static enum szl_res szl_file_flush(void *priv)
{
	if (fflush((FILE *)priv) == EOF)
		return SZL_ERR;

	return SZL_OK;
}

static void szl_file_close(void *priv)
{
	fclose((FILE *)priv);
}

static szl_int szl_file_handle(void *priv)
{
	return (szl_int)fileno((FILE *)priv);
}

static const struct szl_stream_ops szl_file_ops = {
	szl_file_read,
	szl_file_write,
	szl_file_flush,
	szl_file_close,
	NULL,
	szl_file_handle
};

static ssize_t szl_socket_read(void *priv, unsigned char *buf, const size_t len)
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

static ssize_t szl_socket_write(void *priv,
                                const unsigned char *buf,
                                const size_t len)
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

static enum szl_res szl_socket_flush(void *priv)
{
	return SZL_OK;
}

static void szl_socket_close(void *priv)
{
	close((int)(intptr_t)priv);
}

static struct szl_stream *szl_socket_new(const int fd,
                                         const struct szl_stream_ops *ops)
{
	struct szl_stream *strm;

	strm = (struct szl_stream *)malloc(sizeof(struct szl_stream));
	if (!strm)
		return NULL;

	strm->priv = (void *)(intptr_t)fd;
	strm->ops = ops;
	strm->closed = 0;

	return strm;
}

static struct szl_stream *szl_socket_accept(void *priv)
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

static szl_int szl_socket_fileno(void *priv)
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
	szl_socket_flush,
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
	szl_socket_flush,
	szl_socket_close,
	NULL,
	szl_socket_fileno
};

static enum szl_res szl_stream_read(struct szl_interp *interp,
                                    struct szl_stream *strm,
                                    struct szl_obj **ret,
                                    const size_t len)
{
	unsigned char *buf;
	ssize_t out;

	*ret = NULL;

	if (!strm->ops->read)
		return SZL_ERR;

	if (strm->closed) {
		*ret = szl_empty(interp);
		return SZL_OK;
	}

	buf = (unsigned char *)malloc(len + 1);
	if (!buf)
		return SZL_ERR;

	out = strm->ops->read(strm->priv, buf, len);
	if (out > 0) {
		buf[len] = '\0';
		*ret = szl_new_str_noalloc((char *)buf, (size_t)out);
		if (!*ret) {
			free(buf);
			return SZL_ERR;
		}
	}
	else {
		free(buf);

		if (out < 0)
			return SZL_ERR;
	}

	return SZL_OK;
}

static enum szl_res szl_stream_write(struct szl_interp *interp,
                                     struct szl_stream *strm,
                                     struct szl_obj **ret,
                                     const unsigned char *buf,
                                     const size_t len)
{
	*ret = NULL;

	if (!strm->ops->write)
		return SZL_ERR;

	if (strm->closed)
		return 0;

	if (strm->ops->write(strm->priv, buf, len) == (ssize_t)len)
		return SZL_OK;

	return SZL_ERR;
}

static enum szl_res szl_stream_flush(struct szl_interp *interp,
                                     struct szl_stream *strm)
{
	if (!strm->ops->flush)
		return SZL_ERR;

	return strm->ops->flush(strm->priv);
}

static void szl_stream_close(struct szl_stream *strm)
{
	if (!strm->closed) {
		strm->ops->close(strm->priv);
		strm->closed = 1;
	}
}

static struct szl_obj *szl_stream_handle(struct szl_interp *interp,
                                         struct szl_stream *strm)
{
	if (strm->closed)
		return szl_empty(interp);

	return szl_new_int(strm->ops->handle(strm->priv));
}

struct szl_obj *szl_register_stream(struct szl_interp *interp,
                                    struct szl_stream *strm,
                                    const char *type)
{
	char name[sizeof("socket.stream.FFFFFFFF")];
	struct szl_obj *proc;

	szl_new_obj_name(interp, type, name, sizeof(name));
	proc = szl_new_proc(interp,
	                    name,
	                    1,
	                    3,
	                    "strm read|write|flush|handle|close|accept ?len?",
	                    szl_stream_proc,
	                    szl_stream_del,
	                    strm);
	if (!proc)
		return NULL;

	return szl_obj_ref(proc);
}

static enum szl_res szl_stream_accept(struct szl_interp *interp,
                                      struct szl_obj **ret,
                                      struct szl_stream *strm)
{
	struct szl_stream *client;

	client = strm->ops->accept(strm->priv);
	if (!client)
		return SZL_ERR;

	*ret = szl_register_stream(interp, client, "stream.client");
	if (!*ret) {
		szl_stream_close(client);
		return SZL_ERR;
	}

	return SZL_OK;
}

static enum szl_res szl_stream_proc(struct szl_interp *interp,
                                    const int objc,
                                    struct szl_obj **objv,
                                    struct szl_obj **ret)
{
	struct szl_stream *strm = (struct szl_stream *)objv[0]->priv;
	const char *op;
	const char *buf;
	szl_int req;
	size_t len;

	op = szl_obj_str(objv[1], NULL);
	if (!op)
		return SZL_ERR;

	if (objc == 3) {
		if (strcmp("read", op) == 0) {
			if (szl_obj_int(objv[2], &req) != SZL_OK)
				return SZL_ERR;

			if (!req)
				return SZL_OK;

			if (req > SSIZE_MAX)
				req = SSIZE_MAX;

			return szl_stream_read(interp, strm, ret, (size_t)req);
		}
		else if (strcmp("write", op) == 0) {
			buf = szl_obj_str(objv[2], &len);
			if (!buf)
				return SZL_ERR;

			if (!len)
				return SZL_OK;

			return szl_stream_write(interp,
			                        strm,
			                        ret,
			                        (const unsigned char *)buf,
			                        len);
		}
	}
	else if (objc == 2) {
		if (strcmp("flush", op) == 0)
			return szl_stream_flush(interp, strm);
		else if (strcmp("close", op) == 0) {
			szl_stream_close(strm);
			return SZL_OK;
		}
		else if (strcmp("accept", op) == 0)
			return szl_stream_accept(interp, ret, strm);
		else if (strcmp("handle", op) == 0) {
			szl_set_result(ret, szl_stream_handle(interp, strm));
			if (!*ret)
				return SZL_ERR;
			return SZL_OK;
		}
	}

	szl_usage(interp, ret, objv[0]);
	return SZL_ERR;
}

static void szl_stream_del(void *priv)
{
	szl_stream_close((struct szl_stream *)priv);
	free(priv);
}

static enum szl_res szl_io_proc_open(struct szl_interp *interp,
                                     const int objc,
                                     struct szl_obj **objv,
                                     struct szl_obj **ret)
{
	struct szl_stream *strm;
	const char *path, *mode;
	size_t len;

	path = szl_obj_str(objv[1], &len);
	if (!path || !len)
		return SZL_ERR;

	mode = szl_obj_str(objv[2], &len);
	if (!mode || !len)
		return SZL_ERR;

	strm = (struct szl_stream *)malloc(sizeof(struct szl_stream));
	if (!strm)
		return SZL_ERR;

	strm->priv = fopen(path, mode);
	if (!strm->priv) {
		free(strm);
		return SZL_ERR;
	}

	strm->ops = &szl_file_ops;
	strm->closed = 0;

	*ret = szl_register_stream(interp, strm, "file");
	if (!*ret) {
		szl_stream_close(strm);
		free(strm);
		return SZL_ERR;
	}

	return SZL_OK;
}

static struct szl_stream *szl_socket_new_client(
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

static struct szl_stream *szl_socket_new_stream_client(const char *host,
                                                       const char *service)
{
	return szl_socket_new_client(host,
	                             service,
	                             SOCK_STREAM,
	                             &szl_stream_client_ops);

}

static struct szl_stream *szl_socket_new_dgram_client(const char *host,
                                                      const char *service)
{
	return szl_socket_new_client(host,
	                             service,
	                             SOCK_DGRAM,
	                             &szl_dgram_client_ops);

}

static int szl_socket_new_server(const char *host,
                                 const char *service,
                                 const int type)
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

static struct szl_stream *szl_socket_new_stream_server(const char *host,
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

static struct szl_stream *szl_socket_new_dgram_server(const char *host,
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

static enum szl_res szl_io_proc_socket(struct szl_interp *interp,
                                       const int objc,
                                       struct szl_obj **objv,
                                       struct szl_obj **ret)
{
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

	*ret = szl_register_stream(interp, strm, type);
	if (!*ret) {
		szl_stream_close(strm);
		free(strm);
	}

	return SZL_OK;
}

static enum szl_res szl_io_proc_puts(struct szl_interp *interp,
                                     const int objc,
                                     struct szl_obj **objv,
                                     struct szl_obj **ret)
{
	const char *s;

	s = szl_obj_str(objv[1], NULL);
	if (!s)
		return SZL_ERR;

	if (puts(s) == EOF)
		return SZL_ERR;

	return SZL_OK;
}

enum szl_res szl_init_io(struct szl_interp *interp)
{
	if ((!szl_new_proc(interp,
	                   "puts",
	                   2,
	                   2,
	                   "puts msg",
	                   szl_io_proc_puts,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "open",
	                   3,
	                   3,
	                   "open path mode",
	                   szl_io_proc_open,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "socket",
	                   4,
	                   4,
	                   "socket type host service",
	                   szl_io_proc_socket,
	                   NULL,
	                   NULL)))
		return SZL_ERR;

	return SZL_OK;
}
