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
                                   const unsigned int objc,   \
                                   struct szl_obj **objv)     \
{                                                             \
	szl_int ni, mi;                                           \
	szl_float nf, mf;                                         \
	                                                          \
	if (!szl_as_float(interp, objv[1], &mf) ||                \
	    !szl_as_float(interp, objv[2], &nf) ||                \
	    !szl_as_int(interp, objv[1], &mi) ||                  \
	    !szl_as_int(interp, objv[2], &ni))                    \
		return SZL_ERR;                                       \
	                                                          \
	if ((mf != (szl_float)mi) || (nf != (szl_float)ni))       \
		return szl_set_last_float(interp, mf op nf);          \
	                                                          \
	return szl_set_last_int(interp, mi op ni);                \
}

#define SZL_MATH_PROC_DIV(name, op)                           \
static                                                        \
enum szl_res szl_math_proc_## name(struct szl_interp *interp, \
                                   const unsigned int objc,   \
                                   struct szl_obj **objv)     \
{                                                             \
	szl_float m, n;                                           \
	                                                          \
	if (!szl_as_float(interp, objv[1], &m) ||                 \
	    !szl_as_float(interp, objv[2], &n))                   \
		return SZL_ERR;                                       \
	                                                          \
	if (n == 0) {                                             \
		szl_set_last_str(interp,                              \
		                 "division by 0",                     \
		                 sizeof("division by 0") - 1);        \
		return SZL_ERR;                                       \
	}                                                         \
	                                                          \
	return szl_set_last_float(interp, op);                    \
}

SZL_MATH_PROC(add, +)
SZL_MATH_PROC(sub, -)
SZL_MATH_PROC(mul, *)
SZL_MATH_PROC_DIV(div, m / n)
SZL_MATH_PROC_DIV(mod, (szl_float)fmod(m, n))

#define SZL_MATH_BIT_PROC(name, op)                           \
static                                                        \
enum szl_res szl_math_proc_## name(struct szl_interp *interp, \
                                   const unsigned int objc,   \
                                   struct szl_obj **objv)     \
{                                                             \
	szl_int ni, mi;                                           \
	                                                          \
	if (!szl_as_int(interp, objv[1], &mi) ||                  \
	    !szl_as_int(interp, objv[2], &ni))                    \
		return SZL_ERR;                                       \
	                                                          \
	return szl_set_last_int(interp, mi op ni);                \
}

SZL_MATH_BIT_PROC(and, &)
SZL_MATH_BIT_PROC(or, |)
SZL_MATH_BIT_PROC(xor, ^)

static
const struct szl_ext_export math_exports[] = {
	{
		SZL_PROC_INIT("+", "m n", 3, 3, szl_math_proc_add, NULL)
	},
	{
		SZL_PROC_INIT("-", "m n", 3, 3, szl_math_proc_sub, NULL)
	},
	{
		SZL_PROC_INIT("*", "m n", 3, 3, szl_math_proc_mul, NULL)
	},
	{
		SZL_PROC_INIT("/", "m n", 3, 3, szl_math_proc_div, NULL)
	},
	{
		SZL_PROC_INIT("%", "m n", 3, 3, szl_math_proc_mod, NULL)
	},
	{
		SZL_PROC_INIT("&", "m n", 3, 3, szl_math_proc_and, NULL)
	},
	{
		SZL_PROC_INIT("|", "m n", 3, 3, szl_math_proc_or, NULL)
	},
	{
		SZL_PROC_INIT("^", "m n", 3, 3, szl_math_proc_xor, NULL)
	}
};

int szl_init_math(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "math",
	                   math_exports,
	                   sizeof(math_exports) / sizeof(math_exports[0]));
}
