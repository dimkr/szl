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

#include "ed25519/src/ed25519.h"

#include "szl.h"

static
enum szl_res szl_ed25519_proc_verify(struct szl_interp *interp,
                                     const unsigned int objc,
                                     struct szl_obj **objv)
{
	char *sig, *data, *pub;
	size_t len;

	if (!szl_as_str(interp, objv[3], &pub, &len) || (len != 32)) {
		szl_set_last_str(interp,
		                   "the public key must be 32 bytes long",
		                   -1);
		return SZL_ERR;
	}

	if (!szl_as_str(interp, objv[2], &sig, &len) || (len != 64)) {
		szl_set_last_str(interp, "the signature must be 64 bytes long", -1);
		return SZL_ERR;
	}

	if (!szl_as_str(interp, objv[1], &data, &len) || !len) {
		szl_set_last_str(interp, "the data cannot be an empty buffer", -1);
		return SZL_ERR;
	}

	if (!ed25519_verify((const unsigned char *)sig,
	                    (const unsigned char *)data,
	                    len,
	                    (const unsigned char *)pub)) {
		szl_set_last_str(interp, "the digital signature is invalid", -1);
		return SZL_ERR;
	}

	return SZL_OK;
}

static
enum szl_res szl_ed25519_proc_sign(struct szl_interp *interp,
                                   const unsigned int objc,
                                   struct szl_obj **objv)
{
	char sig[64], *data, *priv, *pub;
	size_t len, klen;

	if (!szl_as_str(interp, objv[1], &data, &len))
		return SZL_ERR;

	if (!len) {
		szl_set_last_str(interp, "the data cannot be an empty buffer", -1);
		return SZL_ERR;
	}

	if (!szl_as_str(interp, objv[2], &priv, &klen))
		return SZL_ERR;

	if (klen != 64) {
		szl_set_last_str(interp, "the private key must be 64 bytes long", -1);
		return SZL_ERR;
	}

	if (!szl_as_str(interp, objv[3], &pub, &klen))
		return SZL_ERR;

	if (klen != 32) {
		szl_set_last_str(interp, "the public key must be 32 bytes long", -1);
		return SZL_ERR;
	}

	ed25519_sign((unsigned char *)sig,
	             (const unsigned char *)data,
	             len,
	             (const unsigned char *)pub,
	             (const unsigned char *)priv);

	return szl_set_last_str(interp, sig, sizeof(sig));
}

static
enum szl_res szl_ed25519_proc_keypair(struct szl_interp *interp,
                                      const unsigned int objc,
                                      struct szl_obj **objv)
{
	unsigned char buf[128] = {0};
	struct szl_obj *list;

	list = szl_new_list(interp, NULL, 0);
	if (!list)
		return SZL_ERR;

	if (ed25519_create_seed(buf + 96) != 0) {
		szl_free(list);
		return SZL_ERR;
	}

	ed25519_create_keypair(buf, buf + 32, buf + 96);

	if (!szl_list_append_str(interp, list, (const char *)buf + 32, 64) ||
	    !szl_list_append_str(interp, list, (const char *)buf, 32)) {
		szl_free(list);
		return SZL_ERR;
	}

	return szl_set_last(interp, list);
}

static
const struct szl_ext_export ed25519_exports[] = {
	{
		SZL_PROC_INIT("ed25519.verify",
		              "data sig pub",
		              4,
		              4,
		              szl_ed25519_proc_verify,
		              NULL)
	},
	{
		SZL_PROC_INIT("ed25519.sign",
		              "data priv pub",
		              4,
		              4,
		              szl_ed25519_proc_sign,
		              NULL)
	},
	{
		SZL_PROC_INIT("ed25519.keypair",
		              NULL,
		              1,
		              1,
		              szl_ed25519_proc_keypair,
		              NULL)
	}
};

int szl_init_ed25519(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "ed25519",
	                   ed25519_exports,
	                   sizeof(ed25519_exports) / sizeof(ed25519_exports[0]));
}
