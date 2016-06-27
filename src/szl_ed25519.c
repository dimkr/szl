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

#include "ed25519/src/ed25519.h"

#include "szl.h"

static
enum szl_res szl_ed25519_proc_verify(struct szl_interp *interp,
                                     const int objc,
                                     struct szl_obj **objv)
{
	const char *sig, *data, *pub;
	size_t len;

	pub = szl_obj_str(interp, objv[3], &len);
	if (!pub || (len != 32)) {
		szl_set_result_str(interp,
		                   "the public key must be 32 bytes long",
		                   -1);
		return SZL_ERR;
	}

	sig = szl_obj_str(interp, objv[2], &len);
	if (!sig || (len != 64)) {
		szl_set_result_str(interp, "the signature must be 64 bytes long", -1);
		return SZL_ERR;
	}

	data = szl_obj_str(interp, objv[1], &len);
	if (!data || !len) {
		szl_set_result_str(interp, "the data cannot be an empty buffer", -1);
		return SZL_ERR;
	}

	if (!ed25519_verify((const unsigned char *)sig,
	                    (const unsigned char *)data,
	                    len,
	                    (const unsigned char *)pub)) {
		szl_set_result_str(interp, "the digital signature is invalid", -1);
		return SZL_ERR;
	}

	return SZL_OK;
}

static
enum szl_res szl_ed25519_proc_sign(struct szl_interp *interp,
                                   const int objc,
                                   struct szl_obj **objv)
{
	char sig[64];
	const char *data, *priv, *pub;
	size_t len, klen;

	data = szl_obj_str(interp, objv[1], &len);
	if (!len) {
		szl_set_result_str(interp, "the data cannot be an empty buffer", -1);
		return SZL_ERR;
	}

	priv = szl_obj_str(interp, objv[2], &klen);
	if (klen != 64) {
		szl_set_result_str(interp, "the private key must be 64 bytes long", -1);
		return SZL_ERR;
	}

	pub = szl_obj_str(interp, objv[3], &klen);
	if (klen != 32) {
		szl_set_result_str(interp, "the public key must be 32 bytes long", -1);
		return SZL_ERR;
	}

	ed25519_sign((unsigned char *)sig,
	             (const unsigned char *)data,
	             len,
	             (const unsigned char *)pub,
	             (const unsigned char *)priv);

	return szl_set_result_str(interp, sig, sizeof(sig));
}

static
enum szl_res szl_ed25519_proc_keypair(struct szl_interp *interp,
                                      const int objc,
                                      struct szl_obj **objv)
{
	unsigned char buf[128] = {0};
	struct szl_obj *items[2];
	enum szl_res res;

	if (ed25519_create_seed(buf + 96) != 0)
		return SZL_ERR;

	ed25519_create_keypair(buf, buf + 32, buf + 96);

	items[0] = szl_new_str((char *)buf + 32, 64);
	if (!items[0])
		return SZL_ERR;

	items[1] = szl_new_str((char *)buf, 32);
	if (!items[1]) {
		szl_obj_unref(items[0]);
		return SZL_ERR;
	}

	res = szl_set_result_list(interp, items, 2);
	szl_obj_unref(items[1]);
	szl_obj_unref(items[0]);
	return res;
}

int szl_init_ed25519(struct szl_interp *interp)
{
	return ((szl_new_proc(interp,
	                      "ed25519.verify",
	                      4,
	                      4,
	                      "ed25519.verify data sig pub",
	                      szl_ed25519_proc_verify,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "ed25519.sign",
	                      4,
	                      4,
	                      "ed25519.sign data priv pub",
	                      szl_ed25519_proc_sign,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "ed25519.keypair",
	                      1,
	                      1,
	                      "ed25519.keypair",
	                      szl_ed25519_proc_keypair,
	                      NULL,
	                      NULL)));
}
