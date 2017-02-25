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

#include <stdlib.h>
#include <string.h>

#include "szl.h"

static const char szl_szl_inc[] = {
#include "szl_szl.h"
};

static
void szl_interp_del(void *priv)
{
	szl_free_interp((struct szl_interp *)priv);
}

static
enum szl_res szl_szl_interp_proc(struct szl_interp *interp,
                                 struct szl_interp *interp2,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	char *op;
	enum szl_res res;

	if (!szl_as_str(interp, objv[1], &op, NULL))
		return SZL_ERR;

	if (strcmp("eval", op) != 0)
		return szl_set_last_help(interp, objv[0]);

	res = szl_run_obj(interp2, objv[2]);
	if (interp != interp2)
		szl_set_last(interp, szl_ref(interp2->last));

	return res;
}

static
enum szl_res szl_interp_proc(struct szl_interp *interp,
                             const unsigned int objc,
                             struct szl_obj **objv)
{
	return szl_szl_interp_proc(interp,
	                           (struct szl_interp *)objv[0]->priv,
	                           objc,
	                           objv);
}

static
enum szl_res szl_szl_proc_interp(struct szl_interp *interp,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	struct szl_interp *interp2;
	struct szl_obj *argl = objv[1], **argov, *name, *proc;
	char **argv;
	size_t argc, i;

	if (((objc == 1) && !szl_get(interp, interp->args, &argl)) ||
	    !szl_as_list(interp, argl, &argov, &argc))
		return SZL_ERR;

	if (argc) {
		if (argc > (INT_MAX / sizeof(char *)))
			return SZL_ERR;

		argv = (char **)szl_malloc(interp, sizeof(char *) * argc);
		if (!argv)
			return SZL_ERR;

		for (i = 0; i < argc; ++i) {
			if (!szl_as_str(interp, argov[i], &argv[i], NULL)) {
				free(argv);
				return SZL_ERR;
			}
		}

		interp2 = szl_new_interp((int)argc, argv);
		free(argv);
	}
	else
		interp2 = szl_new_interp(0, NULL);

	if (!interp2)
		return SZL_ERR;

	name = szl_new_str_fmt(interp, "szl:%"PRIxPTR, (uintptr_t)interp2);
	if (!name) {
		szl_free_interp(interp2);
		return SZL_ERR;
	}

	proc = szl_new_proc(interp,
	                    name,
	                    3,
	                    3,
	                    "eval code",
	                    szl_interp_proc,
	                    szl_interp_del,
	                    interp2);
	if (!proc) {
		szl_free(name);
		szl_free_interp(interp2);
		return SZL_ERR;
	}

	szl_unref(name);
	return szl_set_last(interp, proc);
}

static
enum szl_res szl_szl_proc_this(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	return szl_szl_interp_proc(interp, interp, objc, objv);
}

static
const struct szl_ext_export szl_exports[] = {
	{
		SZL_PROC_INIT("szl.interp",
		              "?args?",
		              1,
		              2,
		              szl_szl_proc_interp,
		              NULL)
	},
	{
		SZL_PROC_INIT("szl.this",
		              "eval code",
		              3,
		              3,
		              szl_szl_proc_this,
		              NULL)
	},
};

int szl_init_szl(struct szl_interp *interp)
{
	return (szl_new_ext(interp,
	                    "szl",
	                    szl_exports,
	                    sizeof(szl_exports) / sizeof(szl_exports[0])) &&
	        (szl_run(interp, szl_szl_inc, sizeof(szl_szl_inc) - 1) == SZL_OK));
}
