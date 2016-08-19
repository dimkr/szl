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
			szl_set_last_str(interp, strerror(errno), -1);
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
				szl_set_last_str(interp, strerror(errno), -1);
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
                 const int keep)
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

	szl_set_last(interp, obj);
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

	szl_set_last_fmt(interp, "invalid file mode: %s", mode);
	return NULL;
}

static
enum szl_res szl_io_proc_open(struct szl_interp *interp,
                              const unsigned int objc,
                              struct szl_obj **objv)
{
	char *path, *mode;
	const char *rmode;
	FILE *fp;
	size_t len;
	int bmode;

	if (!szl_as_str(interp, objv[1], &path, &len) ||
	    !len ||
	    !szl_as_str(interp, objv[2], &mode, &len) ||
	    !len)
		return SZL_ERR;

	rmode = szl_file_mode(interp, mode, &bmode);
	if (!rmode)
		return SZL_ERR;

	fp = fopen(path, rmode);
	if (!fp) {
		szl_set_last_str(interp, strerror(errno), -1);
		return SZL_ERR;
	}

	if (!szl_new_file(interp, fp, bmode, 0)) {
		fclose(fp);
		return SZL_ERR;
	}

	return SZL_OK;
}

static
enum szl_res szl_io_proc_isatty(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	szl_int fd;
	int tty;

	if (!szl_as_int(interp, objv[1], &fd) || (fd < 0) || (fd > INT_MAX))
		return SZL_ERR;

	tty = isatty((int)fd);
	if (tty)
		return szl_set_last_bool(interp, 1);

	if ((errno == EINVAL) || (errno == ENOTTY))
		return szl_set_last_bool(interp, 0);

	return szl_set_last_str(interp, strerror(errno), -1);
}

static
struct szl_stream *szl_io_wrap_pipe(struct szl_interp *interp, FILE *fp)
{
	struct szl_stream *strm;

	strm = (struct szl_stream *)malloc(sizeof(struct szl_stream));
	if (!strm)
		return strm;

	strm->ops = &szl_file_ops;
	strm->keep = 1;
	strm->closed = 0;
	strm->priv = fp;
	strm->buf = NULL;
	strm->blocking = 1;

	if (!isatty(fileno(fp)) && !szl_io_enable_fbf(strm, fp)) {
		szl_stream_free(strm);
		return NULL;
	}

	return strm;
}

int szl_init_io(struct szl_interp *interp)
{
	static struct szl_ext_export io_exports[] = {
		{
			SZL_STREAM_INIT("stdin")
		},
		{
			SZL_STREAM_INIT("stdout")
		},
		{
			SZL_STREAM_INIT("stderr")
		},
		{
			SZL_PROC_INIT("open", "path mode", 3, 3, szl_io_proc_open, NULL)
		},
		{
			SZL_PROC_INIT("isatty", "handle", 2, 2, szl_io_proc_isatty, NULL)
		}
	};

	io_exports[0].val.proc.priv = szl_io_wrap_pipe(interp, stdin);
	if (!io_exports[0].val.proc.priv)
		return 0;

	io_exports[1].val.proc.priv = szl_io_wrap_pipe(interp, stdout);
	if (!io_exports[1].val.proc.priv) {
		szl_stream_free((struct szl_stream *)io_exports[0].val.proc.priv);
		return 0;
	}

	io_exports[2].val.proc.priv = szl_io_wrap_pipe(interp, stderr);
	if (!io_exports[2].val.proc.priv) {
		szl_stream_free((struct szl_stream *)io_exports[1].val.proc.priv);
		szl_stream_free((struct szl_stream *)io_exports[0].val.proc.priv);
		return 0;
	}

	if (!szl_new_ext(interp,
	                 "io",
	                 io_exports,
	                 sizeof(io_exports) / sizeof(io_exports[0]))) {
		szl_stream_free((struct szl_stream *)io_exports[2].val.proc.priv);
		szl_stream_free((struct szl_stream *)io_exports[1].val.proc.priv);
		szl_stream_free((struct szl_stream *)io_exports[0].val.proc.priv);
		return 0;
	}

	return (szl_run(interp,
	                szl_io_inc,
	                sizeof(szl_io_inc) - 1) == SZL_OK) ? 1 : 0;
}
