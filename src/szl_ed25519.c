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
	const char *sig, *data, *kp;
	size_t len;

	kp = szl_obj_str(interp, objv[3], &len);
	if (!kp || (len < 32)) {
		szl_set_result_str(interp,
		                   "the keypair must be at least 32 bytes long",
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
	                    (const unsigned char *)kp)) {
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
	const char *data, *kp;
	size_t len, klen;

	data = szl_obj_str(interp, objv[1], &len);
	if (!len) {
		szl_set_result_str(interp, "the data cannot be an empty buffer", -1);
		return SZL_ERR;
	}

	kp = szl_obj_str(interp, objv[2], &klen);
	if (klen != 96) {
		szl_set_result_str(interp, "the keypair must be 96 bytes long", -1);
		return SZL_ERR;
	}

	ed25519_sign((unsigned char *)sig,
	             (const unsigned char *)data, len,
	             (const unsigned char *)kp,
	             (const unsigned char *)kp + 32);
	return szl_set_result_str(interp, sig, sizeof(sig));
}

static
enum szl_res szl_ed25519_proc_keypair(struct szl_interp *interp,
                                      const int objc,
                                      struct szl_obj **objv)
{
	char kp[64 + 32];
	unsigned char seed[32];

	if (ed25519_create_seed(seed) != 0)
		return SZL_ERR;

	ed25519_create_keypair((unsigned char *)kp,
	                       (unsigned char *)kp + 64,
	                       seed);

	return szl_set_result_str(interp, kp, sizeof(kp));
}

int szl_init_ed25519(struct szl_interp *interp)
{
	return ((szl_new_proc(interp,
	                      "ed25519.verify",
	                      4,
	                      4,
	                      "ed25519.verify data sig keypair",
	                      szl_ed25519_proc_verify,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "ed25519.sign",
	                      3,
	                      3,
	                      "ed25519.sign data keypair",
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
