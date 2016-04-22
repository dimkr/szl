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

#ifndef _SZL_H_INCLUDED
#	define _SZL_H_INCLUDED

#	include <stdint.h>
#	include <sys/types.h>

#	include <zlib.h>

#	include "szl_conf.h"

#	define SZL_MAX_PROC_OBJC 12
#	define SZL_MAX_OBJC_DIGITS sizeof("12") - 1

#	define SZL_INT_FMT "%jd"
#	define SZL_DOUBLE_FMT "%.12f"
#	define SZL_DOUBLE_SCANF_FMT "%lf"

#	define SZL_EXC_OBJ_NAME "ex"

#	define SZL_EXT_INIT_FUNC_NAME_FMT "szl_init_%s"
#	define SZL_MAX_EXT_INIT_FUNC_NAME_LEN 32
#	define SZL_EXT_PATH_FMT SZL_EXT_DIR"/szl_%s.so"

typedef intmax_t szl_int;
typedef double szl_double;
typedef unsigned char szl_bool;

typedef uLong szl_hash;

enum szl_res {
	SZL_OK,
	SZL_ERR,
	SZL_BREAK,
	SZL_CONT
};

enum szl_type {
	SZL_TYPE_STR    = 1 << 1,
	SZL_TYPE_INT    = 1 << 2,
	SZL_TYPE_DOUBLE = 1 << 3,
	SZL_TYPE_BOOL   = 1 << 4
};

struct szl_obj;
struct szl_interp;

typedef enum szl_res (*szl_proc)(struct szl_interp *,
                                 const int,
                                 struct szl_obj **,
                                 struct szl_obj **);
typedef void (*szl_delproc)(void *);

struct szl_obj {
	unsigned int refc;
	enum szl_type type;
	char *s;
	size_t len;
	szl_bool b;
	szl_int i;
	szl_double d;
	void *priv;
	szl_proc proc;
	szl_delproc del;
	int min_argc;
	int max_argc;
	const char *help;
	struct szl_local **locals;
	size_t nlocals;
	struct szl_obj *exp;
};

struct szl_builtin {
	const char *name;
	szl_proc proc;
	szl_delproc del;
	int min_argc;
	int max_argc;
	const char *help;
};

struct szl_ext {
	struct szl_obj **objs;
	unsigned int nobjs;
	void *handle;
};

struct szl_interp {
	struct szl_obj *empty;
	struct szl_obj *zero;
	struct szl_obj *one;
	uLong init;
	struct szl_obj *caller;
	struct szl_obj *current;
	struct szl_obj *global;
	struct szl_ext *exts;
	unsigned int nexts;
};

struct szl_local {
	szl_hash name;
	struct szl_obj *obj;
};

typedef enum szl_res (*szl_ext_init)(struct szl_interp *);

struct szl_interp *szl_interp_new(void);
void szl_interp_free(struct szl_interp *interp);
#	define szl_empty(interp) szl_obj_ref(interp->empty)
#	define szl_zero(interp) szl_obj_ref(interp->zero)
#	define szl_false szl_zero
#	define szl_one(interp) szl_obj_ref(interp->one)
#	define szl_true szl_one

struct szl_obj *szl_obj_ref(struct szl_obj *obj);
void szl_obj_unref(struct szl_obj *obj);

struct szl_obj *szl_new_str_noalloc(struct szl_interp *interp,
                                    char *s,
                                    size_t len);
struct szl_obj *szl_new_str(struct szl_interp *interp, const char *s, int len);
struct szl_obj *szl_new_int(struct szl_interp *interp, const szl_int i);
struct szl_obj *szl_new_double(struct szl_interp *interp, const szl_double d);
struct szl_obj *szl_new_proc(struct szl_interp *interp,
                             const char *name,
                             const int min_argc,
                             const int max_argc,
                             const char *help,
                             const szl_proc proc,
                             const szl_delproc del,
                             void *priv,
                             struct szl_obj *exp);

enum szl_res szl_append(struct szl_obj *obj,
                        const char *s,
                        int len);

char *szl_obj_str(struct szl_obj *obj, size_t *len);
char *szl_obj_strdup(struct szl_obj *obj, size_t *len);
enum szl_res szl_obj_len(struct szl_obj *obj, size_t *len);
enum szl_res szl_obj_int(struct szl_obj *obj, szl_int *i);
enum szl_res szl_obj_double(struct szl_obj *obj, szl_double *d);

szl_bool szl_obj_istrue(struct szl_obj *obj);
#	define szl_obj_isfalse !szl_obj_istrue

void szl_set_result(struct szl_obj **out, struct szl_obj *obj);
void szl_empty_result(struct szl_interp *interp, struct szl_obj **out);
void szl_set_result_str(struct szl_interp *interp,
                        struct szl_obj **out,
                        const char *s);
void szl_set_result_fmt(struct szl_interp *interp,
                        struct szl_obj **out,
                        const char *fmt,
                        ...);
void szl_set_result_int(struct szl_interp *interp,
                        struct szl_obj **out,
                        const szl_int i);
void szl_set_result_double(struct szl_interp *interp,
                           struct szl_obj **out,
                           const szl_double d);
void szl_set_result_bool(struct szl_interp *interp,
                         struct szl_obj **out,
                         const szl_bool b);
void szl_usage(struct szl_interp *interp,
               struct szl_obj **out,
               struct szl_obj *proc);

enum szl_res szl_get(struct szl_interp *interp,
                     struct szl_obj **out,
                     const char *name);
enum szl_res szl_set(struct szl_interp *interp,
                     const char *name,
                     struct szl_obj *obj);
enum szl_res szl_local(struct szl_interp *interp,
                       struct szl_obj *proc,
                       const char *name,
                       struct szl_obj *obj);

enum szl_res szl_eval(struct szl_interp *interp,
                      struct szl_obj **out,
                      const char *s);
enum szl_res szl_run(struct szl_interp *interp,
                     struct szl_obj **out,
                     char *s,
                     const size_t len);
enum szl_res szl_run_const(struct szl_interp *interp,
                           struct szl_obj **out,
                           const char *s,
                           const size_t len);

enum szl_res szl_load(struct szl_interp *interp, const char *name);
enum szl_res szl_source(struct szl_interp *interp, const char *path);

enum szl_res szl_main(int argc, char *argv[]);

#endif
