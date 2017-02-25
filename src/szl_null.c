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

#include <stdlib.h>

#include "szl.h"

static
ssize_t szl_null_read(struct szl_interp *interp,
                      void *priv,
                      unsigned char *buf,
                      const size_t len,
                      int *more)
{
	return 0;
}

static
ssize_t szl_null_write(struct szl_interp *interp,
                       void *priv,
                       const unsigned char *buf,
                       const size_t len)
{
	return (ssize_t)len;
}

static
ssize_t szl_null_size(void *priv)
{
	return 0;
}

static
const struct szl_stream_ops szl_null_ops = {
	.read = szl_null_read,
	.write = szl_null_write,
	.size = szl_null_size
};

int szl_init_null(struct szl_interp *interp)
{
	static struct szl_ext_export null_exports[] = {
		{
			SZL_STREAM_INIT("null")
		}
	};
	struct szl_stream *strm;

	strm = (struct szl_stream *)szl_malloc(interp, sizeof(struct szl_stream));
	if (!strm)
		return 0;

	strm->ops = &szl_null_ops;
	strm->flags = SZL_STREAM_BLOCKING;
	strm->priv = NULL;
	strm->buf = NULL;

	null_exports[0].val.proc.priv = strm;

	if (!szl_new_ext(interp,
	                 "null",
	                 null_exports,
	                 sizeof(null_exports) / sizeof(null_exports[0]))) {
		szl_stream_free(strm);
		return 0;
	}

	return 1;
}
