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
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <math.h>
#include <dlfcn.h>
#include <time.h>

#include "szl.h"

extern enum szl_res szl_init_builtin_exts(struct szl_interp *interp);

SZL_STATIC
enum szl_res szl_run_line(struct szl_interp *,
                          char *,
                          size_t,
                          struct szl_obj **);

struct szl_interp *szl_interp_new(void)
{
	struct szl_interp *interp;

	interp = (struct szl_interp *)malloc(sizeof(*interp));
	if (!interp)
		return NULL;

	interp->init = crc32(0, Z_NULL, 0);

	interp->empty = szl_new_str("", 0);
	if (!interp->empty) {
		free(interp);
		return NULL;
	}

	interp->space = szl_new_str(" ", 1);
	if (!interp->space) {
		szl_obj_unref(interp->empty);
		free(interp);
		return NULL;
	}

	interp->zero = NULL;
	interp->zero = szl_new_int(0);
	if (!interp->zero) {
		szl_obj_unref(interp->space);
		szl_obj_unref(interp->empty);
		free(interp);
		return NULL;
	}

	interp->one = NULL;
	interp->one = szl_new_int(1);
	if (!interp->one) {
		szl_obj_unref(interp->zero);
		szl_obj_unref(interp->space);
		szl_obj_unref(interp->empty);
		free(interp);
		return NULL;
	}

	interp->global = szl_new_proc(interp,
	                              "",
	                              -1,
	                              -1,
	                              NULL,
	                              NULL,
	                              NULL,
	                              NULL);
	if (!interp->global) {
		szl_obj_unref(interp->one);
		szl_obj_unref(interp->zero);
		szl_obj_unref(interp->space);
		szl_obj_unref(interp->empty);
		free(interp);
		return NULL;
	}

	/* increase the reference count of the global frame to 1 - szl_new_proc()
	 * does not increase the reference count if the name is empty */
	szl_obj_ref(interp->global);

	interp->current = szl_obj_ref(interp->global);
	interp->caller = szl_obj_ref(interp->global);

	interp->exts = NULL;
	interp->nexts = 0;

	if (szl_init_builtin_exts(interp) != SZL_OK) {
		szl_interp_free(interp);
		return NULL;
	}

	interp->seed = (unsigned int)time(NULL);

	return interp;
}

SZL_STATIC
void szl_unload_ext(struct szl_ext *ext)
{
	unsigned int i;

	if (ext->handle) {
		dlclose(ext->handle);
		ext->handle = NULL;
	}

	if (ext->nobjs) {
		for (i = 0; i < ext->nobjs; ++i)
			szl_obj_unref(ext->objs[i]);

		free(ext->objs);

		ext->objs = NULL;
		ext->nobjs = 0;
	}
}

void szl_interp_free(struct szl_interp *interp)
{
	unsigned int i;

	/* both should be interp->global once all code execution stops */
	szl_obj_unref(interp->current);
	szl_obj_unref(interp->caller);

	szl_obj_unref(interp->global);
	szl_obj_unref(interp->one);
	szl_obj_unref(interp->zero);
	szl_obj_unref(interp->space);
	szl_obj_unref(interp->empty);

	if (interp->nexts) {
		for (i = 0; i < interp->nexts; ++i)
			szl_unload_ext(&interp->exts[i]);

		free(interp->exts);
	}

	free(interp);
}

struct szl_obj *szl_obj_ref(struct szl_obj *obj)
{
	++obj->refc;
	return obj;
}


SZL_STATIC
void szl_free_locals(struct szl_obj *proc)
{
	size_t i;

	if (proc->nlocals) {
		for (i = 0; i < proc->nlocals; ++i) {
			szl_obj_unref(proc->locals[i]->obj);
			free(proc->locals[i]);
		}

		free(proc->locals);

		proc->locals = NULL;
		proc->nlocals = 0;
	}
}

void szl_obj_unref(struct szl_obj *obj)
{
	if (--obj->refc == 0) {
		szl_free_locals(obj);

		if (obj->del)
			obj->del(obj->priv);

		if (obj->s)
			free(obj->s);

		free(obj);
	}
}

void szl_new_obj_name(struct szl_interp *interp,
                      const char *pfix,
                      char *buf,
                      const size_t len)
{
	snprintf(buf, len, "%s.%x", pfix, rand_r(&interp->seed));
}

struct szl_obj *szl_new_str_noalloc(char *s, const size_t len)
{
	struct szl_obj *obj;

	obj = (struct szl_obj *)malloc(sizeof(*obj));
	if (!obj)
		return NULL;

	obj->s = s;
	obj->len = (size_t)len;
	obj->type = SZL_TYPE_STR;
	obj->proc = NULL;
	obj->del = NULL;
	obj->locals = NULL;
	obj->nlocals = 0;
	obj->refc = 1;

	return obj;
}

struct szl_obj *szl_new_str(const char *s, int len)
{
	struct szl_obj *obj;
	char *s2;
	size_t rlen;

	if (len < 0)
		rlen = strlen(s);
	else
		rlen = (size_t)len;

	s2 = malloc(rlen + 1);
	if (!s2)
		return NULL;
	memcpy(s2, s, len);
	s2[len] = '\0';

	obj = szl_new_str_noalloc(s2, rlen);
	if (!obj)
		free(s2);

	return obj;
}

struct szl_obj *szl_new_int(const szl_int i)
{
	struct szl_obj *obj;

	obj = (struct szl_obj *)malloc(sizeof(*obj));
	if (!obj)
		return NULL;

	obj->s = NULL;
	obj->i = i;
	obj->type = SZL_TYPE_INT;
	obj->proc = NULL;
	obj->del = NULL;
	obj->locals = NULL;
	obj->nlocals = 0;
	obj->refc = 1;

	return obj;
}

struct szl_obj *szl_new_double(const szl_double d)
{
	struct szl_obj *obj;

	obj = (struct szl_obj *)malloc(sizeof(*obj));
	if (!obj)
		return NULL;

	obj->s = NULL;
	obj->d = d;
	obj->type = SZL_TYPE_DOUBLE;
	obj->proc = NULL;
	obj->del = NULL;
	obj->locals = NULL;
	obj->nlocals = 0;
	obj->refc = 1;

	return obj;
}

struct szl_obj *szl_new_proc(struct szl_interp *interp,
                             const char *name,
                             const int min_argc,
                             const int max_argc,
                             const char *help,
                             const szl_proc proc,
                             const szl_delproc del,
                             void *priv)
{
	struct szl_obj *obj;

	obj = (struct szl_obj *)malloc(sizeof(*obj));
	if (!obj)
		return NULL;

	obj->s = strdup(name);
	if (!obj->s) {
		free(obj);
		return NULL;
	}

	obj->refc = 0;
	if (name[0] && (szl_local(interp, interp->global, name, obj) != SZL_OK)) {
		if (del)
			del(priv);
		free(obj->s);
		free(obj);
		return NULL;
	}

	obj->type = SZL_TYPE_STR | SZL_TYPE_PROC;
	obj->min_argc = min_argc;
	obj->max_argc = max_argc;
	obj->help = help;
	obj->proc = proc;
	obj->del = del;
	obj->priv = priv;
	obj->locals = NULL;
	obj->nlocals = 0;

	return obj;
}

enum szl_res szl_append(struct szl_obj *obj,
                        const char *buf,
                        const size_t len)
{
	char *s;
	size_t nlen = obj->len + (size_t)len;

	if (!szl_obj_str(obj, NULL))
		return SZL_ERR;

	s = realloc(obj->s, nlen + 1);
	if (!s)
		return SZL_ERR;

	memcpy(s + obj->len, buf, (size_t)len);
	s[nlen] = '\0';
	obj->s = s;
	obj->len = nlen;

	/* mark all additional representations as expired */
	obj->type = SZL_TYPE_STR;

	return SZL_OK;
}

enum szl_res szl_join(struct szl_interp *interp,
                      const int objc,
                      struct szl_obj *delim,
                      struct szl_obj **objv,
                      struct szl_obj **ret)
{
	struct szl_obj *obj;
	const char *delims;
	const char *s;
	size_t slen, dlen;
	enum szl_res res;
	int i, last = objc - 1;

	*ret = NULL;

	delims = szl_obj_str(delim, &dlen);
	if (!delims)
		return SZL_ERR;

	obj = szl_new_str("", 0);
	if (!obj)
		return SZL_ERR;

	for (i = 0; i < objc; ++i) {
		s = szl_obj_str(objv[i], &slen);
		if (!s) {
			szl_obj_unref(obj);
			return SZL_ERR;
		}

		res = szl_append(obj, s, slen);
		if (res != SZL_OK) {
			szl_obj_unref(obj);
			return res;
		}

		if (dlen && (i != last)) {
			res = szl_append(obj, delims, dlen);
			if (res != SZL_OK) {
				szl_obj_unref(obj);
				return res;
			}
		}
	}

	szl_set_result(ret, obj);
	return SZL_OK;
}

char *szl_obj_str(struct szl_obj *obj, size_t *len)
{
	szl_int i;
	int rlen;

	/* cast the object, if needed */
	if (!(obj->type & SZL_TYPE_STR) && !obj->s) {
		if (obj->type & SZL_TYPE_DOUBLE) {
			if (szl_obj_int(obj, &i) != SZL_OK)
				return NULL;

			if ((szl_double)i == obj->d)
				rlen = asprintf(&obj->s, SZL_INT_FMT, obj->i);
			else
				rlen = asprintf(&obj->s, SZL_DOUBLE_FMT, obj->d);
		}
		else
			rlen = asprintf(&obj->s, SZL_INT_FMT, obj->i);

		if (rlen < 0) {
			if (obj->s)
				free(obj->s);
			return NULL;
		}
		obj->len = (size_t)rlen;
		obj->type |= SZL_TYPE_STR;
	}

	if (len)
		*len = obj->len;

	return obj->s;
}

enum szl_res szl_obj_int(struct szl_obj *obj, szl_int *i)
{
	/* cast the object, if needed */
	if (!(obj->type & SZL_TYPE_INT)) {
		if (obj->type & SZL_TYPE_DOUBLE)
			obj->i = (szl_int)round(obj->d);
		else if (sscanf(obj->s, SZL_INT_FMT, &obj->i) != 1)
			return SZL_ERR;

		obj->type |= SZL_TYPE_INT;
	}

	*i = obj->i;
	return SZL_OK;
}

enum szl_res szl_obj_double(struct szl_obj *obj, szl_double *d)
{
	/* cast the object, if needed */
	if (!(obj->type & SZL_TYPE_DOUBLE)) {
		if (obj->type & SZL_TYPE_INT) {
			obj->d = (szl_double)obj->i;
		}
		else if (sscanf(obj->s, SZL_DOUBLE_SCANF_FMT, &obj->d) != 1)
			return SZL_ERR;

		obj->type |= SZL_TYPE_DOUBLE;
	}

	*d = obj->d;
	return SZL_OK;
}

char *szl_obj_strdup(struct szl_obj *obj, size_t *len)
{
	char *s;

	s = szl_obj_str(obj, len);
	if (s)
		return strndup(obj->s, obj->len);

	return NULL;
}

enum szl_res szl_obj_len(struct szl_obj *obj, size_t *len)
{
	const char *s;

	s = szl_obj_str(obj, len);
	if (!s)
		return SZL_ERR;

	return SZL_OK;
}

#if 0
char **szl_obj_list(struct szl_obj *obj, int *n)
{
	char *s;
	size_t len;

	s = szl_obj_strdup(obj, &len);
	if (!s)
		return NULL;

	szl_split(interp, s, n, NULL);
}
#endif

szl_bool szl_obj_istrue(struct szl_obj *obj)
{
	if (!(obj->type & SZL_TYPE_BOOL)) {
		obj->b = (((obj->type & SZL_TYPE_INT) && obj->i) ||
		          ((obj->type & SZL_TYPE_DOUBLE) && obj->d) ||
		          ((obj->type & SZL_TYPE_STR) &&
		            obj->s[0] &&
		           (strcmp("0", obj->s) != 0)));
		obj->type |= SZL_TYPE_BOOL;
	}

	return obj->b;
}

void szl_set_result(struct szl_obj **out, struct szl_obj *obj)
{
	if (*out)
		szl_obj_unref(*out);
	*out = obj;
}

void szl_empty_result(struct szl_interp *interp, struct szl_obj **out)
{
	szl_set_result(out, szl_empty(interp));
}

void szl_set_result_str(struct szl_interp *interp,
                        struct szl_obj **out,
                        const char *s)
{
	szl_set_result(out, szl_new_str(s, -1));
}

void szl_set_result_int(struct szl_interp *interp,
                        struct szl_obj **out,
                        const szl_int i)
{
	szl_set_result(out, szl_new_int(i));
}

void szl_set_result_double(struct szl_interp *interp,
                           struct szl_obj **out,
                           const szl_double d)
{
	szl_set_result(out, szl_new_double(d));
}

void szl_set_result_bool(struct szl_interp *interp,
                         struct szl_obj **out,
                         const szl_bool b)
{
	if (b)
		szl_set_result(out, szl_true(interp));
	else
		szl_set_result(out, szl_false(interp));
}

void szl_set_result_fmt(struct szl_interp *interp,
                        struct szl_obj **out,
                        const char *fmt,
                        ...)
{
	struct szl_obj *obj;
	va_list ap;
	char *s;
	int len;

	va_start(ap, fmt);

	len = vasprintf(&s, fmt, ap);
	if (len >= 0) {
		obj = szl_new_str_noalloc(s, len);
		if (obj)
			szl_set_result(out, obj);
		else
			free(s);
	}

 	va_end(ap);
}

void szl_usage(struct szl_interp *interp,
               struct szl_obj **out,
               struct szl_obj *proc)
{
	const char *s;

	s = szl_obj_str(proc, NULL);
	if (s)
		szl_set_result_fmt(interp, out, "bad %s usage: %s", s, proc->help);
}

void szl_unset(struct szl_obj **out)
{
	if (*out) {
		szl_obj_unref(*out);
		*out = NULL;
	}
}

SZL_STATIC
szl_hash szl_hash_name(struct szl_interp *interp, const char *name)
{
	return crc32(interp->init, (const Bytef *)name, (uInt)strlen(name));
}

SZL_STATIC
struct szl_local *szl_get_byhash(struct szl_interp *interp,
                                 struct szl_obj *proc,
                                 const szl_hash hash)
{
	size_t i;

	for (i = 0; i < proc->nlocals; ++i) {
		if (proc->locals[i]->name == hash)
			return proc->locals[i];
	}

	return NULL;
}

enum szl_res szl_get(struct szl_interp *interp,
                     struct szl_obj **out,
                     const char *name)
{
	struct szl_local *local = NULL;
	szl_hash hash;

	if (name[0]) {
		hash = szl_hash_name(interp, name);

		/* try the current frame's locals first */
		local = szl_get_byhash(interp, interp->current, hash);

		/* then, fall back to the caller's */
		if ((!local) && (interp->current != interp->caller))
			local = szl_get_byhash(interp, interp->caller, hash);

		/* finally, try the global frame */
		if ((!local) &&
			(interp->global != interp->caller) &&
			(interp->global != interp->current))
			local = szl_get_byhash(interp, interp->global, hash);
	}

	if (!local) {
		szl_set_result_fmt(interp, out, "bad obj: %s", name);
		return SZL_ERR;
	}

	szl_obj_ref(local->obj);
	*out = NULL;
	szl_set_result(out, local->obj);
	return SZL_OK;
}

SZL_STATIC
enum szl_res szl_local_by_hash(struct szl_interp *interp,
                               const szl_hash hash,
                               struct szl_obj *obj,
                               struct szl_obj *proc)
{
	struct szl_local *old;
	struct szl_local **locals;
	size_t nlocals = proc->nlocals + 1;

	/* if a previous object already exists with the same name, override its
	 * value */
	old = szl_get_byhash(interp, proc, hash);
	if (old) {
		szl_obj_unref(old->obj);
		old->obj = szl_obj_ref(obj);
		return SZL_OK;
	}

	/* otherwise, add a new object */
	locals = (struct szl_local **)realloc(proc->locals,
	                                      sizeof(struct szl_local *) * nlocals);
	if (!locals)
		return SZL_ERR;

	proc->locals = locals;

	locals[proc->nlocals] = (struct szl_local *)malloc(
	                                                  sizeof(struct szl_local));
	if (!locals[proc->nlocals])
		return SZL_ERR;

	locals[proc->nlocals]->name = hash;
	locals[proc->nlocals]->obj = szl_obj_ref(obj);
	proc->nlocals = nlocals;

	return SZL_OK;
}

enum szl_res szl_local(struct szl_interp *interp,
                       struct szl_obj *proc,
                       const char *name,
                       struct szl_obj *obj)
{
	return szl_local_by_hash(interp, szl_hash_name(interp, name), obj, proc);
}

SZL_STATIC
enum szl_res szl_copy_locals(struct szl_interp *interp,
                             struct szl_obj *caller,
                             struct szl_obj *current)
{
	size_t i;
	enum szl_res res;

	/* we never inherit globals, so all procedures share the same object
	 * values */
	if (caller != interp->global) {
		for (i = 0; i < caller->nlocals; ++i) {
			res = szl_local_by_hash(interp,
			                        caller->locals[i]->name,
			                        caller->locals[i]->obj,
			                        current);
			if (res != SZL_OK)
				return res;
		}
	}

	return SZL_OK;
}

SZL_STATIC
enum szl_res szl_call(struct szl_interp *interp,
                      const int objc,
                      struct szl_obj **objv,
                      struct szl_obj **ret)
{
	const char *s;
	enum szl_res res;

	szl_unset(ret);

	if (!(objv[0]->type & SZL_TYPE_PROC)) {
		s = szl_obj_str(objv[0], NULL);
		if (s)
			szl_set_result_fmt(interp, ret, "not a proc: %s", s);
		else
			szl_set_result_str(interp, ret, "not a proc");
		return SZL_ERR;
	}

	if ((objc <= 0) ||
	    (objc >= SZL_MAX_PROC_OBJC) ||
	    ((objv[0]->min_argc != -1) && (objc < objv[0]->min_argc)) ||
	    ((objv[0]->max_argc != -1) && (objc > objv[0]->max_argc))) {
		if (objv[0]->help)
			szl_usage(interp, ret, objv[0]);
		else
			szl_empty_result(interp, ret);

		return SZL_ERR;
	}

	res = objv[0]->proc(interp, objc, objv, ret);

	/* if no return value is set, return the empty object */
	if (!*ret)
		szl_empty_result(interp, ret);

	return res;
}

SZL_STATIC
int szl_isspace(const char ch)
{
	return ((ch == ' ') || (ch == '\t') || (ch == '\n') || (ch == '\r'));
}

SZL_STATIC
void szl_ltrim(char *s, char **start)
{
	for (*start = s; szl_isspace(*start[0]); ++*start);
}

SZL_STATIC
void szl_trim(char *s, char **start, char **end)
{
	size_t len;

	if (!s[0]) {
		*start = *end = s;
		return;
	}

	len = 1 + strlen(s + 1);
	if (len == 1)
		*end = &s[1];
	else {
		*end = &s[len - 1];
		if (szl_isspace(**end)) {
			for (--*end; (*end > s) && szl_isspace(**end); --*end);
			++*end;
			**end = '\0';
		} else
			*end = &s[len];
	}

	szl_ltrim(s, start);
}

SZL_STATIC
struct szl_obj *szl_expand(struct szl_interp *interp, const char *s, int len)
{
	struct szl_obj *tmp = NULL, *obj;
	char **toks, *s2;
	const char *tok;
	size_t tlen;
	int ntoks, i;

	s2 = strndup(s, len);
	if (!s2)
		return NULL;

	toks = szl_split(interp, s2, &ntoks, &tmp);
	if (tmp)
		szl_obj_unref(tmp);
	if (!toks) {
		free(s2);
		return NULL;
	}

	obj = szl_new_str("", 0);
	if (!obj) {
		free(toks);
		free(s2);
		return NULL;
	}

	for (i = 0; i < ntoks; ++i) {
		tmp = NULL;
		if (szl_eval(interp, &tmp, toks[i]) != SZL_OK) {
			if (tmp)
				szl_obj_unref(tmp);
			szl_obj_unref(obj);
			free(toks);
			free(s2);
			return NULL;
		}

		tok = szl_obj_str(tmp, &tlen);
		if (!tok) {
			szl_obj_unref(tmp);
			szl_obj_unref(obj);
			free(toks);
			free(s2);
			return NULL;
		}

		if (szl_append(obj, tok, tlen) != SZL_OK) {
			szl_obj_unref(tmp);
			szl_obj_unref(obj);
			free(toks);
			free(s2);
			return NULL;
		}

		szl_obj_unref(tmp);
	}

	free(toks);
	free(s2);
	return obj;
}

enum szl_res szl_eval(struct szl_interp *interp,
                      struct szl_obj **out,
                      const char *s)
{
	enum szl_res res = SZL_OK;
	char *s2, *start, *end;
	size_t len;

	if (!s[0]) {
		*out = szl_empty(interp);
		return SZL_OK;
	}

	*out = NULL;
	len = strlen(s);

	s2 = strndup(s, len);
	if (!s2)
		return SZL_ERR;

	/* locate the expression, by skipping all trailing and leading whitespace */
	szl_trim(s2, &start, &end);

	/* if the expression is wrapped with {}, leave it as-is */
	if ((start[0] == '{') && (*(end - 1) == '}')) {
		*(end - 1) = '\0';
		szl_set_result(out, szl_new_str(start + 1, end - start - 2));
	}

	/* if the expression is wrapped with [], it's a procedure call - call it and
	 * pass the return value */
	else if ((start[0] == '[') && (*(end - 1) == ']')) {
		*(end - 1) = '\0';
		res = szl_run_line(interp, start + 1, end - start - 2, out);
	}

	/* if the expression starts with ${ and ends with }, the middle is a
	 * variable name */
	else if ((len > 3) &&
	         (start[0] == '$') &&
	         (start[1] == '{') &&
	         (*(end - 1) == '}')) {
		*(end - 1) = '\0';
		res = szl_get(interp, out, start + 2);
	}

	/* if the expression starts with $, the rest is a variable name */
	else if (start[0] == '$')
		res = szl_get(interp, out, start + 1);

	/* if the expression is wrapped with quotes, treat everything between them
	 * as a string literal */
	else if ((start[0] == '"') && (*(end - 1) == '"')) {
		*(end - 1) = '\0';
		szl_set_result(out, szl_expand(interp, start + 1, end - start));
	}

	/* otherwise, treat it as an unquoted string literal */
	else
		szl_set_result(out, szl_new_str(start, end - start));

	free(s2);

	if (!*out)
		return SZL_ERR;

	return res;
}

SZL_STATIC
char *szl_get_next_token(struct szl_interp *interp,
                         struct szl_obj **err,
                         char *s)
{
	char *pos;
	char odelim, cdelim = '\0';
	int nodelim = 0, ncdelim = 0;

	if (s[0] == '\0')
		return NULL;

	if (s[0] == '[')
		cdelim = ']';
	else if (s[0] == '{')
		cdelim = '}';

	if (cdelim != '\0') {
		odelim = s[0];

		for (pos = s; pos[0] != '\0'; ++pos) {
			if (pos[0] == odelim)
				++nodelim;
			else if (pos[0] == cdelim) {
				++ncdelim;
				if (nodelim == ncdelim) {
					for (pos += 1; szl_isspace(pos[0]); ++pos);
					if (pos[0] != '\0') {
						*(pos - 1) = '\0';
						return pos;
					}
					return NULL;
				}
			}
		}

		szl_set_result_fmt(interp,
		                   err,
		                   "unbalanced %c%c: %s",
		                   odelim,
		                   cdelim,
		                   s);
		return NULL;
	}

	if (s[0] == '"') {
		for (pos = s; pos[0] != '\0'; ++pos) {
			if ((pos[0] == '"') && szl_isspace(pos[1])) {
				++nodelim;
				if (nodelim % 2 == 0) {
					for (pos += 1; szl_isspace(pos[0]); ++pos);
					if (pos[0] != '\0') {
						*(pos - 1) = '\0';
						return pos;
					}
					return NULL;
				}
			}
		}

		szl_set_result_fmt(interp, err, "unbalanced quotes: %s", s);
		return NULL;
	}

	if (szl_isspace(s[0])) {
		for (pos = s; szl_isspace(pos[0]); ++pos);
		if (pos[0] != '\0') {
			/* terminate the previous token */
			s[0] = '\0';

			/* terminate the current token */
			*(pos - 1) = '\0';
			return pos;
		}
	}
	else {
		/* otherwise, skip all leading non-whitespace characters and look for
		 * the next token after them */
		pos = s + 1;
		do {
			if (szl_isspace(pos[0]))
				return szl_get_next_token(interp, err, pos);
			++pos;
		} while (pos[0] != '\0');
	}

	/* expression ends with whitespace */
	return NULL;
}

char **szl_split(struct szl_interp *interp,
                 char *s,
                 int *argc,
                 struct szl_obj **out)
{
	char *tok,  *next, **argv = NULL, **margv, *start, *end;
	int margc = 1;

	if (s[0] == '\0') {
		szl_set_result_str(interp, out, "syntax error: empty expression");
		return NULL;
	}

	szl_trim(s, &start, &end);

	next = szl_get_next_token(interp, out, start);
	tok = start;

	*argc = 0;
	do {
		margv = (char **)realloc(argv, sizeof(char *) * margc);
		if (!margv) {
			if (argv)
				free(argv);
			return NULL;
		}

		margv[*argc] = tok;

		*argc = margc;
		argv = margv;

		if (!next)
			break;

		tok = next;
		++margc;
		szl_unset(out);
		next = szl_get_next_token(interp, out, tok);
	} while (1);

	return argv;
}

SZL_STATIC
enum szl_res szl_run_line(struct szl_interp *interp,
                          char *s,
                          size_t len,
                          struct szl_obj **out)
{
	struct szl_obj **objv, *prev_caller = szl_obj_ref(interp->caller);
	char **argv;
	enum szl_res res;
	int argc, i, j;

	/* make sure the line is terminated */
	s[len] = '\0';

	*out = NULL;

	/* split the command arguments */
	argv = szl_split(interp, s, &argc, out);
	if (!argv)
		return SZL_ERR;

	objv = (struct szl_obj **)malloc(sizeof(struct szl_obj *) * argc);
	if (!objv) {
		szl_unset(out);
		free(argv);
		return SZL_ERR;
	}

	/* the first token is the procedure - try to find a variable named that way
	 * and if it fails evaluate this token */
	objv[0] = NULL;
	res = szl_get(interp, &objv[0], argv[0]);
	if (res != SZL_OK) {
		szl_unset(&objv[0]);
		szl_set_result_fmt(interp, out, "bad proc: %s", argv[0]);
		res = szl_eval(interp, &objv[0], argv[0]);
	}
	if (res != SZL_OK) {
		if (objv[0])
			szl_set_result(out, objv[0]);
		free(objv);
		free(argv);
		return res;
	}

	res = szl_copy_locals(interp, interp->current, objv[0]);
	if (res != SZL_OK) {
		szl_obj_unref(objv[0]);
		free(objv);
		free(argv);
		return res;
	}

	/* set the current function, so evaluation of refers to the right frame */
	interp->caller = szl_obj_ref(interp->current);
	interp->current = objv[0];

	/* evaluate all arguments */
	for (i = 1; i < argc; ++i) {
		if (szl_eval(interp, &objv[i], argv[i]) != SZL_OK) {
			if (objv[i])
				szl_set_result(out, objv[i]);
			else
				szl_set_result_fmt(interp, out, "bad arg: %s", argv[i]);

			szl_free_locals(objv[0]);

			for (j = 0; j < i; ++j)
				szl_obj_unref(objv[j]);

			interp->current = interp->caller;
			szl_obj_unref(interp->caller);
			interp->caller = prev_caller;
			szl_obj_unref(prev_caller);

			free(objv);
			free(argv);
			return SZL_ERR;
		}
	}

	/* call objv[0] */
	res = szl_call(interp, argc, objv, out);

	/* free all locals */
	szl_free_locals(objv[0]);

	interp->current = interp->caller;
	szl_obj_unref(interp->caller);
	interp->caller = prev_caller;
	szl_obj_unref(prev_caller);

	for (i = 0; i < argc; ++i)
		szl_obj_unref(objv[i]);
	free(objv);
	free(argv);

	/* set the special SZL_PREV_RET_OBJ_NAME variable so it points to the
	 * procedure return value */
	if (res == SZL_OK) {
		if (*out)
			res = szl_local(interp,
			                interp->caller,
			                SZL_PREV_RET_OBJ_NAME,
			                *out);
		else {
			*out = szl_empty(interp);
			res = szl_local(interp,
			                interp->caller,
			                SZL_PREV_RET_OBJ_NAME, *out);
			szl_obj_unref(*out);
			*out = NULL;
		}
	}

	return res;
}

SZL_STATIC
enum szl_res szl_run_lines(struct szl_interp *interp,
                           char **s,
                           const size_t n,
                           struct szl_obj **out)
{
	size_t i;
	enum szl_res res;

	/* run all lines except the last one */
	*out = NULL;
	for (i = 0; i < n - 1; ++i) {
		res = szl_run_line(interp, s[i], strlen(s[i]), out);
		if (res != SZL_OK)
			return res;

		/* if this isn't the last line, get rid of the previous one's return
		 * value */
		szl_unset(out);
	}

	*out = NULL;
	return szl_run_line(interp, s[n - 1], strlen(s[n - 1]), out);
}

SZL_STATIC
char **szl_split_lines(struct szl_interp *interp,
                       struct szl_obj **err,
                       char *s,
                       const size_t len,
                       size_t *n)
{
	char **lines = NULL, **mlines, *line = s, *next = NULL;
	const char *fmt = NULL;
	size_t i = 0;
	int mn, nbraces = 0, nbrackets = 0, nquotes = 0;

	*n = 0;
	while (i < len) {
		if (s[i] == '{')
			++nbraces;
		else if (s[i] == '}')
			--nbraces;
		else if (s[i] == '[')
			++nbrackets;
		else if (s[i] == ']')
			--nbrackets;
		else if (s[i] == '"')
			++nquotes;

		if (i == len - 1) {
			if (nbraces != 0)
				fmt = "unbalanced {}: %s\n";
			else if (nbrackets != 0)
				fmt = "unbalanced []: %s\n";
			else if (nquotes % 2 != 0)
				fmt = "unbalanced quotes: %s\n";

			if (fmt) {
				szl_set_result_fmt(interp, err, fmt, s);
				if (lines)
					free(lines);
				return NULL;
			}
		}
		else if ((s[i] != '\n') ||
		         (nbraces != 0) ||
		         (nbrackets != 0) ||
		         (nquotes % 2 != 0))
			goto nexti;

		if (s[i] == '\n')
			s[i] = '\0';
		next = &s[i + 1];

		/* if the line contains nothing but whitespace, skip it */
		for (; szl_isspace(*line); ++line);
		if (line[0] != '\0' && line[0] != SZL_COMMENT_PREFIX) {
			mn = *n + 1;
			mlines = (char **)realloc(lines, sizeof(char *) * mn);
			if (!mlines) {
				if (lines)
					free(lines);
				return NULL;
			}

			mlines[*n] = line;

			lines = mlines;
			*n = mn;
		}

		line = next;
		nquotes = 0;

nexti:
		++i;
	}

	return lines;
}

enum szl_res szl_run(struct szl_interp *interp,
                     struct szl_obj **out,
                     char *s,
                     const size_t len)
{
	char **lines;
	size_t n;
	enum szl_res res;

	*out = NULL;

	lines = szl_split_lines(interp, out, s, len, &n);
	if (!lines)
		return SZL_ERR;

	res = szl_run_lines(interp, lines, n, out);

	free(lines);
	return res;
}

enum szl_res szl_run_const(struct szl_interp *interp,
                           struct szl_obj **out,
                           const char *s,
                           const size_t len)
{
	char *s2;
	enum szl_res res;

	s2 = strndup(s, len);
	if (!s2)
		return SZL_ERR;

	res = szl_run(interp, out, s2, len);
	free(s2);
	return res;
}

enum szl_res szl_load(struct szl_interp *interp, const char *name)
{
	char path[PATH_MAX];
	char init_name[SZL_MAX_EXT_INIT_FUNC_NAME_LEN + 1];
	struct szl_ext *exts;
	void *handle;
	szl_ext_init init;
	unsigned int nexts = interp->nexts + 1;
	enum szl_res res;

	snprintf(path, sizeof(path), SZL_EXT_PATH_FMT, name);
	handle = dlopen(path, RTLD_LAZY);
	if (!handle)
		return SZL_ERR;

	snprintf(init_name, sizeof(init_name), SZL_EXT_INIT_FUNC_NAME_FMT, name);
	init = (szl_ext_init)dlsym(handle, init_name);
	if (!init) {
		dlclose(handle);
		return SZL_ERR;
	}

	exts = (struct szl_ext *)realloc(interp->exts,
	                                 sizeof(struct szl_ext) * nexts);
	if (!exts) {
		dlclose(handle);
		return SZL_ERR;
	}

	exts[interp->nexts].handle = handle;
	exts[interp->nexts].objs = 0;
	exts[interp->nexts].nobjs = 0;

	interp->exts = exts;
	interp->nexts = nexts;

	res = init(interp);
	if (res != SZL_OK) {
		dlclose(handle);
		interp->exts[interp->nexts - 1].handle = NULL;
		return SZL_ERR;
	}

	return SZL_OK;
}

void szl_unload(void *h)
{
	dlclose(h);
}

enum szl_res szl_source(struct szl_interp *interp,
                        struct szl_obj **out,
                        const char *path)
{
	struct stat stbuf;
	char *buf;
	size_t len;
	int fd, res;

	*out = NULL;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return SZL_ERR;

	if (fstat(fd, &stbuf) < 0) {
		close(fd);
		return SZL_ERR;
	}

	buf = (char *)malloc((size_t)stbuf.st_size + 1);
	if (!buf) {
		close(fd);
		return SZL_ERR;
	}

	len = read(fd, buf, (size_t)stbuf.st_size);
	if (len <= 0) {
		free(buf);
		close(fd);
		return SZL_ERR;
	}
	buf[len] = '\0';

	close(fd);

	res = szl_run(interp, out, buf, (size_t)stbuf.st_size);
	free(buf);
	return res;
}

enum szl_res szl_main(int argc, char *argv[])
{
	struct szl_obj *out = NULL;
	struct szl_interp *interp;
	const char *s;
	size_t len;
	enum szl_res res;

	interp = szl_interp_new();
	if (!interp)
		return SZL_ERR;

	switch (argc) {
		case 2:
			res = szl_source(interp, &out, argv[1]);
			break;

		case 3:
			if (strcmp(argv[1], "-c") == 0) {
				res = szl_run(interp, &out, argv[2], strlen(argv[2]));
				break;
			}
			/* fall through */

		default:
			res = SZL_ERR;
			break;
	}

	if (out) {
		if (res != SZL_OK) {
			s = szl_obj_str(out, &len);
			if (s && len)
				fwrite(s, 1, len, stderr);
		}
		szl_obj_unref(out);
	}

	szl_interp_free(interp);
	return res;
}
