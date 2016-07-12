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

#include "lzfse/src/lzfse.h"

#include "szl.h"

static
enum szl_res szl_lzfse_op(struct szl_interp *interp,
	                      struct szl_obj **objv,
	                      size_t (*op)(uint8_t *__restrict,
	                                   size_t,
	                                   const uint8_t *__restrict,
	                                   size_t,
	                                   void *__restrict),
	                      size_t (*get_outlen)(const size_t),
	                      size_t (*get_auxlen)(void))
{
	struct szl_obj *str;
	const char *in;
	char *out;
	void *aux;
	size_t inlen, outlen;

	in = szl_obj_str(interp, objv[1], &inlen);
	if (!in)
		return SZL_ERR;

	outlen = get_outlen(inlen);
	if (!outlen) {
		szl_set_result_fmt(interp, "bad input len: %zd", inlen);
		return SZL_ERR;
	}

	out = (char *)malloc(outlen + 1);
	if (!out)
		return SZL_ERR;

	aux = malloc(get_auxlen());
	if (!aux) {
		free(out);
		return SZL_ERR;
	}

	outlen = op((uint8_t *)out, outlen, (const uint8_t *)in, inlen, aux);
	free(aux);

	if (!outlen) {
		free(out);
		return SZL_ERR;
	}

	out[outlen] = '\0';
	str = szl_new_str_noalloc(out, outlen);
	if (!str) {
		free(out);
		return SZL_ERR;
	}

	return szl_set_result(interp, str);
}

static
size_t get_encode_outlen(const size_t inlen)
{
	if (inlen == SIZE_MAX)
		return 0;

	return inlen;
}

static
enum szl_res szl_lzfse_proc_compress(struct szl_interp *interp,
	                                 const int objc,
	                                 struct szl_obj **objv)
{
	return szl_lzfse_op(interp,
	                    objv,
	                    lzfse_encode_buffer,
	                    get_encode_outlen,
	                    lzfse_encode_scratch_size);
}

static
size_t get_decode_outlen(const size_t inlen)
{
	/* same assumption as lzfse_main.c */
	if (inlen >= (SIZE_MAX / 4))
		return 0;

	return inlen * 4;
}

static
enum szl_res szl_lzfse_proc_decompress(struct szl_interp *interp,
	                                   const int objc,
	                                   struct szl_obj **objv)
{
	return szl_lzfse_op(interp,
	                    objv,
	                    lzfse_decode_buffer,
	                    get_decode_outlen,
	                    lzfse_decode_scratch_size);
}

int szl_init_lzfse(struct szl_interp *interp)
{
	return (szl_new_proc(interp,
	                     "lzfse.compress",
	                     2,
	                     2,
	                     "lzfse.compress str",
	                     szl_lzfse_proc_compress,
	                     NULL,
	                     NULL) &&
	        szl_new_proc(interp,
	                     "lzfse.decompress",
	                     2,
	                     2,
	                     "lzfse.decompress str",
	                     szl_lzfse_proc_decompress,
	                     NULL,
	                     NULL));
}

