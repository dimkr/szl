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

#include "szl.h"

#if BUFSIZ > 64 * 1024
#	define SZL_BINARY_FILE_BUFSIZ BUFSIZ
#else
#	define SZL_BINARY_FILE_BUFSIZ 64 * 1024
#endif

static const char szl_io_inc[] = {
#include "szl_io.inc"
};

static
ssize_t szl_file_read(void *priv, unsigned char *buf, const size_t len)
{
	size_t ret;

	ret = fread(buf, 1, len, (FILE *)priv);
	if (!ret) {
		if (ferror((FILE *)priv))
			return -1;
	}

	return (ssize_t)ret;
}

static
ssize_t szl_file_write(void *priv, const unsigned char *buf, const size_t len)
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

static const struct szl_stream_ops szl_file_ops = {
	szl_file_read,
	szl_file_write,
	szl_file_flush,
	szl_file_close,
	NULL,
	szl_file_handle
};

static
int szl_io_enable_buffering(struct szl_stream *strm, FILE *fp)
{
	strm->buf = malloc(SZL_BINARY_FILE_BUFSIZ);
	if (!strm->buf)
		return 0;

	if (setvbuf(fp, strm->buf, _IOFBF, SZL_BINARY_FILE_BUFSIZ) != 0) {
		free(strm->buf);
		return 0;
	}

	return 1;
}

static
int szl_new_file(struct szl_interp *interp,
                 FILE *fp,
                 const int binary,
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

	obj = szl_new_stream(interp, strm, "file");
	if (!obj) {
		szl_stream_free(strm);
		return 0;
	}

	if (binary) {
		if (!szl_io_enable_buffering(strm, fp)) {
			szl_stream_free(strm);
			return 0;
		}
	}

	szl_set_result(interp, obj);
	return 1;
}

static
const char *szl_file_mode(struct szl_interp *interp,
                          const char *mode,
                          int *binary)
{
	if ((strcmp("r", mode) == 0) ||
	    (strcmp("w", mode) == 0) ||
	    (strcmp("a", mode) == 0)) {
		*binary = 0;
		return mode;
	}
	else if (strcmp("rb", mode) == 0) {
		*binary = 1;
		return "r";
	}
	else if (strcmp("wb", mode) == 0) {
		*binary = 1;
		return "w";
	}
	else if (strcmp("ab", mode) == 0) {
		*binary = 1;
		return "a";
	}
	else if ((strcmp("r+", mode) == 0) ||
	         (strcmp("w+", mode) == 0) ||
	         (strcmp("a+", mode) == 0)) {
		*binary = 0;
		return mode;
	}
	else if (strcmp("r+b", mode) == 0) {
		*binary = 1;
		return "r+";
	}
	else if (strcmp("w+b", mode) == 0) {
		*binary = 1;
		return "w+";
	}
	else if (strcmp("a+b", mode) == 0) {
		*binary = 1;
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
	int binary;

	path = szl_obj_str(objv[1], &len);
	if (!path || !len)
		return SZL_ERR;

	mode = szl_obj_str(objv[2], &len);
	if (!mode || !len)
		return SZL_ERR;

	rmode = szl_file_mode(interp, mode, &binary);
	if (!rmode)
		return SZL_ERR;

	fp = fopen(path, rmode);
	if (!fp)
		return SZL_ERR;

	if (!szl_new_file(interp, fp, binary, 0)) {
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
	int binary;

	if ((!szl_obj_int(objv[1], &fd)) || (fd < 0) || (fd > INT_MAX))
		return SZL_ERR;

	mode = szl_obj_str(objv[2], &len);
	if (!mode || !len)
		return SZL_ERR;

	rmode = szl_file_mode(interp, mode, &binary);
	if (!rmode)
		return SZL_ERR;

	fp = fdopen((int)fd, rmode);
	if (!fp)
		return SZL_ERR;

	if (!szl_new_file(interp, fp, binary, 0)) {
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

	if ((!szl_obj_int(objv[1], &fd)) || (fd < 0) || (fd > INT_MAX))
		return SZL_ERR;

	fd = (szl_int)dup((int)fd);
	if (fd < 0)
		return SZL_ERR;

	return szl_set_result_int(interp, fd);
}

static
int szl_io_wrap_stream(struct szl_interp *interp, FILE *fp, const char *name)
{
	struct szl_obj *obj;
	struct szl_stream *strm;

	strm = (struct szl_stream *)malloc(sizeof(struct szl_stream));
	if (!strm)
		return 0;

	strm->ops = &szl_file_ops;
	strm->keep = 1;
	strm->closed = 0;
	strm->priv = fp;
	strm->buf = NULL;

	obj = szl_new_stream(interp, strm, "stream");
	if (!obj) {
		szl_stream_free(strm);
		return 0;
	}

	if (!szl_local(interp, interp->global, name, obj)) {
		szl_obj_unref(obj);
		return 0;
	}

	szl_obj_unref(obj);
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
	        (szl_run_const(interp,
	                       szl_io_inc,
	                       sizeof(szl_io_inc) - 1) == SZL_OK));
}
