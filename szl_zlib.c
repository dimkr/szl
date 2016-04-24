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

#include <zlib.h>

#include "szl.h"

#define _PASTE(x) # x
#define PASTE(x) _PASTE(x)

#define WBITS_GZIP (MAX_WBITS | 16)
/* use small 64K chunks if no size was specified during decompression, to reduce
 * memory consumption */
#define DEF_DECOMPRESS_BUFSIZ (64 * 1024)

static enum szl_res szl_zlib_proc_crc32(struct szl_interp *interp,
                                        const int objc,
                                        struct szl_obj **objv,
                                        struct szl_obj **ret)
{
	const char *s;
	szl_int init;
	size_t len;

	if (objc == 2)
		init = crc32(0L, Z_NULL, 0);
	else {
		if (szl_obj_int(objv[2], &init) != SZL_OK)
			return SZL_ERR;
	}

	s = szl_obj_str(objv[1], &len);
	if (!s)
		return SZL_ERR;

	szl_set_result_int(interp,
	                   ret,
	                   (szl_int)(crc32((uLong)init,
	                                   (const Bytef *)s,
	                                   (uInt)len) & 0xFFFFFFFF));
	if (!*ret)
		return SZL_ERR;

	return SZL_OK;
}

static int szl_zlib_compress(struct szl_interp *interp,
							 const char *in,
							 const size_t len,
							 const long level,
							 const int wbits,
							 struct szl_obj **ret)
{
	z_stream strm = {0};
	Bytef *buf;
	struct szl_obj *obj;

	if ((level != Z_DEFAULT_COMPRESSION) &&
	    ((level < Z_NO_COMPRESSION) || (level > Z_BEST_COMPRESSION))) {
		szl_set_result_str(interp, ret, "level must be 0 to 9");
		return SZL_ERR;
	}

	if (deflateInit2(&strm,
	                 level,
	                 Z_DEFLATED,
	                 wbits,
	                 MAX_MEM_LEVEL,
	                 Z_DEFAULT_STRATEGY) != Z_OK)
		return SZL_ERR;

	strm.avail_out = deflateBound(&strm, (uLong)len);
	if (strm.avail_out > INT_MAX) {
		deflateEnd(&strm);
		return SZL_ERR;
	}
	buf = (Bytef *)malloc((size_t)strm.avail_out);
	strm.next_out = buf;
	strm.next_in = (Bytef *)in;
	strm.avail_in = (uInt)len;

	/* always compress in one pass - the return value holds the entire
	 * decompressed data anyway, so there's no reason to do chunked
	 * decompression */
	if (deflate(&strm, Z_FINISH) != Z_STREAM_END) {
		free(strm.next_out);
		deflateEnd(&strm);
		return SZL_ERR;
	}

	deflateEnd(&strm);

	if (strm.total_out > INT_MAX) {
		free(strm.next_out);
		return SZL_ERR;
	}

	obj = szl_new_str_noalloc((char *)buf, (size_t)strm.total_out);
	if (!obj) {
		free(buf);
		return SZL_ERR;
	}

	szl_set_result(ret, obj);
	return SZL_OK;
}

static enum szl_res szl_zlib_proc_deflate(struct szl_interp *interp,
                                          const int objc,
                                          struct szl_obj **objv,
                                          struct szl_obj **ret)
{
	szl_int level = Z_DEFAULT_COMPRESSION;
	const char *in;
	size_t len;

	if ((objc == 3) &&
	    ((szl_obj_int(objv[2], &level) != SZL_OK) ||
	     ((level != Z_DEFAULT_COMPRESSION) &&
	      ((level <= Z_NO_COMPRESSION) ||
	      (level >= Z_BEST_COMPRESSION)))))
		return SZL_ERR;

	in = szl_obj_str(objv[1], &len);
	if (!in)
		return SZL_ERR;

	return szl_zlib_compress(interp, in, len, (long)level, -MAX_WBITS, ret);
}

static enum szl_res szl_zlib_decompress(struct szl_interp *interp,
                                        const char *in,
                                        const size_t len,
                                        const size_t bufsiz,
                                        const int wbits,
                                        struct szl_obj **ret)
{
	z_stream strm = {0};
	void *buf;
	struct szl_obj *out;
	int res;

	if ((bufsiz <= 0) || (bufsiz > INT_MAX)) {
		szl_set_result_str(interp,
		                   ret,
		                   "buffer size must be 0 to "PASTE(INT_MAX));
		return SZL_ERR;
	}

	if (inflateInit2(&strm, wbits) != Z_OK)
		return SZL_ERR;

	/* allocate a buffer - decompression is done in chunks, into this buffer;
	 * when the decompressed data size is given, decompression is faster because
	 * it's done in one pass, with less memcpy() overhead */
	buf = malloc(bufsiz);
	if (!buf)
		return SZL_ERR;

	out = szl_new_str("", 0);

	strm.next_in = (Bytef*)in;
	strm.avail_in = (uInt)len;
	do {
		do {
			strm.next_out = buf;
			strm.avail_out = (uInt)bufsiz;

			res = inflate(&strm, Z_NO_FLUSH);
			switch (res) {
			case Z_OK:
			case Z_STREAM_END:
				/* append each chunk to the output object */
				if (szl_append(out,
				               buf,
				               bufsiz - (size_t)strm.avail_out) == SZL_OK)
					break;

			default:
				szl_obj_unref(out);
				free(buf);
				inflateEnd(&strm);
				if (strm.msg != NULL)
					szl_set_result_str(interp, ret, strm.msg);
				return SZL_ERR;
			}
		} while (strm.avail_out == 0);
	} while (res != Z_STREAM_END);

	/* free memory used for decompression before we assign the return value */
	free(buf);
	inflateEnd(&strm);

	szl_set_result(ret, out);
	return SZL_OK;
}

static enum szl_res szl_zlib_proc_inflate(struct szl_interp *interp,
                                          const int objc,
                                          struct szl_obj **objv,
                                          struct szl_obj **ret)
{
	szl_int bufsiz = DEF_DECOMPRESS_BUFSIZ;
	const char *in;
	size_t len;

	if (objc != 2) {
		if (szl_obj_int(objv[2], &bufsiz) != SZL_OK)
			return SZL_ERR;

		if ((bufsiz <= 0) || (bufsiz > INT_MAX)) {
			szl_set_result_str(interp,
			                   ret,
			                   "buffer size must be 0 to "PASTE(INT_MAX));
			return SZL_ERR;
		}
	}

	in = szl_obj_str(objv[1], &len);
	if (!in || !len)
		return SZL_ERR;

	return szl_zlib_decompress(interp, in, len, bufsiz, -MAX_WBITS, ret);
}

static enum szl_res szl_zlib_proc_gzip(struct szl_interp *interp,
                                       const int objc,
                                       struct szl_obj **objv,
                                       struct szl_obj **ret)
{
	szl_int level = Z_DEFAULT_COMPRESSION;
	const char *in;
	size_t len;

	if ((objc == 3) && (szl_obj_int(objv[2], &level) != SZL_OK)) {
		szl_usage(interp, ret, objv[0]);
		return SZL_ERR;
	}

	in = szl_obj_str(objv[1], &len);
	if (!in || !len)
		return SZL_ERR;

	return szl_zlib_compress(interp, in, len, level, WBITS_GZIP, ret);
}

static enum szl_res szl_zlib_proc_gunzip(struct szl_interp *interp,
                                         const int objc,
                                         struct szl_obj **objv,
                                         struct szl_obj **ret)
{
	const char *in;
	szl_int bufsiz = DEF_DECOMPRESS_BUFSIZ;
	size_t len;

	if ((objc == 2) &&
	    ((szl_obj_int(objv[2], &bufsiz) != SZL_OK) ||
	    (bufsiz < 0) ||
	    (bufsiz > LONG_MAX))) {
		szl_usage(interp, ret, objv[0]);
		return SZL_ERR;
	}

	in = szl_obj_str(objv[1], &len);
	if (!in)
		return SZL_ERR;

	return szl_zlib_decompress(interp, in, len, bufsiz, WBITS_GZIP, ret);
}

SZL_EXT_INIT
enum szl_res szl_init_zlib(struct szl_interp *interp)
{
	if ((!szl_new_proc(interp,
	                   "zlib.crc32",
	                   2,
	                   3,
	                   "zlib.crc32 buf ?init?",
	                   szl_zlib_proc_crc32,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "zlib.deflate",
	                   2,
	                   3,
	                   "zlib.deflate string ?level?",
	                   szl_zlib_proc_deflate,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "zlib.inflate",
	                   2,
	                   3,
	                   "zlib.inflate data ?bufsiz?",
	                   szl_zlib_proc_inflate,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "zlib.gzip",
	                   2,
	                   3,
	                   "zlib.gzip data ?level?",
	                   szl_zlib_proc_gzip,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "zlib.gunzip",
	                   2,
	                   3,
	                   "zlib.gunzip data ?bufsiz?",
	                   szl_zlib_proc_gunzip,
	                   NULL,
	                   NULL)))
		return SZL_ERR;

	return SZL_OK;
}
