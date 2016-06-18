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

#include <string.h>

#include "szl.h"

static
enum szl_res szl_exc_proc_try(struct szl_interp *interp,
                              const int objc,
                              struct szl_obj **objv)
{
	struct szl_obj *obj;
	const char *try = NULL, *except = NULL, *finally = NULL;
	const char *s;
	size_t tlen, elen = 0, flen;
	enum szl_res res, eres = SZL_OK;

	switch (objc) {
		case 6:
			s = szl_obj_str(interp, objv[4], NULL);
			if (!s || (strcmp("finally", s) != 0))
				return SZL_ERR;

			finally = szl_obj_str(interp, objv[5], &flen);
			if (!finally)
				return SZL_ERR;

			/* fall through */

		case 4:
			s = szl_obj_str(interp, objv[2], NULL);
			if (!s)
				return SZL_ERR;

			if (strcmp("except", s) == 0) {
				except = szl_obj_str(interp, objv[3], &elen);
				if (!except)
					return SZL_ERR;

				/* fall through */

			}
			else if (strcmp("finally", s) == 0) {
				finally = szl_obj_str(interp, objv[3], &flen);
				if (!finally)
					return SZL_ERR;

			} else
				return SZL_ERR;

		case 2:
			try = szl_obj_str(interp, objv[1], &tlen);
			if (!try)
				return SZL_ERR;

			break;

		default:
			return szl_usage(interp, objv[0]);
	}

	res = szl_run(interp, try, tlen);
	obj = szl_obj_ref(interp->last);
	if ((res == SZL_ERR) && elen) {
		eres = szl_run(interp, except, elen);
		if (eres == SZL_ERR) {
			szl_obj_unref(obj);
			obj = szl_obj_ref(interp->last);
		}
	}

	if (finally)
		szl_run(interp, finally, flen);

	szl_set_result(interp, obj);

	if (res == SZL_ERR) {
		if (eres == SZL_ERR)
			return SZL_ERR;
		return SZL_OK;
	}

	return res;
}

static
enum szl_res szl_exc_proc_throw(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	szl_set_result(interp, szl_obj_ref(objv[1]));
	return SZL_ERR;
}

int szl_init_exc(struct szl_interp *interp)
{
	return ((szl_new_proc(interp,
	                      "try",
	                      2,
	                      6,
	                      "try exp ?except exp? ?finally exp?",
	                      szl_exc_proc_try,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "throw",
	                      1,
	                      2,
	                      "throw ?msg?",
	                      szl_exc_proc_throw,
	                      NULL,
	                      NULL)));
}

