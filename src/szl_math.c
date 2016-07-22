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
#include <math.h>

#include "szl.h"

#define SZL_MATH_PROC(name, op)                               \
static                                                        \
enum szl_res szl_math_proc_## name(struct szl_interp *interp, \
                                   const int objc,            \
                                   struct szl_obj **objv)     \
{                                                             \
	szl_int ni, mi;                                           \
	szl_double nd, md;                                        \
	                                                          \
	if (!szl_obj_double(interp, objv[1], &md) ||              \
	    !szl_obj_double(interp, objv[2], &nd) ||              \
	    !szl_obj_int(interp, objv[1], &mi) ||                 \
	    !szl_obj_int(interp, objv[2], &ni))                   \
		return SZL_ERR;                                       \
	                                                          \
	if ((md != (szl_double)mi) || (nd != (szl_double)ni))     \
		return szl_set_result_double(interp, md + nd);        \
	                                                          \
	return szl_set_result_int(interp, mi op ni);              \
}

#define SZL_MATH_PROC_DIV(name, op)                           \
static                                                        \
enum szl_res szl_math_proc_## name(struct szl_interp *interp, \
                                   const int objc,            \
                                   struct szl_obj **objv)     \
{                                                             \
	szl_int ni, mi;                                           \
	szl_double nd, md;                                        \
	                                                          \
	if (!szl_obj_double(interp, objv[1], &md) ||              \
	    !szl_obj_double(interp, objv[2], &nd) ||              \
	    !szl_obj_int(interp, objv[1], &mi) ||                 \
	    !szl_obj_int(interp, objv[2], &ni))                   \
		return SZL_ERR;                                       \
	                                                          \
	if (nd == 0) {                                            \
		szl_set_result_str(interp, "division by 0", -1);      \
		return SZL_ERR;                                       \
	}                                                         \
	                                                          \
	if ((md != (szl_double)mi) || (nd != (szl_double)ni))     \
		return szl_set_result_double(interp, md + nd);        \
	                                                          \
	return szl_set_result_int(interp, mi op ni);              \
}

SZL_MATH_PROC(add, +)
SZL_MATH_PROC(sub, -)
SZL_MATH_PROC(mul, *)
SZL_MATH_PROC_DIV(div, /)
SZL_MATH_PROC_DIV(mod, %)

int szl_init_math(struct szl_interp *interp)
{
	return (szl_new_proc(interp,
	                     "+",
	                     3,
	                     3,
	                     "+ m n",
	                     szl_math_proc_add,
	                     NULL,
	                     NULL) &&
	        szl_new_proc(interp,
	                     "-",
	                     3,
	                     3,
	                     "- m n",
	                     szl_math_proc_sub,
	                     NULL,
	                     NULL) &&
	        szl_new_proc(interp,
	                     "*",
	                     3,
	                     3,
	                     "* m n",
	                     szl_math_proc_mul,
	                     NULL,
	                     NULL) &&
	        szl_new_proc(interp,
	                     "/",
	                     3,
	                     3,
	                     "/ m n",
	                     szl_math_proc_div,
	                     NULL,
	                     NULL) &&
	        szl_new_proc(interp,
	                     "%",
	                     3,
	                     3,
	                     "% m n",
	                     szl_math_proc_mod,
	                     NULL,
	                     NULL));
}
