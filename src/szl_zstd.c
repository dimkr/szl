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

#include "zstd/lib/zstd.h"

#include "szl.h"

/* same as ZWRAP_DEFAULT_CLEVEL */
#define SZL_ZSTD_DEFAULT_LEVEL 5

static
enum szl_res szl_zstd_proc_compress(struct szl_interp *interp,
	                                const unsigned int objc,
	                                struct szl_obj **objv)
{
	struct szl_obj *obj;
	char *in, *out;
	szl_int level = SZL_ZSTD_DEFAULT_LEVEL;
	size_t inlen, blen, outlen;

	if (!szl_as_str(interp, objv[1], &in, &inlen))
		return SZL_ERR;

	if ((objc == 3) &&
		((!szl_as_int(interp, objv[2], &level)) ||
		 (level < 0) ||
		 (level > (szl_int)(intptr_t)objv[0]->priv) ||
		 (level > INT_MAX))) {
		szl_set_last_fmt(interp, "bad zstd level: "SZL_INT_FMT"d", level);
		return SZL_ERR;
	}

	blen = ZSTD_compressBound(inlen);
	out = (char *)malloc(blen);
	if (!out)
		return SZL_ERR;

	outlen = ZSTD_compress(out, blen, in, inlen, (int)level);
	if (ZSTD_isError(outlen)) {
		free(out);
		szl_set_last_str(interp, ZSTD_getErrorName(outlen), -1);
		return SZL_ERR;
	}

	obj = szl_new_str_noalloc(out, outlen);
	if (!obj) {
		free(out);
		return SZL_ERR;
	}

	return szl_set_last(interp, obj);
}

static
enum szl_res szl_zstd_proc_decompress(struct szl_interp *interp,
	                                  const unsigned int objc,
	                                  struct szl_obj **objv)
{
	struct szl_obj *obj;
	char *in, *out;
	szl_int klen;
	unsigned long long blen;
	size_t inlen, outlen;

	if (!szl_as_str(interp, objv[1], &in, &inlen))
		return SZL_ERR;

	if (objc == 3) {
		if (!szl_as_int(interp, objv[2], &klen))
			return SZL_ERR;

		if ((klen <= 0) || (klen > ULONG_LONG_MAX) || (klen > SIZE_MAX)) {
			szl_set_last_str(interp, "bad size: "SZL_INT_FMT"d", klen);
			return SZL_ERR;
		}

		blen = (unsigned long long)klen;
	}
	else {
		blen = ZSTD_getDecompressedSize(in, inlen);
		if (!blen || (blen > SIZE_MAX))
			return SZL_ERR;
	}

	out = (char *)malloc((size_t)blen);
	if (!out)
		return SZL_ERR;

	outlen = ZSTD_decompress(out, (size_t)blen, in, inlen);
	if (ZSTD_isError(outlen)) {
		free(out);
		szl_set_last_str(interp, ZSTD_getErrorName(outlen), -1);
		return SZL_ERR;
	}

	obj = szl_new_str_noalloc(out, outlen);
	if (!obj) {
		free(out);
		return SZL_ERR;
	}

	return szl_set_last(interp, obj);
}

int szl_init_zstd(struct szl_interp *interp)
{
	static
	struct szl_ext_export zstd_exports[] = {
		{
			SZL_PROC_INIT("zstd.compress",
			              "str ?level?",
			              2,
			              3,
			              szl_zstd_proc_compress,
			              NULL)
		},
		{
			SZL_PROC_INIT("zstd.decompress",
			              "str ?size?",
			              2,
			              3,
			              szl_zstd_proc_decompress,
			              NULL)
		},
		{
			SZL_INT_INIT("zstd.max", 0)
		}
	};
	int max;

	max = ZSTD_maxCLevel();
	zstd_exports[0].val.proc.priv = (void *)(intptr_t)max;
	zstd_exports[2].val.i = (szl_int)max;

	return szl_new_ext(interp,
	                   "zstd",
	                   zstd_exports,
	                   sizeof(zstd_exports) / sizeof(zstd_exports[0]));
}
