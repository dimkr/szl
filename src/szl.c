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
#include <time.h>
#include <dlfcn.h>

#include "szl.h"

extern char *szl_builtin_exts[];
extern enum szl_res szl_init_builtin_exts(struct szl_interp *interp);

static
enum szl_res szl_run_line(struct szl_interp *, struct szl_obj *);

static
struct szl_local *szl_get_in_proc(struct szl_interp *interp,
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

static
int szl_local_by_hash(struct szl_interp *interp,
                      const szl_hash hash,
                      struct szl_obj *obj,
                      struct szl_obj *proc)
{
	struct szl_local *old;
	struct szl_local **locals;
	size_t nlocals = proc->nlocals + 1;

	/* if a previous object already exists with the same name, override its
	 * value */
	old = szl_get_in_proc(interp, proc, hash);
	if (old) {
		szl_obj_unref(old->obj);
		old->obj = szl_obj_ref(obj);
		return 1;
	}

	/* otherwise, add a new object */
	locals = (struct szl_local **)realloc(proc->locals,
	                                      sizeof(struct szl_local *) * nlocals);
	if (!locals)
		return 0;

	proc->locals = locals;

	locals[proc->nlocals] = (struct szl_local *)malloc(
	                                                  sizeof(struct szl_local));
	if (!locals[proc->nlocals])
		return 0;

	locals[proc->nlocals]->name = hash;
	locals[proc->nlocals]->obj = szl_obj_ref(obj);
	proc->nlocals = nlocals;

	return 1;
}

static
int szl_copy_locals(struct szl_interp *interp,
                    struct szl_obj *caller,
                    struct szl_obj *current)
{
	size_t i;

	/* we never inherit globals, so all procedures share the same object
	 * values */
	if (caller != interp->global) {
		for (i = 0; i < caller->nlocals; ++i) {
			if (!szl_local_by_hash(interp,
			                       caller->locals[i]->name,
			                       caller->locals[i]->obj,
			                       current))
				return 0;
		}
	}

	return 1;
}

static
struct szl_obj *szl_new_call(struct szl_interp *interp,
                             struct szl_obj *caller,
                             const char *exp,
                             const size_t len,
                             const int copy_locals)
{
	struct szl_obj *obj;

	obj = szl_new_str(exp, len);
	if (!obj)
		return obj;

	if (copy_locals && !szl_copy_locals(interp, interp->current, obj)) {
		szl_obj_unref(obj);
		return NULL;
	}

	if (caller)
		obj->caller = szl_obj_ref(caller);

	obj->type |= SZL_TYPE_CALL;
	return obj;
}

struct szl_interp *szl_interp_new(void)
{
	struct szl_interp *interp;

	interp = (struct szl_interp *)malloc(sizeof(*interp));
	if (!interp)
		return NULL;

	interp->init = crc32(0, Z_NULL, 0);

	interp->empty = szl_new_empty();
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

	interp->current = szl_new_call(interp, NULL, NULL, 0, 0);
	if (!interp->current) {
		szl_obj_unref(interp->global);
		szl_obj_unref(interp->one);
		szl_obj_unref(interp->zero);
		szl_obj_unref(interp->space);
		szl_obj_unref(interp->empty);
		free(interp);
		return NULL;
	}
	interp->current->caller = NULL;

	interp->exts = NULL;
	interp->nexts = 0;

	interp->null = NULL;
	interp->last = szl_empty(interp);
	interp->level = 0;

	if (szl_init_builtin_exts(interp) != SZL_OK) {
		szl_interp_free(interp);
		return NULL;
	}

	return interp;
}

static
void szl_unload_ext(struct szl_ext *ext)
{
	unsigned int i;

	if (ext->handle) {
		szl_unload(ext->handle);
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

	szl_obj_unref(interp->last);
	szl_obj_unref(interp->global);
	if (interp->null)
		szl_obj_unref(interp->null);
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

void szl_obj_unref(struct szl_obj *obj)
{
	size_t i;

	if (--obj->refc == 0) {
		if (obj->nlocals) {
			for (i = 0; i < obj->nlocals; ++i) {
				szl_obj_unref(obj->locals[i]->obj);
				free(obj->locals[i]);
			}

			free(obj->locals);
		}

		if (obj->del)
			obj->del(obj->priv);

		if (obj->s)
			free(obj->s);

		if (obj->l) {
			for (i = 0; i < obj->n; ++i)
				szl_obj_unref(obj->l[i]);
			free(obj->l);
		}

		if (obj->caller)
			szl_obj_unref(obj->caller);

		free(obj);
	}
}

void szl_new_obj_name(struct szl_interp *interp,
                      const char *pfix,
                      char *buf,
                      const size_t len,
                      const void *priv)
{
	snprintf(buf, len, "%s:"SZL_INT_FMT, pfix, (szl_int)(intptr_t)priv);
}

struct szl_obj *szl_new_str_noalloc(char *s, const size_t len)
{
	struct szl_obj *obj;

	obj = (struct szl_obj *)malloc(sizeof(*obj));
	if (!obj)
		return NULL;

	obj->s = s;
	obj->l = NULL;
	obj->len = len;
	obj->type = SZL_TYPE_STR;
	obj->proc = NULL;
	obj->del = NULL;
	obj->locals = NULL;
	obj->nlocals = 0;
	obj->caller = NULL;
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
	memcpy(s2, s, rlen);
	s2[rlen] = '\0';

	obj = szl_new_str_noalloc(s2, rlen);
	if (!obj)
		free(s2);

	return obj;
}

struct szl_obj *szl_new_empty(void)
{
	return szl_new_str("", 0);
}

struct szl_obj *szl_new_int(const szl_int i)
{
	struct szl_obj *obj;

	obj = (struct szl_obj *)malloc(sizeof(*obj));
	if (!obj)
		return NULL;

	obj->s = NULL;
	obj->l = NULL;
	obj->i = i;
	obj->type = SZL_TYPE_INT;
	obj->proc = NULL;
	obj->del = NULL;
	obj->locals = NULL;
	obj->nlocals = 0;
	obj->caller = NULL;
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
	obj->l = NULL;
	obj->d = d;
	obj->type = SZL_TYPE_DOUBLE;
	obj->proc = NULL;
	obj->del = NULL;
	obj->locals = NULL;
	obj->nlocals = 0;
	obj->caller = NULL;
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

	obj->len = strlen(name);
	obj->s = strndup(name, obj->len);
	if (!obj->s) {
		free(obj);
		return NULL;
	}

	obj->refc = 0;
	if (name[0] && (!szl_local(interp, interp->global, name, obj))) {
		if (del)
			del(priv);
		free(obj->s);
		free(obj);
		return NULL;
	}

	obj->l = NULL;
	obj->type = SZL_TYPE_STR | SZL_TYPE_PROC;
	obj->min_argc = min_argc;
	obj->max_argc = max_argc;
	obj->help = help;
	obj->proc = proc;
	obj->del = del;
	obj->priv = priv;
	obj->locals = NULL;
	obj->nlocals = 0;
	obj->caller = NULL;

	return obj;
}

int szl_new_const_int(struct szl_interp *interp,
                      const char *name,
                      const szl_int val)
{
	struct szl_obj *obj;
	int ret;

	obj = szl_new_int(val);
	if (!obj)
		return 0;

	ret = szl_local(interp, interp->global, name, obj);
	szl_obj_unref(obj);
	return ret;
}

int szl_new_const_str(struct szl_interp *interp,
                      const char *name,
                      const char *val,
                      const int len)
{
	struct szl_obj *obj;
	int ret;

	obj = szl_new_str(val, len);
	if (!obj)
		return 0;

	ret = szl_local(interp, interp->global, name, obj);
	szl_obj_unref(obj);
	return ret;
}

int szl_append(struct szl_interp *interp,
               struct szl_obj *str,
               const char *buf,
               const size_t len)
{
	char *s;
	size_t i, nlen = str->len + len;

	if (nlen >= INT_MAX) {
		szl_set_result_str(interp, "reached the str length limit", -1);
		return 0;
	}

	if (!szl_obj_str(interp, str, NULL))
		return 0;

	s = realloc(str->s, nlen + 1);
	if (!s)
		return 0;

	memcpy(s + str->len, buf, (size_t)len);
	s[nlen] = '\0';
	str->s = s;
	str->len = nlen;

	/* mark all additional representations as expired */
	if (str->l) {
		for (i = 0; i < str->n; ++i)
			szl_obj_unref(str->l[i]);
		free(str->l);
		str->l = NULL;
	}

	str->type &= ~(SZL_TYPE_INT |
	               SZL_TYPE_DOUBLE |
	               SZL_TYPE_BOOL |
	               SZL_TYPE_LIST |
	               SZL_TYPE_HASH);

	return 1;
}

int szl_lappend(struct szl_interp *interp,
                struct szl_obj *list,
                struct szl_obj *item)
{
	struct szl_obj **l;
	size_t n;

	if (!szl_obj_list(interp, list, &l, &n))
		return 0;

	if (n >= INT_MAX) {
		szl_set_result_str(interp, "reached the list length limit", -1);
		return 0;
	}

	++n;
	l = realloc(l, sizeof(struct szl_obj *) * n);
	if (!l)
		return 0;
	l[list->n] = szl_obj_ref(item);

	list->l = l;
	list->n = n;

	/* mark other representations as expired */
	if (list->s) {
		free(list->s);
		list->s = NULL;
	}
	list->type = SZL_TYPE_LIST;

	return 1;
}

int szl_lappend_str(struct szl_interp *interp,
                    struct szl_obj *list,
                    const char *s,
                    const int len)
{
	struct szl_obj *item;
	int ret;

	item = szl_new_str(s, len);
	if (!item)
		return 0;

	ret = szl_lappend(interp, list, item);
	szl_obj_unref(item);
	return ret;
}

int szl_lappend_int(struct szl_interp *interp,
                    struct szl_obj *list,
                    const szl_int i)
{
	struct szl_obj *item;
	int ret;

	item = szl_new_int(i);
	if (!item)
		return 0;

	ret = szl_lappend(interp, list, item);
	szl_obj_unref(item);
	return ret;
}

struct szl_obj *szl_join(struct szl_interp *interp,
                         struct szl_obj *delim,
                         struct szl_obj **objv,
                         const int objc,
                         const int wrap)
{
	struct szl_obj *obj;
	const char *delims, *s;
	char *ws;
	size_t slen, dlen;
	int i, j, last = objc - 1, mw = 0, wlen;

	delims = szl_obj_str(interp, delim, &dlen);
	if (!delims)
		return NULL;

	obj = szl_new_empty();
	if (!obj)
		return NULL;

	for (i = 0; i < objc; ++i) {
		s = szl_obj_str(interp, objv[i], &slen);
		if (!s) {
			szl_obj_unref(obj);
			return NULL;
		}

		if (wrap) {
			mw = 0;
			if (slen) {
				for (j = 0; j < slen; ++j) {
					if (szl_isspace(s[j])) {
						mw = 1;
						break;
					}
				}
			}
			else
				mw = 1;
		}

		if (mw) {
			wlen = asprintf(&ws, "{%s}", s);
			if (wlen < 0) {
				szl_obj_unref(obj);
				return NULL;
			}

			if (!szl_append(interp, obj, ws, (size_t)wlen)) {
				free(ws);
				szl_obj_unref(obj);
				return NULL;
			}

			free(ws);
		}
		else if (!szl_append(interp, obj, s, slen)) {
			szl_obj_unref(obj);
			return NULL;
		}

		if (dlen && (i != last)) {
			if (!szl_append(interp, obj, delims, dlen)) {
				szl_obj_unref(obj);
				return NULL;
			}
		}
	}

	return obj;
}

char *szl_obj_str(struct szl_interp *interp, struct szl_obj *obj, size_t *len)
{
	struct szl_obj *obj2, *space;
	szl_int i;
	int rlen, j;

	/* cast the object, if needed */
	if (!(obj->type & SZL_TYPE_STR) && !obj->s) {
		if (obj->type & SZL_TYPE_LIST) {
			space = szl_space(interp);
			obj2 = szl_join(interp, space, obj->l, (int)obj->n, 1);
			szl_obj_unref(space);
			if (!obj2)
				return 0;

			/* kinda ugly - we "steal" the string representation of the
			 * result */
			obj->s = obj2->s;
			rlen = obj2->len;

			obj2->s = NULL;
			szl_obj_unref(obj2);
		}
		else if (obj->type & SZL_TYPE_DOUBLE) {
			if (!szl_obj_int(interp, obj, &i))
				return NULL;

			if ((szl_double)i == obj->d)
				rlen = asprintf(&obj->s, SZL_INT_FMT, obj->i);
			else {
				rlen = asprintf(&obj->s, SZL_DOUBLE_FMT, obj->d);
				if (rlen > 0) {
					for (j = rlen - 1; j >= 0; --j) {
						if (obj->s[j] == '0')
							--rlen;
						else
							break;
					}
					obj->s[rlen] = '\0';
				}
			}
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

int szl_obj_int(struct szl_interp *interp, struct szl_obj *obj, szl_int *i)
{
	const char *s;
	size_t len;

	/* cast the object, if needed */
	if (!(obj->type & SZL_TYPE_INT)) {
		if (obj->type & SZL_TYPE_DOUBLE)
			obj->i = (szl_int)round(obj->d);
		else {
			s = szl_obj_str(interp, obj, &len);
			if (!s)
				return 0;

			if (!len || (sscanf(s, SZL_INT_FMT, &obj->i) != 1)) {
				szl_set_result_fmt(interp, "bad int: %s", s);
				return 0;
			}
		}

		obj->type |= SZL_TYPE_INT;
	}

	*i = obj->i;
	return 1;
}

int szl_obj_double(struct szl_interp *interp,
                   struct szl_obj *obj,
                   szl_double *d)
{
	const char *s;
	size_t len;

	/* cast the object, if needed */
	if (!(obj->type & SZL_TYPE_DOUBLE)) {
		if (obj->type & SZL_TYPE_INT)
			obj->d = (szl_double)obj->i;
		else {
			s = szl_obj_str(interp, obj, &len);
			if (!s)
				return 0;

			if (!len || (sscanf(s, SZL_DOUBLE_SCANF_FMT, &obj->d) != 1)) {
				szl_set_result_fmt(interp, "bad float: %s", s);
				return 0;
			}
		}

		obj->type |= SZL_TYPE_DOUBLE;
	}

	*d = obj->d;
	return 1;
}

int szl_obj_list(struct szl_interp *interp,
                 struct szl_obj *obj,
                 struct szl_obj ***objv,
                 size_t *objc)
{
	char *s2, **toks;
	size_t len;
	int ntoks, i, j;

	if (!(obj->type & SZL_TYPE_LIST)) {
		s2 = szl_obj_strdup(interp, obj, &len);
		if (!s2)
			return 0;

		if (!len) {
			free(s2);
			obj->l = NULL;
			obj->n = 0;
		}
		else {
			if (!szl_split(interp, s2, &toks, &ntoks)) {
				free(s2);
				return 0;
			}

			obj->l = (struct szl_obj **)malloc(
			                                  sizeof(struct szl_obj *) * ntoks);
			if (!obj->l) {
				free(toks);
				free(s2);
				return 0;
			}

			for (i = 0; i < ntoks; ++i) {
				obj->l[i] = szl_new_str(toks[i], strlen(toks[i]));
				if (!obj->l[i]) {
					for (j = 0; j < i; ++j)
						szl_obj_unref(obj->l[j]);
					free(toks);
					free(s2);
					return 0;
				}
			}

			free(toks);
			free(s2);

			obj->n = (size_t)ntoks;
		}

		obj->type |= SZL_TYPE_LIST;
	}

	*objc = obj->n;
	if (objv)
		*objv = obj->l;
	return 1;
}

int szl_obj_hash(struct szl_interp *interp,
                 struct szl_obj *obj,
                 szl_hash *hash)
{
	const char *s;
	size_t len;

	if (!(obj->type & SZL_TYPE_HASH)) {
		s = szl_obj_str(interp, obj, &len);
		if (!s)
			return 0;

		obj->hash = crc32(interp->init, (const Bytef *)s, (uInt)len);
		obj->type |= SZL_TYPE_HASH;
	}

	*hash = obj->hash;
	return 1;
}

char *szl_obj_strdup(struct szl_interp *interp,
                     struct szl_obj *obj,
                     size_t *len)
{
	char *s;

	s = szl_obj_str(interp, obj, len);
	if (s)
		return strndup(obj->s, obj->len);

	return NULL;
}

int szl_obj_len(struct szl_interp *interp, struct szl_obj *obj, size_t *len)
{
	const char *s;

	s = szl_obj_str(interp, obj, len);
	if (!s)
		return 0;

	return 1;
}

int szl_obj_istrue(struct szl_obj *obj)
{
	if (!(obj->type & SZL_TYPE_BOOL)) {
		obj->b = (((obj->type & SZL_TYPE_INT) && obj->i) ||
		          ((obj->type & SZL_TYPE_DOUBLE) && obj->d) ||
		          ((obj->type & SZL_TYPE_LIST) && obj->n) ||
		          ((obj->type & SZL_TYPE_STR) &&
		            obj->s[0] &&
		           (strcmp("0", obj->s) != 0)));
		obj->type |= SZL_TYPE_BOOL;
	}

	return (int)obj->b;
}

int szl_obj_eq(struct szl_interp *interp,
               struct szl_obj *a,
               struct szl_obj *b,
               int *eq)
{
	const char *as, *bs;
	size_t alen, blen;
	szl_hash ah, bh;

	/* optimization: could be the same object */
	if (a == b) {
		*eq = 1;
		return 1;
	}

	if ((a->type & SZL_TYPE_INT) && (b->type & SZL_TYPE_INT)) {
		*eq = (a->i == b->i);
		return 1;
	}

	if ((a->type & SZL_TYPE_DOUBLE) && (b->type & SZL_TYPE_DOUBLE)) {
		*eq = (a->d == b->d);
		return 1;
	}

	as = szl_obj_str(interp, a, &alen);
	if (!as)
		return 0;

	bs = szl_obj_str(interp, b, &blen);
	if (!bs)
		return 0;

	*eq = 0;
	if (alen == blen) {
		if (!szl_obj_hash(interp, a, &ah) || !szl_obj_hash(interp, b, &bh))
			return 0;

		if (ah == bh)
			*eq = 1;
	}

	return 1;
}

enum szl_res szl_set_result(struct szl_interp *interp, struct szl_obj *obj)
{
	szl_obj_unref(interp->last);
	interp->last = obj;

	return SZL_OK;
}

void szl_empty_result(struct szl_interp *interp)
{
	szl_set_result(interp, szl_empty(interp));
}

enum szl_res szl_set_result_str(struct szl_interp *interp,
                                const char *s,
                                const int len)
{
	struct szl_obj *obj;

	obj = szl_new_str(s, len);
	if (!obj)
		return SZL_ERR;

	return szl_set_result(interp, obj);
}

enum szl_res szl_set_result_int(struct szl_interp *interp, const szl_int i)
{
	struct szl_obj *obj;

	obj = szl_new_int(i);
	if (!obj)
		return SZL_ERR;

	return szl_set_result(interp, obj);
}

enum szl_res szl_set_result_double(struct szl_interp *interp,
                                   const szl_double d)
{
	struct szl_obj *obj;

	obj = szl_new_double(d);
	if (!obj)
		return SZL_ERR;

	return szl_set_result(interp, obj);
}

enum szl_res szl_set_result_bool(struct szl_interp *interp, const szl_bool b)
{
	if (b)
		return szl_set_result(interp, szl_true(interp));

	return szl_set_result(interp, szl_false(interp));
}

enum szl_res szl_set_result_fmt(struct szl_interp *interp,
                                const char *fmt,
                                ...)
{
	struct szl_obj *obj;
	va_list ap;
	char *s;
	int len;

	va_start(ap, fmt);
	len = vasprintf(&s, fmt, ap);
	va_end(ap);

	if (len >= 0) {
		obj = szl_new_str_noalloc(s, len);
		if (obj)
			return szl_set_result(interp, obj);

		free(s);
	}

	return SZL_ERR;
}

enum szl_res szl_set_result_list(struct szl_interp *interp,
                                 struct szl_obj **items,
                                 const size_t n)
{
	struct szl_obj *list;
	size_t i;

	list = szl_new_empty();
	if (!list)
		return SZL_ERR;

	for (i = 0; i < n; ++i) {
		if (!szl_lappend(interp, list, items[i])) {
			szl_obj_unref(list);
			return SZL_ERR;
		}
	}

	return szl_set_result(interp, list);
}

enum szl_res szl_usage(struct szl_interp *interp, struct szl_obj *proc)
{
	const char *name, *exp;

	name = szl_obj_str(interp, proc, NULL);
	if (name) {
		exp = szl_obj_str(interp, interp->current, NULL);

		if (proc->help) {
			if (exp)
				szl_set_result_fmt(interp,
				                   "bad %s call: '%s', should be '%s'",
				                   name,
				                   exp,
				                   proc->help);
			else
				szl_set_result_fmt(interp,
				                   "bad %s call, should be '%s'",
				                   name,
				                   proc->help);
		}
		else if (exp)
			szl_set_result_fmt(interp, "bad %s call: '%s'", name, exp);
	}

	return SZL_ERR;
}

static
szl_hash szl_hash_name(struct szl_interp *interp, const char *name)
{
	return crc32(interp->init, (const Bytef *)name, (uInt)strlen(name));
}

struct szl_obj *szl_get_byhash(struct szl_interp *interp, const szl_hash hash)
{
	struct szl_local *local = NULL;

	/* try the current frame's locals first */
	local = szl_get_in_proc(interp, interp->current, hash);

	/* then, fall back to the caller's */
	if (!local &&
	   interp->current->caller &&
	   (interp->current != interp->current->caller))
		local = szl_get_in_proc(interp, interp->current->caller, hash);

	/* finally, try the global frame */
	if ((!local) &&
		(interp->global != interp->current->caller) &&
		(interp->global != interp->current))
		local = szl_get_in_proc(interp, interp->global, hash);

	if (local)
		return szl_obj_ref(local->obj);

	return NULL;
}

struct szl_obj *szl_get_byname(struct szl_interp *interp, const char *name)
{
	struct szl_obj *local;
	if (name[0]) {
		local = szl_get_byhash(interp, szl_hash_name(interp, name));
		if (local)
			return local;
	}

	szl_set_result_fmt(interp, "bad obj: %s", name);
	return NULL;
}

struct szl_obj *szl_get(struct szl_interp *interp, struct szl_obj *name)
{
	const char *s;
	struct szl_obj *local;
	szl_hash hash;

	s = szl_obj_str(interp, name, NULL);
	if (!s)
		return NULL;

	if (!szl_obj_hash(interp, name, &hash))
		return NULL;

	local = szl_get_byhash(interp, szl_hash_name(interp, s));
	if (!local)
		szl_set_result_fmt(interp, "bad obj: %s", s);

	return local;
}

int szl_local(struct szl_interp *interp,
              struct szl_obj *proc,
              const char *name,
              struct szl_obj *obj)
{
	return szl_local_by_hash(interp, szl_hash_name(interp, name), obj, proc);
}

static
enum szl_res szl_call(struct szl_interp *interp,
                      const int objc,
                      struct szl_obj **objv)
{
	const char *s;

	if (!(objv[0]->type & SZL_TYPE_PROC)) {
		s = szl_obj_str(interp, objv[0], NULL);
		if (s)
			szl_set_result_fmt(interp, "not a proc: %s", s);
		else
			szl_set_result_str(interp, "not a proc", -1);
		return SZL_ERR;
	}

	if ((objc <= 0) ||
	    (objc >= SZL_MAX_PROC_OBJC) ||
	    ((objv[0]->min_argc != -1) && (objc < objv[0]->min_argc)) ||
	    ((objv[0]->max_argc != -1) && (objc > objv[0]->max_argc)))
		return szl_usage(interp, objv[0]);

	return objv[0]->proc(interp, objc, objv);
}

int szl_isspace(const char ch)
{
	return ((ch == ' ') || (ch == '\t') || (ch == '\n') || (ch == '\r'));
}

static
void szl_ltrim(char *s, char **start)
{
	for (*start = s; szl_isspace(*start[0]); ++*start);
}

static
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

enum szl_res szl_eval(struct szl_interp *interp,
                      struct szl_obj **out,
                      const char *s)
{
	struct szl_obj *exp;
	char *s2, *start, *end;
	size_t len;
	enum szl_res res = SZL_OK;

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
		*out = szl_new_str(start + 1, end - start - 2);
		res = SZL_OK;
	}

	/* if the expression is wrapped with [], it's a procedure call - call it and
	 * pass the return value */
	else if ((start[0] == '[') && (*(end - 1) == ']')) {
		*(end - 1) = '\0';
		exp = szl_new_str(start + 1, end - start - 2);
		if (!exp)
			return SZL_ERR;
		res = szl_run_line(interp, exp);
		szl_obj_unref(exp);
		*out = szl_last(interp);
	}

	/* if the expression starts with ${ and ends with }, the middle is a
	 * variable name */
	else if ((len > 3) &&
	         (start[0] == '$') &&
	         (start[1] == '{') &&
	         (*(end - 1) == '}')) {
		*(end - 1) = '\0';
		*out = szl_get_byname(interp, start + 2);
	}

	/* if the expression starts with $, the rest is a variable name */
	else if (start[0] == '$')
		*out = szl_get_byname(interp, start + 1);

	/* otherwise, treat it as a string literal */
	else
		*out = szl_new_str(start, end - start);

	free(s2);

	if (!*out)
		return SZL_ERR;

	return res;
}

static
char *szl_get_next_token(struct szl_interp *interp, char *s)
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

		szl_set_result_fmt(interp, "unbalanced %c%c: %s", odelim, cdelim, s);
		return NULL;
	}

	if (szl_isspace(s[0])) {
		for (pos = s + 1; szl_isspace(pos[0]); ++pos);
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
				return szl_get_next_token(interp, pos);
			if (pos[0] == '\0')
				break;
			++pos;
		} while (1);
	}

	/* expression ends with whitespace */
	return NULL;
}

int szl_split(struct szl_interp *interp, char *s, char ***argv, int *argc)
{
	char *tok,  *next, **margv, *start, *end;
	int margc = 1;

	*argc = 0;
	*argv = NULL;

	if (s[0] == '\0')
		return 0;

	szl_trim(s, &start, &end);

	next = szl_get_next_token(interp, start);
	tok = start;

	do {
		margv = (char **)realloc(*argv, sizeof(char *) * margc);
		if (!margv) {
			if (*argv)
				free(*argv);
			return 0;
		}

		margv[*argc] = tok;

		*argc = margc;
		*argv = margv;

		if (!next)
			break;

		tok = next;
		++margc;
		next = szl_get_next_token(interp, tok);
	} while (1);

	return 1;
}

static
enum szl_res szl_run_line(struct szl_interp *interp, struct szl_obj *exp)
{
	struct szl_obj *call, **argv, **objv;
	const char *s, *proc;
	enum szl_res res;
	size_t i, j, len, argc;

	/* if no return value is specified, fall back to the empty object */
	szl_empty_result(interp);

	if (!szl_obj_list(interp, exp, &argv, &argc))
		return SZL_ERR;

	if (argc > INT_MAX) {
		szl_set_result_str(interp, "statement is too long", -1);
		return SZL_ERR;
	}

	if (interp->level >= SZL_MAX_NESTING) {
		szl_set_result_str(interp, "reached nesting limit", -1);
		return SZL_ERR;
	}

	s = szl_obj_str(interp, exp, &len);
	if (!s)
		return SZL_ERR;

	call = szl_new_call(interp, interp->current, s, len, 1);
	if (!call)
		return SZL_ERR;

	objv = (struct szl_obj **)malloc(sizeof(struct szl_obj *) * argc);
	if (!objv) {
		szl_obj_unref(call);
		return SZL_ERR;
	}

	/* the first token is the procedure - try to find a variable named that way
	 * and if it fails evaluate this token */
	objv[0] = szl_get(interp, argv[0]);
	if (!objv[0]) {
		proc = szl_obj_str(interp, argv[0], NULL);
		if (!proc) {
			szl_obj_unref(call);
			free(objv);
			return SZL_ERR;
		}

		res = szl_eval(interp, &objv[0], proc);
		if (res != SZL_OK) {
			szl_obj_unref(call);
			free(objv);
			return res;
		}
	}

	/* set the current function, so evaluation of refers to the right frame */
	interp->current = call;
	++interp->level;

	/* evaluate all arguments */
	for (i = 1; i < argc; ++i) {
		s = szl_obj_str(interp, argv[i], NULL);
		if (s) {
			res = szl_eval(interp, &objv[i], s);
			if (res == SZL_OK)
				continue;
		}

		for (j = 0; j < i; ++j)
			szl_obj_unref(objv[j]);

		--interp->level;
		interp->current = call->caller;

		szl_obj_unref(call);
		free(objv);
		return res;
	}

	/* call objv[0] */
	szl_empty_result(interp);
	res = szl_call(interp, argc, objv);

	--interp->level;
	interp->current = call->caller;

	for (i = 0; i < argc; ++i)
		szl_obj_unref(objv[i]);

	szl_obj_unref(call);
	free(objv);

	/* set the special SZL_PREV_RET_OBJ_NAME variable so it points to the
	 * procedure return value */
	if (!szl_local(interp,
	               interp->current,
	               SZL_PREV_RET_OBJ_NAME,
	               interp->last))
		return SZL_ERR;

	return res;
}

static
char **szl_split_lines(struct szl_interp *interp,
                       char *s,
                       const size_t len,
                       size_t *n)
{
	char **lines = NULL, **mlines, *line = s, *next = NULL;
	const char *fmt = NULL;
	size_t i = 0;
	int mn, nbraces = 0, nbrackets = 0;

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

		if (i == len - 1) {
			if (nbraces != 0)
				fmt = "unbalanced {}: %s\n";
			else if (nbrackets != 0)
				fmt = "unbalanced []: %s\n";

			if (fmt) {
				szl_set_result_fmt(interp, fmt, s);
				if (lines)
					free(lines);
				return NULL;
			}
		}
		else if ((s[i] != '\n') || (nbraces != 0) || (nbrackets != 0)) {
			++i;
			continue;
		}

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
		++i;
	}

	return lines;
}

int szl_parse_block(struct szl_interp *interp,
                    struct szl_block *block,
                    char *s,
                    const size_t len)
{
	char **lines;
	size_t i, j;

	/* make sure the line is terminated */
	s[len] = '\0';

	lines = szl_split_lines(interp, s, len, &block->nlines);
	if (!lines)
		return 0;

	block->lines = (struct szl_obj **)malloc(
	                                  sizeof(struct szl_obj *) * block->nlines);
	if (!block->lines) {
		free(lines);
		return 0;
	}

	for (i = 0; i < block->nlines; ++i) {
		block->lines[i] = szl_new_str(lines[i], -1);
		if (!block->lines[i]) {
			for (j = 0; j < i; ++j)
				szl_obj_unref(block->lines[j]);
			free(block->lines);
			free(lines);
		}
	}

	free(lines);
	return 1;
}

enum szl_res szl_run_block(struct szl_interp *interp,
                           const struct szl_block *block)
{
	size_t i;
	enum szl_res res = SZL_OK;

	szl_empty_result(interp);

	for (i = 0; i < block->nlines; ++i) {
		res = szl_run_line(interp, block->lines[i]);
		if (res != SZL_OK)
			break;
	}

	return res;
}

void szl_free_block(struct szl_block *block)
{
	size_t i;

	for (i = 0; i < block->nlines; ++i)
		szl_obj_unref(block->lines[i]);
	free(block->lines);
}

enum szl_res szl_run(struct szl_interp *interp, const char *s, const size_t len)
{
	struct szl_block block;
	char *s2;
	enum szl_res res;

	s2 = strndup(s, len);
	if (!s2)
		return SZL_ERR;

	if (!szl_parse_block(interp, &block, s2, len)) {
		free(s2);
		return SZL_ERR;
	}

	res = szl_run_block(interp, &block);

	szl_free_block(&block);
	free(s2);
	return res;
}

int szl_load(struct szl_interp *interp, const char *name)
{
	char path[PATH_MAX];
	char init_name[SZL_MAX_EXT_INIT_FUNC_NAME_LEN + 1];
	char **builtin;
	struct szl_ext *exts;
	void *handle;
	szl_ext_init init;
	unsigned int nexts = interp->nexts + 1;

	for (builtin = szl_builtin_exts; *builtin; ++builtin) {
		if (strcmp(*builtin, name) == 0)
			return 1;
	}

	snprintf(path, sizeof(path), SZL_EXT_PATH_FMT, name);
	handle = dlopen(path, RTLD_LAZY);
	if (!handle) {
		szl_set_result_fmt(interp, "failed to load %s", path);
		return 0;
	}

	snprintf(init_name, sizeof(init_name), SZL_EXT_INIT_FUNC_NAME_FMT, name);
	init = (szl_ext_init)dlsym(handle, init_name);
	if (!init) {
		szl_set_result_fmt(interp, "failed to locate %s", init_name);
		szl_unload(handle);
		return 0;
	}

	exts = (struct szl_ext *)realloc(interp->exts,
	                                 sizeof(struct szl_ext) * nexts);
	if (!exts) {
		szl_unload(handle);
		return 0;
	}

	exts[interp->nexts].handle = (void *)handle;
	exts[interp->nexts].objs = 0;
	exts[interp->nexts].nobjs = 0;

	interp->exts = exts;
	interp->nexts = nexts;

	if (!init(interp)) {
		szl_unload(handle);
		interp->exts[interp->nexts - 1].handle = NULL;
		return 0;
	}

	return 1;
}

void szl_unload(void *h)
{
	dlclose(h);
}

enum szl_res szl_source(struct szl_interp *interp, const char *path)
{
	struct szl_block block;
	struct stat stbuf;
	char *buf;
	size_t len;
	int fd, res;

	szl_empty_result(interp);

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		szl_set_result_fmt(interp, "failed to open %s", path);
		return SZL_ERR;
	}

	if (fstat(fd, &stbuf) < 0) {
		close(fd);
		szl_set_result_fmt(interp, "failed to stat %s", path);
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
		szl_set_result_fmt(interp, "failed to read %s", path);
		return SZL_ERR;
	}
	buf[len] = '\0';

	close(fd);

	if (!szl_parse_block(interp, &block, buf, (size_t)stbuf.st_size)) {
		free(buf);
		return SZL_ERR;
	}

	res = szl_run_block(interp, &block);
	szl_free_block(&block);
	free(buf);
	return res;
}

static
enum szl_res szl_stream_read(struct szl_interp *interp,
                             struct szl_stream *strm,
                             const size_t len)
{
	struct szl_obj *obj;
	unsigned char *buf;
	ssize_t out;

	if (!strm->ops->read) {
		szl_set_result_str(interp, "read from unsupported stream", -1);
		return SZL_ERR;
	}

	if (strm->closed)
		return SZL_OK;

	buf = (unsigned char *)malloc(len + 1);
	if (!buf)
		return SZL_ERR;

	out = strm->ops->read(strm->priv, buf, len);
	if (out > 0) {
		buf[out] = '\0';
		obj = szl_new_str_noalloc((char *)buf, (size_t)out);
		if (!obj) {
			free(buf);
			return SZL_ERR;
		}
		szl_set_result(interp, obj);
	}
	else {
		free(buf);

		if (out < 0)
			return SZL_ERR;
	}

	return SZL_OK;
}

static
enum szl_res szl_stream_read_all(struct szl_interp *interp,
                                 struct szl_stream *strm)
{
	struct szl_obj *obj;
	unsigned char *buf, *nbuf;
	ssize_t len = SZL_STREAM_BUFSIZ, tot, more;

	if (!strm->ops->read) {
		szl_set_result_str(interp, "read from unsupported stream", -1);
		return SZL_ERR;
	}

	if (strm->closed)
		return SZL_OK;

	/* if we know the amount of pending data, we'd like to enhance efficiency by
	 * allocating a sufficient buffer in first place, so we allocate memory only
	 * once */
	if (strm->ops->size) {
		len = strm->ops->size(strm->priv);
		if (len < 0)
			return SZL_ERR;
		else if (len == 0)
			return SZL_OK;
	}

	buf = (unsigned char *)malloc(len + 1);
	if (!buf)
		return SZL_ERR;

	tot = strm->ops->read(strm->priv, buf, len);
	if (tot > 0) {
		/* if we don't know whether there's more data to read, read more data in
		 * SZL_STREAM_BUFSIZ-byte chunks until the read() callback returns 0 */
		if (!strm->ops->size) {
			do {
				len += SZL_STREAM_BUFSIZ;
				nbuf = (unsigned char *)realloc(buf, len);
				if (!nbuf) {
					free(buf);
					return SZL_ERR;
				}
				buf = nbuf;

				more = strm->ops->read(strm->priv,
				                       buf + tot,
				                       SZL_STREAM_BUFSIZ);
				if (more < 0) {
					free(buf);
					return SZL_ERR;
				}
				else if (more)
					tot += more;
				else
					break;
			} while (1);
		}

		/* wrap the buffer with a string object */
		buf[tot] = '\0';
		obj = szl_new_str_noalloc((char *)buf, (size_t)tot);
		if (!obj) {
			free(buf);
			return SZL_ERR;
		}

		return szl_set_result(interp, obj);
	}

	free(buf);
	return (tot < 0) ? SZL_ERR : SZL_OK;
}

static
enum szl_res szl_stream_write(struct szl_interp *interp,
                              struct szl_stream *strm,
                              const unsigned char *buf,
                              const size_t len)
{
	if (!strm->ops->write) {
		szl_set_result_str(interp, "write to unsupported stream", -1);
		return SZL_ERR;
	}

	if (strm->closed) {
		szl_set_result_str(interp, "write to closed stream", -1);
		return SZL_ERR;
	}

	if (strm->ops->write(strm->priv, buf, len) == (ssize_t)len)
		return SZL_OK;

	return SZL_ERR;
}

static
enum szl_res szl_stream_flush(struct szl_interp *interp,
                              struct szl_stream *strm)
{
	if (!strm->ops->flush)
		return SZL_OK;

	if (strm->closed) {
		szl_set_result_str(interp, "flush of closed stream", -1);
		return SZL_ERR;
	}

	return strm->ops->flush(strm->priv);
}

static
void szl_stream_close(struct szl_stream *strm)
{
	if (!strm->closed) {
		if (strm->ops->close && !strm->keep)
			strm->ops->close(strm->priv);

		if (strm->buf)
			free(strm->buf);

		strm->closed = 1;
	}
}

void szl_stream_free(struct szl_stream *strm)
{
	szl_stream_close(strm);
	free(strm);
}

static
struct szl_obj *szl_stream_handle(struct szl_interp *interp,
                                  struct szl_stream *strm)
{
	if (strm->closed)
		return szl_empty(interp);

	return szl_new_int(strm->ops->handle(strm->priv));
}

static
enum szl_res szl_stream_accept(struct szl_interp *interp,
                               struct szl_stream *strm)
{
	struct szl_obj *obj;
	struct szl_stream *client;

	if (!strm->ops->accept) {
		szl_set_result_str(interp, "accept from unsupported stream", -1);
		return SZL_ERR;
	}

	if (strm->closed) {
		szl_set_result_str(interp, "accept from closed stream", -1);
		return SZL_ERR;
	}

	client = strm->ops->accept(strm->priv);
	if (!client)
		return SZL_ERR;

	obj = szl_new_stream(interp, client, "stream.client");
	if (!obj) {
		szl_stream_free(client);
		return SZL_ERR;
	}

	return szl_set_result(interp, obj);
}

static
enum szl_res szl_stream_proc(struct szl_interp *interp,
                             const int objc,
                             struct szl_obj **objv)
{
	struct szl_stream *strm = (struct szl_stream *)objv[0]->priv;
	struct szl_obj *obj;
	const char *op;
	const char *buf;
	szl_int req;
	size_t len;

	szl_empty_result(interp);

	op = szl_obj_str(interp, objv[1], NULL);
	if (!op)
		return SZL_ERR;

	if (objc == 3) {
		if (strcmp("read", op) == 0) {
			if (!szl_obj_int(interp, objv[2], &req))
				return SZL_ERR;

			if (!req)
				return SZL_OK;

			if (req > SSIZE_MAX)
				req = SSIZE_MAX;

			return szl_stream_read(interp, strm, (size_t)req);
		}
		else if (strcmp("write", op) == 0) {
			buf = szl_obj_str(interp, objv[2], &len);
			if (!buf)
				return SZL_ERR;

			if (!len)
				return SZL_OK;

			return szl_stream_write(interp,
			                        strm,
			                        (const unsigned char *)buf,
			                        len);
		}
	}
	else if (objc == 2) {
		if (strcmp("read", op) == 0)
			return szl_stream_read_all(interp, strm);
		if (strcmp("flush", op) == 0)
			return szl_stream_flush(interp, strm);
		else if (strcmp("close", op) == 0) {
			szl_stream_close(strm);
			return SZL_OK;
		}
		else if (strcmp("accept", op) == 0)
			return szl_stream_accept(interp, strm);
		else if (strcmp("handle", op) == 0) {
			obj = szl_stream_handle(interp, strm);
			if (!obj)
				return SZL_ERR;
			return szl_set_result(interp, obj);
		}
	}

	return szl_usage(interp, objv[0]);
}

static
void szl_stream_del(void *priv)
{
	szl_stream_close((struct szl_stream *)priv);
	free(priv);
}

struct szl_obj *szl_new_stream(struct szl_interp *interp,
                               struct szl_stream *strm,
                               const char *type)
{
	char name[sizeof("socket.stream:"SZL_PASTE(SZL_INT_MIN))];
	struct szl_obj *proc;

	szl_new_obj_name(interp, type, name, sizeof(name), strm->priv);
	proc = szl_new_proc(interp,
	                    name,
	                    1,
	                    3,
	                    "strm read|write|flush|handle|close|accept ?len?",
	                    szl_stream_proc,
	                    szl_stream_del,
	                    strm);
	if (!proc)
		return NULL;

	return szl_obj_ref(proc);
}
