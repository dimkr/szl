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

#include "szl.h"

static
ssize_t szl_null_read(void *priv, unsigned char *buf, const size_t len)
{
	return 0;
}

static
ssize_t szl_null_write(void *priv, const unsigned char *buf, const size_t len)
{
	return (ssize_t)len;
}

static const struct szl_stream_ops szl_null_ops = {
	szl_null_read,
	szl_null_write,
	NULL,
	NULL,
	NULL,
	NULL
};

static
enum szl_res szl_null_proc_null(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	struct szl_stream *strm;

	if (!interp->null) {
		strm = (struct szl_stream *)malloc(sizeof(struct szl_stream));
		if (!strm)
			return SZL_ERR;

		strm->ops = &szl_null_ops;
		strm->closed = 0;
		strm->priv = NULL;

		interp->null = szl_new_stream(interp, strm, "null");
		if (!interp->null) {
			szl_stream_free(strm);
			return SZL_ERR;
		}
	}

	return szl_set_result(interp, szl_obj_ref(interp->null));
}

int szl_init_null(struct szl_interp *interp)
{
	return szl_new_proc(interp,
	                    "null",
	                    1,
	                    1,
	                    "null",
	                    szl_null_proc_null,
	                    NULL,
	                    NULL) ? 1 : 0;
}
