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
#include <signal.h>
#include <string.h>

#include "szl.h"

static
enum szl_res szl_szl_proc(struct szl_interp *interp,
                          const int objc,
                          struct szl_obj **objv)
{
	struct szl_interp *interp2 = (struct szl_interp *)objv[0]->priv;
	const char *s;
	size_t len;
	enum szl_res res;

	s = szl_obj_str(objv[1], &len);
	if (!s || !len)
		return SZL_ERR;

	if (strcmp("eval", s) != 0)
		return SZL_ERR;

	s = szl_obj_str(objv[2], &len);
	if (!s || !len)
		return SZL_ERR;

	res = szl_run_const(interp2, s, len);
	szl_set_result(interp, szl_obj_ref(interp2->last));
	return res;
}

static
void szl_szl_del(void *priv)
{
	szl_interp_free((struct szl_interp *)priv);
}

static
enum szl_res szl_szl_proc_szl(struct szl_interp *interp,
                              const int objc,
                              struct szl_obj **objv)
{
	char name[sizeof("szl:"SZL_PASTE(SZL_INT_MIN))];
	struct szl_obj *proc;
	struct szl_interp *interp2;

	interp2 = szl_interp_new();
	if (!interp2)
		return SZL_ERR;

	szl_new_obj_name(interp, "szl", name, sizeof(name), interp2);
	proc = szl_new_proc(interp,
	                    name,
	                    3,
	                    3,
	                    "szl eval exp",
	                    szl_szl_proc,
	                    szl_szl_del,
	                    interp2);
	if (!proc) {
		szl_interp_free(interp2);
		return SZL_ERR;
	}

	return szl_set_result(interp, szl_obj_ref(proc));
}

int szl_init_szl(struct szl_interp *interp)
{
	return szl_new_proc(interp,
	                    "szl",
	                    1,
	                    1,
	                    "szl",
	                    szl_szl_proc_szl,
	                    NULL,
	                    NULL) ? 1 : 0;
}

