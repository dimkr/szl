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
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "szl.h"

static const char szl_io_inc[] = {
#include "szl_io.inc"
};

static
ssize_t szl_file_read(struct szl_interp *interp,
                      void *priv,
                      unsigned char *buf,
                      const size_t len,
                      int *more)
{
	size_t ret;

	ret = fread(buf, 1, len, (FILE *)priv);
	if (!ret) {
		if (ferror((FILE *)priv)) {
			szl_set_result_str(interp, strerror(errno), -1);
			return -1;
		}
	}

	if (ret < len)
		*more = 0;

	return (ssize_t)ret;
}

static
ssize_t szl_file_write(struct szl_interp *interp,
                       void *priv,
                       const unsigned char *buf,
                       const size_t len)
{
	size_t total = 0, chunk;

	do {
		chunk = fwrite(buf + total, 1, len - total, (FILE *)priv);
		if (chunk == 0) {
			if (ferror((FILE *)priv)) {
				szl_set_result_str(interp, strerror(errno), -1);
				return -1;
			}
			break;
		}
		total += chunk;
	} while (total < len);

	return total;
}

static
enum szl_res szl_file_flush(void *priv)
{
	if (fflush((FILE *)priv) == EOF)
		return SZL_ERR;

	return SZL_OK;
}

static
void szl_file_close(void *priv)
{
	fclose((FILE *)priv);
}

static
szl_int szl_file_handle(void *priv)
{
	return (szl_int)fileno((FILE *)priv);
}

static
ssize_t szl_file_size(void *priv)
{
	struct stat stbuf;

	if ((fstat(fileno((FILE *)priv), &stbuf) < 0) ||
	    (stbuf.st_size > SSIZE_MAX))
		return -1;

	return (ssize_t)stbuf.st_size;
}

static
const struct szl_stream_ops szl_file_ops = {
	.read = szl_file_read,
	.write = szl_file_write,
	.flush = szl_file_flush,
	.close = szl_file_close,
	.handle = szl_file_handle,
	.size = szl_file_size
};

static
int szl_io_enable_fbf(struct szl_stream *strm, FILE *fp)
{
	strm->buf = malloc(SZL_STREAM_BUFSIZ);
	if (!strm->buf)
		return 0;

	if (setvbuf(fp, strm->buf, _IOFBF, SZL_STREAM_BUFSIZ) != 0) {
		free(strm->buf);
		strm->buf = NULL;
		return 0;
	}

	return 1;
}

static
int szl_new_file(struct szl_interp *interp,
                 FILE *fp,
                 const int bmode,
                 const szl_bool keep)
{
	struct szl_obj *obj;
	struct szl_stream *strm;

	strm = (struct szl_stream *)malloc(sizeof(struct szl_stream));
	if (!strm)
		return 0;

	strm->ops = &szl_file_ops;
	strm->keep = keep;
	strm->closed = 0;
	strm->priv = fp;
	strm->buf = NULL;
	strm->blocking = 1;

	obj = szl_new_stream(interp, strm, "file");
	if (!obj) {
		szl_stream_free(strm);
		return 0;
	}

	if (bmode == _IOFBF) {
		if (!szl_io_enable_fbf(strm, fp)) {
			szl_stream_free(strm);
			return 0;
		}
	} else if (bmode == _IONBF)
		setbuf(fp, NULL);

	szl_set_result(interp, obj);
	return 1;
}

static
const char *szl_file_mode(struct szl_interp *interp,
                          const char *mode,
                          int *bmode)
{
	if ((strcmp("r", mode) == 0) ||
	    (strcmp("w", mode) == 0) ||
	    (strcmp("a", mode) == 0)) {
		*bmode = _IOLBF;
		return mode;
	}
	else if (strcmp("rb", mode) == 0) {
		*bmode = _IOFBF;
		return "r";
	}
	else if (strcmp("wb", mode) == 0) {
		*bmode = _IOFBF;
		return "w";
	}
	else if (strcmp("ab", mode) == 0) {
		*bmode = _IOFBF;
		return "a";
	}
	else if (strcmp("ru", mode) == 0) {
		*bmode = _IONBF;
		return "r";
	}
	else if (strcmp("wu", mode) == 0) {
		*bmode = _IONBF;
		return "w";
	}
	else if (strcmp("au", mode) == 0) {
		*bmode = _IONBF;
		return "a";
	}
	else if ((strcmp("r+", mode) == 0) ||
	         (strcmp("w+", mode) == 0) ||
	         (strcmp("a+", mode) == 0)) {
		*bmode = _IOLBF;
		return mode;
	}
	else if (strcmp("r+b", mode) == 0) {
		*bmode = _IOFBF;
		return "r+";
	}
	else if (strcmp("w+b", mode) == 0) {
		*bmode = _IOFBF;
		return "w+";
	}
	else if (strcmp("a+b", mode) == 0) {
		*bmode = _IOFBF;
		return "a+";
	}
	else if (strcmp("r+u", mode) == 0) {
		*bmode = _IONBF;
		return "r+";
	}
	else if (strcmp("w+u", mode) == 0) {
		*bmode = _IONBF;
		return "w+";
	}
	else if (strcmp("a+u", mode) == 0) {
		*bmode = _IONBF;
		return "a+";
	}

	szl_set_result_fmt(interp, "invalid file mode: %s", mode);
	return NULL;
}

static
enum szl_res szl_io_proc_open(struct szl_interp *interp,
                              const int objc,
                              struct szl_obj **objv)
{
	const char *path, *mode, *rmode;
	FILE *fp;
	size_t len;
	int bmode;

	path = szl_obj_str(interp, objv[1], &len);
	if (!path || !len)
		return SZL_ERR;

	mode = szl_obj_str(interp, objv[2], &len);
	if (!mode || !len)
		return SZL_ERR;

	rmode = szl_file_mode(interp, mode, &bmode);
	if (!rmode)
		return SZL_ERR;

	fp = fopen(path, rmode);
	if (!fp) {
		szl_set_result_str(interp, strerror(errno), -1);
		return SZL_ERR;
	}

	if (!szl_new_file(interp, fp, bmode, 0)) {
		fclose(fp);
		return SZL_ERR;
	}

	return SZL_OK;
}

static
enum szl_res szl_io_proc_fdopen(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	const char *mode, *rmode;
	FILE *fp;
	szl_int fd;
	size_t len;
	int bmode;

	if ((!szl_obj_int(interp, objv[1], &fd)) || (fd < 0) || (fd > INT_MAX))
		return SZL_ERR;

	mode = szl_obj_str(interp, objv[2], &len);
	if (!mode || !len)
		return SZL_ERR;

	rmode = szl_file_mode(interp, mode, &bmode);
	if (!rmode)
		return SZL_ERR;

	fp = fdopen((int)fd, rmode);
	if (!fp) {
		szl_set_result_str(interp, strerror(errno), -1);
		return SZL_ERR;
	}

	if (!szl_new_file(interp, fp, bmode, 0)) {
		fclose(fp);
		return SZL_ERR;
	}

	return SZL_OK;
}

static
enum szl_res szl_io_proc_dup(struct szl_interp *interp,
                             const int objc,
                             struct szl_obj **objv)
{
	szl_int fd;

	if ((!szl_obj_int(interp, objv[1], &fd)) || (fd < 0) || (fd > INT_MAX))
		return SZL_ERR;

	fd = (szl_int)dup((int)fd);
	if (fd < 0) {
		szl_set_result_str(interp, strerror(errno), -1);
		return SZL_ERR;
	}

	return szl_set_result_int(interp, fd);
}

static
int szl_io_wrap_stream(struct szl_interp *interp, FILE *fp, const char *name)
{
	struct szl_obj *nameo, *obj;
	struct szl_stream *strm;

	nameo = szl_new_str(name, -1);
	if (!nameo)
		return 0;

	strm = (struct szl_stream *)malloc(sizeof(struct szl_stream));
	if (!strm) {
		szl_obj_unref(nameo);
		return 0;
	}

	strm->ops = &szl_file_ops;
	strm->keep = 1;
	strm->closed = 0;
	strm->priv = fp;
	strm->buf = NULL;
	strm->blocking = 1;

	obj = szl_new_stream(interp, strm, "stream");
	if (!obj) {
		szl_stream_free(strm);
		szl_obj_unref(nameo);
		return 0;
	}

	if ((!isatty(fileno(fp)) && !szl_io_enable_fbf(strm, fp)) ||
	    !szl_local(interp, interp->global, nameo, obj)) {
		szl_obj_unref(obj);
		szl_obj_unref(nameo);
		return 0;
	}

	szl_obj_unref(obj);
	szl_obj_unref(nameo);
	return 1;
}

int szl_init_io(struct szl_interp *interp)
{
	return ((szl_io_wrap_stream(interp, stdin, "stdin")) &&
	        (szl_io_wrap_stream(interp, stdout, "stdout")) &&
	        (szl_io_wrap_stream(interp, stderr, "stderr")) &&
	        (szl_new_proc(interp,
	                      "open",
	                      3,
	                      3,
	                      "open path mode",
	                      szl_io_proc_open,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "_io.fdopen",
	                      3,
	                      3,
	                      "_io.fdopen handle mode",
	                      szl_io_proc_fdopen,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "_io.dup",
	                      2,
	                      2,
	                      "_io.dup handle",
	                      szl_io_proc_dup,
	                      NULL,
	                      NULL)) &&
	        (szl_run(interp, szl_io_inc, sizeof(szl_io_inc) - 1) == SZL_OK));
}
