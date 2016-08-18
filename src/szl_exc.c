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
                              const unsigned int objc,
                              struct szl_obj **objv)
{
	struct szl_obj *obj;
	char *s, *try = NULL, *except = NULL, *finally = NULL;
	size_t tlen, elen = 0, flen;
	enum szl_res res, eres = SZL_OK, fres;

	switch (objc) {
		case 6:
			if (!szl_as_str(interp, objv[4], &s, NULL) ||
			   (strcmp("finally", s) != 0))
				return SZL_ERR;

			if (!szl_as_str(interp, objv[5], &finally, &flen))
				return SZL_ERR;

			/* fall through */

		case 4:
			if (!szl_as_str(interp, objv[2], &s, NULL))
				return SZL_ERR;

			if (strcmp("except", s) == 0) {
				if (!szl_as_str(interp, objv[3], &except, &elen))
					return SZL_ERR;

				/* fall through */

			}
			else if (strcmp("finally", s) == 0) {
				if (!szl_as_str(interp, objv[3], &finally, &flen))
					return SZL_ERR;

			} else
				return SZL_ERR;

		case 2:
			if (!szl_as_str(interp, objv[1], &try, &tlen))
				return SZL_ERR;

			break;

		default:
			return szl_set_last_help(interp, objv[0]);
	}

	res = szl_run(interp, try, tlen);
	obj = szl_ref(interp->last);
	if ((res == SZL_ERR) && elen) {
		eres = szl_run(interp, except, elen);
		if (eres == SZL_ERR) {
			szl_unref(obj);
			obj = szl_ref(interp->last);
		} else if (eres == SZL_EXIT)
			res = SZL_EXIT;
	}

	if (finally) {
		fres = szl_run(interp, finally, flen);
		if (fres == SZL_EXIT)
			res = SZL_EXIT;
	}

	szl_set_last(interp, obj);

	if (res == SZL_ERR) {
		if (eres == SZL_ERR)
			return SZL_ERR;
		return SZL_OK;
	}

	return res;
}

static
enum szl_res szl_exc_proc_throw(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	if (objc == 2)
		szl_set_last(interp, szl_ref(objv[1]));

	return SZL_ERR;
}

static
const struct szl_ext_export exc_exports[] = {
	{
		SZL_PROC_INIT("try",
		              "exp ?except exp? ?finally exp?",
		              2,
		              6,
		              szl_exc_proc_try,
		              NULL)
	},
	{
		SZL_PROC_INIT("throw", "?msg?", 1, 2, szl_exc_proc_throw, NULL)
	}
};

int szl_init_exc(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "exc",
	                   exc_exports,
	                   sizeof(exc_exports) / sizeof(exc_exports[0]));
}
