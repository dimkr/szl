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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "szl.h"

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

static enum szl_res szl_new_file(struct szl_interp *interp,
                                 FILE *fp,
                                 struct szl_obj **ret)
{
	struct szl_stream *strm;

	strm = (struct szl_stream *)malloc(sizeof(struct szl_stream));
	if (!strm)
		return SZL_ERR;

	strm->priv = fp;
	strm->ops = &szl_file_ops;
	strm->closed = 0;

	*ret = szl_new_stream(interp, strm, "file");
	if (!*ret) {
		szl_stream_free(strm);
		return SZL_ERR;
	}

	return SZL_OK;
}

static enum szl_res szl_io_proc_open(struct szl_interp *interp,
                                     const int objc,
                                     struct szl_obj **objv,
                                     struct szl_obj **ret)
{
	const char *path, *mode;
	FILE *fp;
	size_t len;
	enum szl_res res;

	path = szl_obj_str(objv[1], &len);
	if (!path || !len)
		return SZL_ERR;

	mode = szl_obj_str(objv[2], &len);
	if (!mode || !len)
		return SZL_ERR;

	fp = fopen(path, mode);
	if (!fp)
		return SZL_ERR;

	res = szl_new_file(interp, fp, ret);
	if (res != SZL_OK) {
		fclose(fp);
		return SZL_ERR;
	}

	return res;
}

static enum szl_res szl_io_proc_fdopen(struct szl_interp *interp,
                                       const int objc,
                                       struct szl_obj **objv,
                                       struct szl_obj **ret)
{
	const char *mode;
	FILE *fp;
	szl_int fd;
	int fl;
	enum szl_res res;

	res = szl_obj_int(objv[1], &fd);
	if ((res != SZL_OK) || (fd < 0) || (fd > INT_MAX))
		return res;

	fl = fcntl((int)fd, F_GETFL);
	if (fl < 0)
		return SZL_ERR;

	if (fl & O_RDWR)
		mode = "r+";
	else if (fl & O_WRONLY)
		mode = "w";
	else
		mode = "r";

	fp = fdopen((int)fd, mode);
	if (!fp)
		return SZL_ERR;

	res = szl_new_file(interp, fp, ret);
	if (res != SZL_OK)
		fclose(fp);

	return res;
}

static enum szl_res szl_io_proc_dup(struct szl_interp *interp,
                                    const int objc,
                                    struct szl_obj **objv,
                                    struct szl_obj **ret)
{
	szl_int fd;
	enum szl_res res;

	res = szl_obj_int(objv[1], &fd);
	if ((res != SZL_OK) || (fd < 0) || (fd > INT_MAX))
		return res;

	fd = (szl_int)dup((int)fd);
	if (fd < 0)
		return SZL_ERR;

	szl_set_result_int(interp, ret, fd);
	if (*ret)
		return SZL_OK;

	return SZL_ERR;
}

enum szl_res szl_init_io(struct szl_interp *interp)
{
	if ((!szl_new_proc(interp,
	                   "open",
	                   3,
	                   3,
	                   "open path mode",
	                   szl_io_proc_open,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "_io.fdopen",
	                   2,
	                   2,
	                   "_io.fdopen handle",
	                   szl_io_proc_fdopen,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "_io.dup",
	                   2,
	                   2,
	                   "_io.dup handle",
	                   szl_io_proc_dup,
	                   NULL,
	                   NULL)))
		return SZL_ERR;

	return SZL_OK;
}
