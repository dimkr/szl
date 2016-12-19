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
#include <math.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <wchar.h>
#include <time.h>

#include "szl.h"

/*
 * reference counting
 */

__attribute__((nonnull(1)))
struct szl_obj *szl_ref(struct szl_obj *obj)
{
	++obj->refc;
	return obj;
}

__attribute__((nonnull(1)))
void szl_unref(struct szl_obj *obj)
{
	size_t i;

	if (!--obj->refc) {
		if (obj->types & (1 << SZL_TYPE_STR))
			free(obj->val.s);

		if (obj->types & (1 << SZL_TYPE_WSTR))
			free(obj->val.w);

		if ((obj->types & (1 << SZL_TYPE_LIST)) && obj->val.llen) {
			for (i = 0; i < obj->val.llen; ++i)
				szl_unref(obj->val.items[i]);

			free(obj->val.items);
		}

		if (obj->types & (1 << SZL_TYPE_CODE))
			szl_unref(obj->val.c);

		if (obj->del)
			obj->del(obj->priv);

		free(obj);
	}
}

/*
 * type system
 */

static
int szl_no_cast(struct szl_interp *interp, struct szl_obj *obj)
{
	return 1;
}

int szl_isspace(const char ch)
{
	return ((ch == ' ') || (ch == '\t') || (ch == '\n') || (ch == '\r'));
}

static
char *szl_next_item(struct szl_interp *interp, char *s)
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

		szl_set_last_fmt(interp, "unbalanced %c%c: %s", odelim, cdelim, s);
		return NULL;
	}

	if (szl_isspace(s[0])) {
		for (pos = s + 1; szl_isspace(pos[0]); ++pos);
		if (pos[0] != '\0') {
			/* terminate the previous item */
			s[0] = '\0';

			/* terminate the current item */
			*(pos - 1) = '\0';
			return pos;
		}
	}
	else {
		/* otherwise, skip all leading non-whitespace characters and look for
		 * the next item after them */
		pos = s + 1;
		do {
			if (szl_isspace(pos[0]))
				return szl_next_item(interp, pos);
			if (pos[0] == '\0')
				break;
			++pos;
		} while (1);
	}

	/* expression ends with whitespace */
	return NULL;
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

static
int szl_split(struct szl_interp *interp,
              char *s,
              struct szl_obj ***items,
              size_t *len)
{
	struct szl_obj **mitems;
	char *tok,  *next, *start, *end;
	size_t i;
	int mlen = 1;

	*len = 0;
	*items = NULL;

	if (s[0] == '\0')
		return 0;

	szl_trim(s, &start, &end);

	next = szl_next_item(interp, start);
	tok = start;

	do {
		mitems = (struct szl_obj **)realloc(*items,
		                                    sizeof(struct szl_obj *) * mlen);
		if (!mitems) {
			if (*items) {
				for (i = 0; i < *len; ++i)
					szl_unref(*items[i]);

				free(*items);
			}
			return 0;
		}

		mitems[*len] = szl_new_str(tok, -1);
		if (!mitems[*len]) {
			for (i = 0; i < *len; ++i)
				szl_unref(mitems[i]);
			free(mitems);
			return 0;
		}

		*len = mlen;
		*items = mitems;

		if (!next)
			break;

		tok = next;
		++mlen;
		next = szl_next_item(interp, tok);
	} while (1);

	return 1;
}

static
int szl_str_to_list(struct szl_interp *interp, struct szl_obj *obj)
{
	char *buf2;
	int ret;

	if (!obj->val.slen) {
		obj->val.items = NULL;
		obj->val.llen = 0;
		/* an empty dictionary is sorted */
		obj->flags |= SZL_OBJECT_SORTED;
		return 1;
	}

	buf2 = strndup(obj->val.s, obj->val.slen);
	if (!buf2)
		return 0;

	ret = szl_split(interp, buf2, &obj->val.items, &obj->val.llen);
	free(buf2);
	obj->flags &= ~SZL_OBJECT_SORTED;
	obj->types |= 1 << SZL_TYPE_LIST;
	return ret;
}

#define SZL_STR_CONV(fname,                                                    \
                     stype,                                                    \
                     intype,                                                   \
                     outtype,                                                  \
                     inmemb,                                                   \
                     inlmemb,                                                  \
                     outmemb,                                                  \
                     outlmemb,                                                 \
                     convproc,                                                 \
                     nullc,                                                    \
                     szltype)                                                  \
static                                                                         \
int fname(struct szl_interp *interp, struct szl_obj *obj)                      \
{                                                                              \
	mbstate_t ps;                                                              \
	const intype *p = obj->val.inmemb;                                         \
	size_t out, rem;                                                           \
                                                                               \
	if (obj->val.inlmemb >= (SIZE_MAX / sizeof(outtype)))                      \
		return 0;                                                              \
                                                                               \
	obj->val.outmemb =                                                         \
	              (outtype *)malloc(sizeof(outtype) * (obj->val.inlmemb + 1)); \
	if (!obj->val.outmemb)                                                     \
		return 0;                                                              \
                                                                               \
	memset(&ps, 0, sizeof(ps));                                                \
	obj->val.outlmemb = 0;                                                     \
	rem = obj->val.inlmemb * sizeof(intype);                                   \
                                                                               \
	do {                                                                       \
		out = convproc(obj->val.outmemb + obj->val.outlmemb,                   \
		               &p,                                                     \
		               rem,                                                    \
		               &ps);                                                   \
		if ((out == (size_t)-1) || (out == (size_t)-2)) {                      \
			free(obj->val.outmemb);                                            \
			szl_set_last_fmt(interp, "bad "stype": %s", obj->val.inmemb);      \
			return 0;                                                          \
		}                                                                      \
		obj->val.outlmemb += out;                                              \
                                                                               \
		if (!p || !out)                                                        \
			break;                                                             \
                                                                               \
		rem -= out;                                                            \
	} while (1);                                                               \
                                                                               \
	obj->val.outmemb[obj->val.outlmemb] = nullc;                               \
	obj->types |= 1 << szltype;                                                \
	return 1;                                                                  \
}

SZL_STR_CONV(szl_str_to_wstr,
             "str",
             char,
             wchar_t,
             s,
             slen,
             w,
             wlen,
             mbsrtowcs,
             L'\0',
             SZL_TYPE_WSTR)

static
int szl_str_to_int(struct szl_interp *interp, struct szl_obj *obj)
{
	if (sscanf(obj->val.s, SZL_INT_SCANF_FMT"d", &obj->val.i) == 1) {
		obj->types |= 1 << SZL_TYPE_INT;
		return 1;
	}

	szl_set_last_fmt(interp, "bad int: %s", obj->val.s);
	return 0;
}

static
int szl_str_to_float(struct szl_interp *interp, struct szl_obj *obj)
{
	if (sscanf(obj->val.s, SZL_FLOAT_SCANF_FMT, &obj->val.f) == 1) {
		obj->types |= 1 << SZL_TYPE_FLOAT;
		return 1;
	}

	szl_set_last_fmt(interp, "bad float: %s", obj->val.s);
	return 0;
}

static
int szl_str_to_code(struct szl_interp *interp, struct szl_obj *obj)
{
	struct szl_obj *stmt;
	char *copy, *line, *next = NULL;
	const char *fmt = NULL;
	size_t i = 0;
	int nbraces = 0, nbrackets = 0;

	copy = (char *)strndup(obj->val.s, obj->val.slen);
	if (!copy)
		return 0;

	obj->val.c = szl_new_list(NULL, 0);
	if (!obj->val.c) {
		free(copy);
		return 0;
	}

	line = copy;
	while (i < obj->val.slen) {
		if (copy[i] == '{')
			++nbraces;
		else if (copy[i] == '}')
			--nbraces;
		else if (copy[i] == '[')
			++nbrackets;
		else if (copy[i] == ']')
			--nbrackets;

		if (i == obj->val.slen - 1) {
			if (nbraces != 0)
				fmt = "unbalanced {}: %s\n";
			else if (nbrackets != 0)
				fmt = "unbalanced []: %s\n";

			if (fmt) {
				szl_set_last_fmt(interp, fmt, obj->val.s);
				szl_unref(obj->val.c);
				free(copy);
				return 0;
			}
		}
		else if ((copy[i] != '\n') || (nbraces != 0) || (nbrackets != 0)) {
			++i;
			continue;
		}

		if (copy[i] == '\n')
			copy[i] = '\0';
		next = &copy[i + 1];

		/* if the line contains nothing but whitespace, skip it */
		for (; szl_isspace(*line); ++line);
		if (line[0] != '\0' && line[0] != SZL_COMMENT_PFIX) {
			stmt = szl_new_str(line, next - line);
			if (!stmt) {
				szl_unref(obj->val.c);
				free(copy);
				return 0;
			}

			if (!szl_list_append(interp, obj->val.c, stmt)) {
				szl_unref(stmt);
				szl_unref(obj->val.c);
				free(copy);
				return 0;
			}

			szl_unref(stmt);
		}

		line = next;
		++i;
	}

	free(copy);
	obj->types |= 1 << SZL_TYPE_CODE;
	return 1;
}

SZL_STR_CONV(szl_wstr_to_str,
             "wstr",
             wchar_t,
             char,
             w,
             wlen,
             s,
             slen,
             wcsrtombs,
             L'\0',
             SZL_TYPE_STR)

static
int szl_wstr_to_list(struct szl_interp *interp, struct szl_obj *obj)
{
	return szl_wstr_to_str(interp, obj) && szl_str_to_list(interp, obj);
}

static
int szl_wstr_to_int(struct szl_interp *interp, struct szl_obj *obj)
{
	return szl_wstr_to_str(interp, obj) && szl_str_to_int(interp, obj);
}

static
int szl_wstr_to_float(struct szl_interp *interp, struct szl_obj *obj)
{
	return szl_wstr_to_str(interp, obj) && szl_str_to_float(interp, obj);
}

static
int szl_wstr_to_code(struct szl_interp *interp, struct szl_obj *obj)
{
	return szl_wstr_to_str(interp, obj) && szl_str_to_code(interp, obj);
}

static
int szl_list_to_str(struct szl_interp *interp, struct szl_obj *obj)
{
	struct szl_obj *str;

	str = szl_join(interp, interp->space, obj->val.items, obj->val.llen, 1);
	if (!str)
		return 0;

	/* we "steal" the string representation of the result */
	obj->val.s = str->val.s;
	obj->val.slen = str->val.slen;

	str->types &= ~(1 << SZL_TYPE_STR);
	szl_unref(str);

	obj->types |= 1 << SZL_TYPE_STR;
	return 1;
}

static
int szl_list_to_wstr(struct szl_interp *interp, struct szl_obj *obj)
{
	if (!szl_list_to_str(interp, obj))
		return 0;

	return szl_str_to_wstr(interp, obj);
}

static
int szl_list_to_int(struct szl_interp *interp, struct szl_obj *obj)
{
	if (obj->val.llen) {
		szl_set_last_str(interp, "bad int", sizeof("bad int") - 1);
		return 0;
	}

	return szl_list_to_str(interp, obj) && szl_str_to_int(interp, obj);
}

static
int szl_list_to_float(struct szl_interp *interp, struct szl_obj *obj)
{
	if (obj->val.llen) {
		szl_set_last_str(interp, "bad float", sizeof("bad float") - 1);
		return 0;
	}

	return szl_list_to_str(interp, obj) && szl_str_to_float(interp, obj);
}

static
int szl_list_to_code(struct szl_interp *interp, struct szl_obj *obj)
{
	obj->val.c = szl_new_list(obj->val.items, obj->val.llen);
	if (!obj->val.c)
		return 0;

	obj->types |= 1 << SZL_TYPE_CODE;
	return 1;
}

static
int szl_int_to_str(struct szl_interp *interp, struct szl_obj *obj)
{
	int len;

	len = asprintf(&obj->val.s, SZL_INT_FMT"d", obj->val.i);
	if (len < 0)
		return 0;

	obj->val.slen = (size_t)len;
	obj->types |= 1 << SZL_TYPE_STR;
	return 1;
}

static
int szl_int_to_wstr(struct szl_interp *interp, struct szl_obj *obj)
{
	return szl_int_to_str(interp, obj) && szl_str_to_wstr(interp, obj);
}

static
int szl_int_to_list(struct szl_interp *interp, struct szl_obj *obj)
{
	return szl_int_to_str(interp, obj) && szl_str_to_list(interp, obj);
}

static
int szl_int_to_float(struct szl_interp *interp, struct szl_obj *obj)
{
	obj->val.f = (szl_float)obj->val.i;
	obj->types |= 1 << SZL_TYPE_FLOAT;
	return 1;
}

static
int szl_int_to_code(struct szl_interp *interp, struct szl_obj *obj)
{
	return szl_int_to_str(interp, obj) && szl_str_to_code(interp, obj);
}

static
int szl_float_to_str(struct szl_interp *interp, struct szl_obj *obj)
{
	int len, i;

	len = asprintf(&obj->val.s, SZL_FLOAT_FMT, obj->val.f);
	if (len < 0)
		return 0;

	obj->val.slen = (size_t)len;
	if (obj->val.slen) {
		for (i = len - 1; i >= 0; --i) {
			if (obj->val.s[i] == '0')
				--obj->val.slen;
			else {
				if (obj->val.s[i] == '.')
					--obj->val.slen;

				obj->val.s[obj->val.slen] = '\0';
				break;
			}
		}
	}

	obj->types |= 1 << SZL_TYPE_STR;
	return 1;
}

static
int szl_float_to_wstr(struct szl_interp *interp, struct szl_obj *obj)
{
	return szl_float_to_str(interp, obj) && szl_str_to_wstr(interp, obj);
}

static
int szl_float_to_list(struct szl_interp *interp, struct szl_obj *obj)
{
	return szl_float_to_str(interp, obj) && szl_str_to_list(interp, obj);
}

static
int szl_float_to_int(struct szl_interp *interp, struct szl_obj *obj)
{
	obj->val.i = (szl_int)round(obj->val.f);
	obj->types |= 1 << SZL_TYPE_INT;
	return 1;
}

static
int szl_float_to_code(struct szl_interp *interp, struct szl_obj *obj)
{
	return szl_float_to_str(interp, obj) &&szl_str_to_code(interp, obj);
}

typedef int (*szl_cast_t)(struct szl_interp *, struct szl_obj *);

static
const szl_cast_t szl_cast_table[6][6] = {
	{ /* SZL_TYPE_LIST */
		szl_no_cast,
		szl_list_to_str,
		szl_list_to_wstr,
		szl_list_to_int,
		szl_list_to_float,
		szl_list_to_code
	},
	{ /* SZL_TYPE_STR */
		szl_str_to_list,
		szl_no_cast,
		szl_str_to_wstr,
		szl_str_to_int,
		szl_str_to_float,
		szl_str_to_code
	},
	{ /* SZL_TYPE_WSTR */
		szl_wstr_to_list,
		szl_wstr_to_str,
		szl_no_cast,
		szl_wstr_to_int,
		szl_wstr_to_float,
		szl_wstr_to_code
	},
	{ /* SZL_TYPE_INT */
		szl_int_to_list,
		szl_int_to_str,
		szl_int_to_wstr,
		szl_no_cast,
		szl_int_to_float,
		szl_int_to_code
	},
	{ /* SZL_TYPE_FLOAT */
		szl_float_to_list,
		szl_float_to_str,
		szl_float_to_wstr,
		szl_float_to_int,
		szl_no_cast,
		szl_float_to_code
	}
};

static
int szl_cast(struct szl_interp *interp,
             struct szl_obj *obj,
             const unsigned int type)
{
	unsigned int i, mask;

	if (obj->types & (1 << type))
		return 1;

	for (i = 1; i <= sizeof(szl_cast_table) / sizeof(szl_cast_table[0]); ++i) {
		mask = 1 << i;

		if ((i != type) && (obj->types & mask))
			return szl_cast_table[i - 1][type - 1](interp, obj);
	}

	/* unreachable */
	szl_set_last_fmt(interp, "unknown type: %u", type);
	return 0;
}

__attribute__((nonnull(1, 2, 3)))
int szl_as_str(struct szl_interp *interp,
               struct szl_obj *obj,
               char **buf,
               size_t *len)
{
	if (!szl_cast(interp, obj, SZL_TYPE_STR))
		return 0;

	*buf = obj->val.s;
	if (len)
		*len = obj->val.slen;
	return 1;
}

__attribute__((nonnull(1, 2, 3)))
int szl_as_wstr(struct szl_interp *interp,
                struct szl_obj *obj,
                wchar_t **ws,
                size_t *len)
{
	if (!szl_cast(interp, obj, SZL_TYPE_WSTR))
		return 0;

	*ws = obj->val.w;
	if (len)
		*len = obj->val.wlen;
	return 1;
}

__attribute__((nonnull(1, 2, 3, 4)))
int szl_as_list(struct szl_interp *interp,
                struct szl_obj *obj,
                struct szl_obj ***items,
                size_t *len)
{
	if (!szl_cast(interp, obj, SZL_TYPE_LIST))
		return 0;

	*items = obj->val.items;
	*len = obj->val.llen;
	return 1;
}

__attribute__((nonnull(1, 2, 3, 4)))
int szl_as_dict(struct szl_interp *interp,
                struct szl_obj *obj,
                struct szl_obj ***items,
                size_t *len)
{
	if (!szl_as_list(interp, obj, items, len))
		return 0;

	if (*len % 2 == 1) {
		szl_set_last_str(interp, "bad dict", sizeof("bad dict") - 1);
		return 0;
	}

	return 1;
}

__attribute__((nonnull(1, 2, 3)))
int szl_as_int(struct szl_interp *interp, struct szl_obj *obj, szl_int *i)
{
	if (!szl_cast(interp, obj, SZL_TYPE_INT))
		return 0;

	*i = obj->val.i;
	return 1;
}

__attribute__((nonnull(1, 2, 3)))
int szl_as_float(struct szl_interp *interp, struct szl_obj *obj, szl_float *f)
{
	if (!szl_cast(interp, obj, SZL_TYPE_FLOAT))
		return 0;

	*f = obj->val.f;
	return 1;
}

__attribute__((nonnull(1, 2)))
int szl_as_bool(struct szl_obj *obj, int *b)
{
	if (obj->types & (1 << SZL_TYPE_INT)) {
		*b = obj->val.i != 0;
		return 1;
	}

	if (szl_unlikely(obj->types & (1 << SZL_TYPE_FLOAT))) {
		*b = obj->val.f != 0;
		return 1;
	}

	if (szl_likely(obj->types & (1 << SZL_TYPE_STR))) {
		/* "0" is false, anything else is true */
		if (obj->val.slen == 1)
			*b = (obj->val.s[0] == '0') ? 0 : 1;
		else
			*b = obj->val.slen > 0;
		return 1;
	}

	if (obj->types & (1 << SZL_TYPE_LIST)) {
		*b = obj->val.llen > 0;
		return 1;
	}

	return 0;
}

static
int szl_as_code(struct szl_interp *interp,
                struct szl_obj *obj,
                struct szl_obj ***stmts,
                size_t *len)
{
	return szl_cast(interp,
	                obj,
	                SZL_TYPE_CODE) &&
	       szl_as_list(interp,
	                   obj->val.c,
	                   stmts,
	                   len);
}

/*
 * object creation
 */

static
enum szl_res szl_bad_proc(struct szl_interp *interp,
                          const unsigned int objc,
                          struct szl_obj **objv)
{
	char *s;

	if (szl_as_str(interp, objv[0], &s, NULL))
		szl_set_last_fmt(interp, "not a proc: %s", s);
	else
		szl_set_last_str(interp, "not a proc", sizeof("not a proc") - 1);

	return SZL_ERR;
}

#define SZL_OBJ_INIT(obj, type) \
	obj->types = 1 << type;     \
	obj->refc = 1;              \
	obj->proc = szl_bad_proc;   \
	obj->min_objc = -1;         \
	obj->max_objc = -1;         \
	obj->del = NULL;            \
	obj->flags = 0

#define SZL_NEW_STR(fname, ctype, stype, memb, lmemb, nullc, lenproc) \
__attribute__((nonnull(1)))                                           \
struct szl_obj *fname(const ctype *buf, ssize_t len)                  \
{                                                                     \
	struct szl_obj *obj;                                              \
	size_t rlen, blen;                                                \
                                                                      \
	if (szl_likely(len > 0))                                          \
		rlen = (size_t)len;                                           \
	else                                                              \
		rlen = lenproc(buf);                                          \
                                                                      \
	if (rlen >= (SSIZE_MAX / sizeof(ctype)))                          \
		return NULL;                                                  \
                                                                      \
	obj = (struct szl_obj *)malloc(sizeof(*obj));                     \
	if (!obj)                                                         \
		return obj;                                                   \
                                                                      \
	blen = sizeof(ctype) * rlen;                                      \
	obj->val.memb = (ctype *)malloc(blen + sizeof(ctype));            \
	if (!obj->val.memb) {                                             \
		free(obj);                                                    \
		return NULL;                                                  \
	}                                                                 \
	memcpy(obj->val.memb, buf, blen);                                 \
	obj->val.memb[rlen] = nullc;                                      \
	obj->val.lmemb = rlen;                                            \
                                                                      \
	SZL_OBJ_INIT(obj, stype);                                         \
	return obj;                                                \
}

SZL_NEW_STR(szl_new_str, char, SZL_TYPE_STR, s, slen, '\0', strlen)
SZL_NEW_STR(szl_new_wstr, wchar_t, SZL_TYPE_WSTR, w, wlen, L'\0', wcslen)

__attribute__((nonnull(1)))
struct szl_obj *szl_new_str_noalloc(char *buf, const size_t len)
{
	struct szl_obj *obj;

	obj = (struct szl_obj *)malloc(sizeof(*obj));
	if (!obj)
		return obj;

	obj->val.s = buf;
	obj->val.slen = len;

	SZL_OBJ_INIT(obj, SZL_TYPE_STR);
	return obj;
}

static
struct szl_obj *szl_new_str_va(const char *fmt, va_list ap)
{
	struct szl_obj *str = NULL;
	char *s;
	int len;

	len = vasprintf(&s, fmt, ap);

	if (len >= 0) {
		str = szl_new_str_noalloc(s, (size_t)len);
		if (!str)
			free(s);
	}

	return str;
}

__attribute__((nonnull(1)))
__attribute__((format(printf, 1, 2)))
struct szl_obj *szl_new_str_fmt(const char *fmt, ...)
{
	struct szl_obj *str;
	va_list ap;

	va_start(ap, fmt);
	str = szl_new_str_va(fmt, ap);
	va_end(ap);

	return str;
}

static
struct szl_obj *szl_copy_dict(struct szl_interp *interp, struct szl_obj *dict);

static
void szl_pop_call(struct szl_interp *interp)
{
	struct szl_frame *call = interp->current;

	szl_unref(call->args);
	szl_unref(call->locals);

	--interp->depth;
	interp->current = call->caller;

	free(call);
}

static
struct szl_frame *szl_new_call(struct szl_interp *interp,
                               struct szl_obj *stmt,
                               struct szl_frame *caller)
{
	struct szl_frame *call;

	call = (struct szl_frame *)malloc(sizeof(struct szl_frame));
	if (!call)
		return NULL;

	if (caller) {
		call->locals = szl_copy_dict(interp, caller->locals);
		if (!call->locals) {
			free(call);
			return NULL;
		}
		call->caller = caller;
	}
	else {
		call->locals = szl_new_dict(NULL, 0);
		if (!call->locals) {
			free(call);
			return NULL;
		}
		call->caller = NULL;
	}

	call->args = szl_new_list(NULL, 0);
	if (!call->args) {
		szl_unref(call->locals);
		free(call);
		return NULL;
	}

	++interp->depth;
	interp->current = call;

	return call;
}

static
struct szl_obj *szl_new_int_nocache(const szl_int i)
{
	struct szl_obj *obj;

	obj = (struct szl_obj *)malloc(sizeof(*obj));
	if (obj) {
		obj->val.i = i;
		SZL_OBJ_INIT(obj, SZL_TYPE_INT);
	}

	return obj;
}

__attribute__((nonnull(1)))
struct szl_obj *szl_new_int(struct szl_interp *interp, const szl_int i)
{
	if ((i > 0) && (i < sizeof(interp->nums) / sizeof(interp->nums[0])))
		return szl_ref(interp->nums[i]);

	return szl_new_int_nocache(i);
}

struct szl_obj *szl_new_float(const szl_float f)
{
	struct szl_obj *obj;

	obj = (struct szl_obj *)malloc(sizeof(*obj));
	if (obj) {
		obj->val.f = f;
		SZL_OBJ_INIT(obj, SZL_TYPE_FLOAT);
	}

	return obj;
}

struct szl_obj *szl_new_list(struct szl_obj **objv, const size_t len)
{
	struct szl_obj *obj;
	size_t i;

	obj = (struct szl_obj *)malloc(sizeof(*obj));
	if (!obj)
		return obj;

	obj->val.items = (struct szl_obj **)malloc(
	                                            sizeof(struct szl_obj *) * len);
	if (!obj->val.items) {
		free(obj);
		return NULL;
	}
	obj->val.llen = len;
	obj->flags &= ~SZL_OBJECT_SORTED;

	for (i = 0; i < len; ++i)
		obj->val.items[i] = szl_ref(objv[i]);

	SZL_OBJ_INIT(obj, SZL_TYPE_LIST);
	return obj;
}

__attribute__((nonnull(1, 2, 6)))
struct szl_obj *szl_new_proc(struct szl_interp *interp,
                             struct szl_obj *name,
                             const int min_objc,
                             const int max_objc,
                             const char *help,
                             szl_proc_t proc,
                             szl_del_t del,
                             void *priv)
{
	struct szl_obj *obj;

	obj = szl_new_str_fmt("proc:%x", rand_r(&interp->seed));
	if (obj) {
		if (!szl_set(interp, szl_caller(interp), name, obj)) {
			szl_unref(obj);
			return NULL;
		}

		obj->min_objc = min_objc;
		obj->max_objc = max_objc;
		obj->help = help;
		obj->proc = proc;
		obj->del = del;
		obj->priv = priv;
	}

	return obj;
}

__attribute__((nonnull(1, 2, 3)))
int szl_set_args(struct szl_interp *interp,
                 struct szl_frame *call,
                 struct szl_obj *args)
{
	struct szl_obj **items, *arg;
	size_t len, i;

	/* create the arguments list object */
	if (!szl_as_list(interp, args, &items, &len) ||
	    !szl_set(interp, call, interp->args, args))
		return 0;

	/* create the local argument objects */
	for (i = 0; i < len; ++i) {
		arg = szl_new_int(interp, (szl_int)i);
		if (!arg)
			return 0;

		if (!szl_set(interp, call, arg, items[i])) {
			szl_unref(arg);
			return 0;
		}

		szl_unref(arg);
	}

	return 1;
}

/* Jenkins's one-at-a-time hash */
static
int szl_hash(struct szl_interp *interp, struct szl_obj *obj, uint32_t *hash)
{
	char *buf;
	size_t len, i;

	if (szl_unlikely(!(obj->flags & SZL_OBJECT_HASHED))) {
		if (!szl_as_str(interp, obj, &buf, &len))
			return 0;

		obj->hash = 0;
		for (i = 0; i < len; ++i) {
			obj->hash += buf[i] + (obj->hash << 10);
			obj->hash ^= (obj->hash >> 6);
		}

		obj->hash += (obj->hash << 3);
		obj->hash ^= (obj->hash >> 11);
		obj->hash += (obj->hash << 15);

		obj->flags |= SZL_OBJECT_HASHED;
	}

	if (hash)
		*hash = obj->hash;
	return 1;
}

__attribute__((nonnull(1, 2, 3, 4)))
int szl_eq(struct szl_interp *interp,
           struct szl_obj *a,
           struct szl_obj *b,
           int *eq)
{
	char *as, *bs;
	size_t alen, blen;
	uint32_t ah, bh;

	/* optimization: could be the same object */
	if (szl_likely(a != b)) {
		if (!szl_hash(interp, a, &ah) || !szl_hash(interp, b, &bh))
			return 0;

		/* if the hashes match, compare the string representations to ensure
		 * this is not a collision */
		if (szl_likely(ah != bh))
			*eq = 0;
		else {
			if (!szl_as_str(interp, a, &as, &alen) ||
			    !szl_as_str(interp, b, &bs, &blen))
				return 0;

			*eq = (alen == blen) && (strcmp(as, bs) == 0);
		}
	}
	else
		*eq = 1;

	return 1;
}

/*
 * string operations
 */

__attribute__((nonnull(1, 2, 3)))
int szl_len(struct szl_interp *interp, struct szl_obj *obj, size_t *len)
{
	char *buf;

	return szl_as_str(interp, obj, &buf, len) ? 1 : 0;
}

__attribute__((nonnull(1, 2)))
char *szl_strdup(struct szl_interp *interp, struct szl_obj *obj, size_t *len)
{
	char *buf;

	if (!szl_as_str(interp, obj, &buf, len))
		return NULL;

	return strndup(buf, *len);
}

__attribute__((nonnull(1, 2, 3)))
int szl_str_append(struct szl_interp *interp,
                   struct szl_obj *dest,
                   struct szl_obj *src)
{
	char *buf;
	size_t len;

	if (!szl_as_str(interp, src, &buf, &len))
		return 0;

	return szl_str_append_str(interp, dest, buf, len);
}

__attribute__((nonnull(1, 2, 3)))
int szl_str_append_str(struct szl_interp *interp,
                       struct szl_obj *dest,
                       const char *src,
                       const size_t len)
{
	char *dbuf;
	size_t dlen, nlen, i;

	if (dest->flags & SZL_OBJECT_RO) {
		szl_set_last_str(interp,
		                 "append to ro str",
		                 sizeof("append to ro str") - 1);
		return 0;
	}

	if (!szl_as_str(interp, dest, &dbuf, &dlen))
		return 0;

	nlen = dlen + len;
	dbuf = (char *)realloc(dbuf, nlen + 1);
	if (!dbuf)
		return 0;

	memcpy(dbuf + dlen, src, len);
	dest->val.s = dbuf;
	dest->val.s[nlen] = '\0';
	dest->val.slen = nlen;

	/* invalidate all other representations */
	if (dest->types & (1 << SZL_TYPE_WSTR))
		free(dest->val.w);

	if (dest->types & (1 << SZL_TYPE_LIST)) {
		for (i = 0; i < dest->val.llen; ++i)
			szl_unref(dest->val.items[i]);

		free(dest->val.items);
	}

	if (dest->types & (1 << SZL_TYPE_CODE))
		szl_unref(dest->val.c);

	dest->types = 1 << SZL_TYPE_STR;

	dest->flags &= ~SZL_OBJECT_HASHED;
	return 1;
}

/*
 * list operations
 */

__attribute__((nonnull(1, 2, 3)))
int szl_list_append(struct szl_interp *interp,
                    struct szl_obj *list,
                    struct szl_obj *item)
{
	struct szl_obj **items;
	size_t len;

	if (list->flags & SZL_OBJECT_RO) {
		szl_set_last_str(interp,
		                 "append to ro list",
		                 sizeof("append to ro list") - 1);
		return 0;
	}

	if (!szl_as_list(interp, list, &items, &len))
		return 0;

	items = (struct szl_obj **)realloc(items,
	                                   sizeof(struct szl_obj *) * (len + 1));
	if (!items)
		return 0;

	items[list->val.llen] = szl_ref(item);
	++list->val.llen;
	list->val.items = items;
	list->flags &= ~SZL_OBJECT_SORTED;

	/* invalidate all other representations */
	if (list->types & (1 << SZL_TYPE_STR))
		free(list->val.s);

	if (list->types & (1 << SZL_TYPE_WSTR))
		free(list->val.w);

	if (list->types & (1 << SZL_TYPE_CODE))
		szl_unref(list->val.c);

	list->types = 1 << SZL_TYPE_LIST;

	list->flags |= SZL_OBJECT_HASHED;
	return 1;
}

__attribute__((nonnull(1, 2, 3)))
int szl_list_append_str(struct szl_interp *interp,
                        struct szl_obj *list,
                        const char *buf,
                        const ssize_t len)
{
	struct szl_obj *item;
	int ret = 0;

	item = szl_new_str(buf, len);
	if (item) {
		ret = szl_list_append(interp, list, item);
		szl_unref(item);
	}

	return ret;
}

__attribute__((nonnull(1, 2)))
int szl_list_append_int(struct szl_interp *interp,
                        struct szl_obj *list,
                        const szl_int i)
{
	struct szl_obj *item;
	int ret = 0;

	item = szl_new_int(interp, i);
	if (item) {
		ret = szl_list_append(interp, list, item);
		szl_unref(item);
	}

	return ret;
}

__attribute__((nonnull(1, 2, 4)))
int szl_list_set(struct szl_interp *interp,
                 struct szl_obj *list,
                 const szl_int index,
                 struct szl_obj *item)
{
	struct szl_obj **items;
	size_t len;

	if (list->flags & SZL_OBJECT_RO) {
		szl_set_last_str(interp,
		                 "set in ro list",
		                 sizeof("set in ro list") - 1);
		return 0;
	}

	if (index < 0) {
		szl_set_last_fmt(interp, "bad index: "SZL_INT_FMT"d", index);
		return 0;
	}

	if (!szl_as_list(interp, list, &items, &len))
		return 0;

	if (index >= len) {
		szl_set_last_fmt(interp, "bad index: "SZL_INT_FMT"d", index);
		return 0;
	}

	szl_unref(items[index]);
	items[index] = szl_ref(item);

	return 1;
}

__attribute__((nonnull(1, 2, 3)))
int szl_list_extend(struct szl_interp *interp,
                    struct szl_obj *dest,
                    struct szl_obj *src)
{
	struct szl_obj **di, **si;
	size_t dlen, slen, i;

	if (dest->flags & SZL_OBJECT_RO) {
		szl_set_last_str(interp,
		                 "extend ro list",
		                 sizeof("extend ro list") - 1);
		return 0;
	}

	if (!szl_as_list(interp, dest, &di, &dlen) ||
	    !szl_as_list(interp, src, &si, &slen))
		return 0;

	if (!slen)
		return 1;

	if (dlen >= (SIZE_MAX - slen))
		return 0;

	di = (struct szl_obj **)realloc(di,
	                                sizeof(struct szl_obj *) * (dlen + slen));
	if (!di)
		return 0;

	for (i = 0; i < slen; ++i)
		di[dlen + i] = szl_ref(si[i]);

	dest->val.items = di;
	dest->val.llen += slen;
	dest->flags &= ~SZL_OBJECT_SORTED;

	/* invalidate all other representations */
	if (dest->types & (1 << SZL_TYPE_STR))
		free(dest->val.s);

	if (dest->types & (1 << SZL_TYPE_WSTR))
		free(dest->val.w);

	if (dest->types & (1 << SZL_TYPE_CODE))
		szl_unref(dest->val.c);

	dest->types = 1 << SZL_TYPE_LIST;

	dest->flags &= ~SZL_OBJECT_HASHED;
	return 1;
}

int szl_list_in(struct szl_interp *interp,
                struct szl_obj *item,
                struct szl_obj *list,
                int *in)
{
	struct szl_obj **items;
	size_t n, i;

	if (!szl_as_list(interp, list, &items, &n))
		return 0;

	for (i = 0, *in = 0; !*in && (i < n); ++i) {
		if (!szl_eq(interp, items[i], item, in))
			return SZL_ERR;
	}

	return 1;
}

__attribute__((nonnull(1, 2, 3)))
struct szl_obj *szl_join(struct szl_interp *interp,
                         struct szl_obj *delim,
                         struct szl_obj **objv,
                         const size_t objc,
                         const int wrap)
{
	struct szl_obj *obj;
	char *delims, *s, *ws;
	size_t slen, dlen, i, j, last = objc - 1;
	int mw = 0, wlen;

	if (!szl_as_str(interp, delim, &delims, &dlen))
		return NULL;

	obj = szl_new_empty();
	if (!obj)
		return NULL;

	for (i = 0; i < objc; ++i) {
		if (!szl_as_str(interp, objv[i], &s, &slen)) {
			szl_unref(obj);
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
				szl_unref(obj);
				return NULL;
			}

			if (!szl_str_append_str(interp, obj, ws, (size_t)wlen)) {
				free(ws);
				szl_unref(obj);
				return NULL;
			}

			free(ws);
		}
		else if (!szl_str_append_str(interp, obj, s, slen)) {
			szl_unref(obj);
			return NULL;
		}

		if (dlen && (i != last)) {
			if (!szl_str_append_str(interp, obj, delims, dlen)) {
				szl_unref(obj);
				return NULL;
			}
		}
	}

	return obj;
}

/*
 * dict operations
 */

static
int szl_cmp(const void *p1, const void *p2)
{
	uint32_t h1 = (*((struct szl_obj **)p1))->hash,
	         h2 = (*((struct szl_obj **)p2))->hash;

	if (h1 > h2)
		return 1;

	if (h1 < h2)
		return -1;

	return 0;
}

__attribute__((nonnull(1, 2, 3, 4)))
static
int szl_dict_get_key(struct szl_interp *interp,
                     struct szl_obj *dict,
                     struct szl_obj *k,
                     struct szl_obj ***pos)
{
	struct szl_obj **items, *p;
	size_t len, i;

	if (!szl_as_dict(interp, dict, &items, &len))
		return 0;

	if (!szl_hash(interp, k, NULL))
		return 0;

	*pos = NULL;
	if (szl_likely(len)) {
		if (!(dict->flags & SZL_OBJECT_SORTED)) {
			for (i = 0; i < len; i += 2) {
				if (!szl_hash(interp, items[i], NULL))
					return 0;
			}

			qsort(items, len / 2, sizeof(struct szl_obj *) * 2, szl_cmp);
			dict->flags |= SZL_OBJECT_SORTED;
		}

		p = k;
		*pos = (struct szl_obj **)bsearch(&p,
		                                  items,
		                                  len / 2,
		                                  sizeof(struct szl_obj *) * 2,
		                                  szl_cmp);
	}

	return 1;
}

__attribute__((nonnull(1, 2, 3, 4)))
int szl_dict_set(struct szl_interp *interp,
                 struct szl_obj *dict,
                 struct szl_obj *k,
                 struct szl_obj *v)
{
	struct szl_obj **items, **pos;
	size_t len;

	if (!szl_as_dict(interp, dict, &items, &len) ||
	    !szl_dict_get_key(interp, dict, k, &pos))
		return 0;

	if (szl_unlikely(pos != NULL)) {
		if (!szl_list_set(interp, dict, (pos - items) + 1, v))
			return 0;
	}
	else if (!szl_list_append(interp, dict, k) ||
	         !szl_list_append(interp, dict, v))
		return 0;

	szl_set_ro(k);
	return 1;
}

__attribute__((nonnull(1, 2, 3, 4)))
int szl_dict_get(struct szl_interp *interp,
                 struct szl_obj *dict,
                 struct szl_obj *k,
                 struct szl_obj **v)
{
	struct szl_obj **pos;

	if (!szl_dict_get_key(interp, dict, k, &pos))
		return 0;

	if (pos)
		*v = szl_ref(*(pos + 1));
	else
		*v = NULL;

	return 1;
}

static
struct szl_obj *szl_copy_dict(struct szl_interp *interp, struct szl_obj *dict)
{
	struct szl_obj **items, *copy;
	size_t len;

	if (!szl_as_dict(interp, dict, &items, &len))
		return NULL;

	copy = szl_new_list(items, len);
	/* if the copied dictionary is sorted, we don't want to sort it again */
	if (copy)
		copy->flags |= (dict->flags & SZL_OBJECT_SORTED);

	return copy;
}

/*
 * interpreters
 */

extern int szl_init_builtin_exts(struct szl_interp *);

static
struct szl_frame *szl_new_call(struct szl_interp *,
                               struct szl_obj *,
                               struct szl_frame *);

struct szl_interp *szl_new_interp(int argc, char *argv[])
{
	struct szl_interp *interp;
	szl_int i;

	interp = (struct szl_interp *)malloc(sizeof(*interp));
	if (!interp)
		return interp;

	interp->empty = szl_new_empty();
	if (!interp->empty) {
		free(interp);
		return NULL;
	}

	interp->space = szl_new_str(" ", 1);
	if (!interp->space) {
		szl_unref(interp->empty);
		free(interp);
		return NULL;
	}

	for (i = 0; i < sizeof(interp->nums) / sizeof(interp->nums[0]); ++i) {
		interp->nums[i] = szl_new_int_nocache(i);
		if (!interp->nums[i]) {
			for (--i; i >= 0; --i)
				szl_unref(interp->nums[i]);
			szl_unref(interp->space);
			szl_unref(interp->empty);
			free(interp);
			return NULL;
		}
		szl_set_ro(interp->nums[i]);
	}

	interp->sep = szl_new_str("/", 1);
	if (!interp->sep) {
		for (--i; i >= 0; --i)
			szl_unref(interp->nums[i]);
		szl_unref(interp->space);
		szl_unref(interp->empty);
		free(interp);
		return NULL;
	}

	interp->_ = szl_new_str(SZL_LAST_NAME, sizeof(SZL_LAST_NAME) - 1);
	if (!interp->_) {
		szl_unref(interp->sep);
		for (--i; i >= 0; --i)
			szl_unref(interp->nums[i]);
		szl_unref(interp->space);
		szl_unref(interp->empty);
		free(interp);
		return NULL;
	}

	interp->depth = 0;
	interp->global = szl_new_call(interp, interp->empty, NULL);
	if (!interp->global) {
		szl_unref(interp->_);
		szl_unref(interp->sep);
		for (--i; i >= 0; --i)
			szl_unref(interp->nums[i]);
		szl_unref(interp->space);
		szl_unref(interp->empty);
		free(interp);
		return NULL;
	}

	interp->args = szl_new_str(SZL_OBJV_NAME, sizeof(SZL_OBJV_NAME) - 1);
	if (!interp->args) {
		szl_pop_call(interp);
		szl_unref(interp->_);
		szl_unref(interp->sep);
		for (--i; i >= 0; --i)
			szl_unref(interp->nums[i]);
		szl_unref(interp->space);
		szl_unref(interp->empty);
		free(interp);
		return NULL;
	}

	interp->priv = szl_new_str(SZL_PRIV_NAME, sizeof(SZL_PRIV_NAME) - 1);
	if (!interp->args) {
		szl_unref(interp->args);
		szl_pop_call(interp);
		szl_unref(interp->_);
		szl_unref(interp->sep);
		for (--i; i >= 0; --i)
			szl_unref(interp->nums[i]);
		szl_unref(interp->space);
		szl_unref(interp->empty);
		free(interp);
		return NULL;
	}

	interp->exts = szl_new_list(NULL, 0);
	if (!interp->exts) {
		szl_unref(interp->priv);
		szl_unref(interp->args);
		szl_pop_call(interp);
		szl_unref(interp->_);
		szl_unref(interp->sep);
		for (--i; i >= 0; --i)
			szl_unref(interp->nums[i]);
		szl_unref(interp->space);
		szl_unref(interp->empty);
		free(interp);
		return NULL;
	}

	interp->libs = szl_new_list(NULL, 0);
	if (!interp->libs) {
		szl_unref(interp->exts);
		szl_unref(interp->priv);
		szl_unref(interp->args);
		szl_pop_call(interp);
		szl_unref(interp->_);
		szl_unref(interp->sep);
		for (--i; i >= 0; --i)
			szl_unref(interp->nums[i]);
		szl_unref(interp->space);
		szl_unref(interp->empty);
		free(interp);
		return NULL;
	}

	interp->last = szl_ref(interp->empty);

	szl_set_ro(interp->empty);
	szl_set_ro(interp->space);
	szl_set_ro(interp->sep);
	szl_set_ro(interp->args);

	interp->seed = (unsigned int)(time(NULL) % UINT_MAX);

	if (!szl_init_builtin_exts(interp)) {
		szl_free_interp(interp);
		return NULL;
	}

	for (i = 0; i < argc; ++i) {
		if (!szl_list_append_str(interp,
		                         interp->global->args,
		                         argv[i], -1)) {
			szl_free_interp(interp);
			return NULL;
		}
	}

	if (!szl_set_args(interp, interp->global, interp->global->args) ||
	    !szl_set(interp, interp->global, interp->_, interp->empty)) {
		szl_free_interp(interp);
		return NULL;
	}

	return interp;
}

__attribute__((nonnull(1)))
void szl_free_interp(struct szl_interp *interp)
{
	unsigned int i;

	szl_unref(interp->last);
	szl_unref(interp->exts);
	szl_unref(interp->priv);
	szl_unref(interp->args);
	szl_pop_call(interp);
	szl_unref(interp->_);
	szl_unref(interp->sep);
	for (i = 0; i < sizeof(interp->nums) / sizeof(interp->nums[0]); ++i)
		szl_unref(interp->nums[i]);
	szl_unref(interp->space);
	szl_unref(interp->empty);

	/* must happen after we free all other objects, in case a destructor is a
	 * shared object function */
	szl_unref(interp->libs);

	free(interp);
}

/*
 * return value assignment
 */

__attribute__((nonnull(1, 2)))
enum szl_res szl_set_last(struct szl_interp *interp, struct szl_obj *obj)
{
	szl_unref(interp->last);
	interp->last = obj;
	return SZL_OK;
}

__attribute__((nonnull(1)))
void szl_empty_last(struct szl_interp *interp)
{
	szl_set_last(interp, szl_ref(interp->empty));
}

__attribute__((nonnull(1)))
enum szl_res szl_set_last_int(struct szl_interp *interp, const szl_int i)
{
	struct szl_obj *obj;

	obj = szl_new_int(interp, i);
	if (obj)
		return szl_set_last(interp, obj);

	return SZL_ERR;
}

__attribute__((nonnull(1)))
enum szl_res szl_set_last_float(struct szl_interp *interp, const szl_float f)
{
	struct szl_obj *obj;

	obj = szl_new_float(f);
	if (obj)
		return szl_set_last(interp, obj);

	return SZL_ERR;
}

__attribute__((nonnull(1)))
enum szl_res szl_set_last_bool(struct szl_interp *interp, const int b)
{
	if (b)
		return szl_set_last(interp, szl_ref(interp->nums[1]));

	return szl_set_last(interp, szl_ref(interp->nums[0]));
}

__attribute__((nonnull(1, 2)))
enum szl_res szl_set_last_str(struct szl_interp *interp,
                              const char *s,
                              const ssize_t len)
{
	struct szl_obj *str;

	str = szl_new_str(s, len);
	if (!str)
		return SZL_ERR;

	return szl_set_last(interp, str);
}

__attribute__((nonnull(1, 2)))
enum szl_res szl_set_last_wstr(struct szl_interp *interp,
                               const wchar_t *ws,
                               const ssize_t len)
{
	struct szl_obj *wstr;

	wstr = szl_new_wstr(ws, len);
	if (!wstr)
		return SZL_ERR;

	return szl_set_last(interp, wstr);
}

__attribute__((nonnull(1, 2)))
enum szl_res szl_set_last_fmt(struct szl_interp *interp,
                              const char *fmt,
                              ...)
{
	struct szl_obj *str;
	va_list ap;

	va_start(ap, fmt);
	str = szl_new_str_va(fmt, ap);
	va_end(ap);

	if (str) {
		szl_set_last(interp, str);
		return SZL_OK;
	}

	return SZL_ERR;
}

__attribute__((nonnull(1, 2)))
enum szl_res szl_set_last_help(struct szl_interp *interp,
                               struct szl_obj *proc)
{
	char *s;

	if (szl_as_str(interp, proc, &s, NULL)) {
		if (proc->help)
			szl_set_last_fmt(interp,
			                 "bad usage, should be '%s %s'",
			                 s,
			                 proc->help);
		else
			szl_set_last_fmt(interp, "bad usage, should be '%s'", s);
	}
	else
		szl_set_last_str(interp, "bad usage", sizeof("bad usage") - 1);

	return SZL_ERR;
}

/*
 * variables
 */

__attribute__((nonnull(1, 2, 3)))
int szl_get(struct szl_interp *interp,
            struct szl_obj *name,
            struct szl_obj **obj)
{
	char *s;

	/* try the current frame's locals first */
	if (!szl_dict_get(interp, interp->current->locals, name, obj))
		return 0;

	/* then, fall back to the global frame */
	if (!*obj &&
	    (interp->current->caller != interp->global) &&
		!szl_dict_get(interp, interp->global->locals, name, obj))
		return 0;

	if (*obj)
		return 1;

	if (szl_as_str(interp, name, &s, NULL))
		szl_set_last_fmt(interp, "no such obj: %s", s);

	*obj = NULL;
	return 1;
}

int szl_get_byname(struct szl_interp *interp,
                   const char *name,
                   struct szl_obj **obj)
{
	struct szl_obj *nameo;
	int ret;

	if (name[0]) {
		nameo = szl_new_str(name, -1);
		if (!nameo)
			return SZL_ERR;

		ret = szl_get(interp, nameo, obj);
		szl_unref(nameo);
		return ret;
	}

	szl_set_last_str(interp, "empty obj name", sizeof("empty obj name") - 1);
	*obj = NULL;
	return 1;
}

/*
 * parsing and evaluation
 */

__attribute__((nonnull(1, 2)))
enum szl_res szl_eval(struct szl_interp *interp, struct szl_obj *obj)
{
	struct szl_obj *out;
	char *s2, *start, *end;
	size_t len, rlen;
	enum szl_res res;

	szl_empty_last(interp);

	if (!szl_len(interp, obj, &len))
		return SZL_ERR;
	if (!len)
		return SZL_OK;

	s2 = szl_strdup(interp, obj, &len);
	if (!s2)
		return SZL_ERR;

	/* locate the expression, by skipping all trailing and leading whitespace */
	szl_trim(s2, &start, &end);

	rlen = end - start;
	if (!rlen) {
		free(s2);
		return SZL_OK;
	}

	/* if the expression is wrapped with braces, strip them */
	if ((start[0] == '{') && (*(end - 1) == '}')) {
		*(end - 1) = '\0';
		res = szl_set_last_str(interp, start + 1, rlen - 2);
	}

	/* if the expression is wrapped with brackets, strip them, call it and pass
	 * the return value */
	else if ((start[0] == '[') && (*(end - 1) == ']')) {
		*(end - 1) = '\0';
		out = szl_new_str(start + 1, rlen - 2);
		if (out) {
			res = szl_run_stmt(interp, out);
			szl_unref(out);
		} else
			res = SZL_ERR;
	}

	/* if the expression starts with $, the rest is a variable name */
	else if (start[0] == '$') {
		if (szl_get_byname(interp, start + 1, &out) && out)
			res = szl_set_last(interp, out);
		else
			res = SZL_ERR;
	}

	/* otherwise, treat it as a string literal */
	else {
		if (len == rlen) {
			szl_set_last(interp, szl_ref(obj));
			res = SZL_OK;
		}
		else
			res = szl_set_last_str(interp, start, rlen);
	}

	free(s2);
	return res;
}

__attribute__((nonnull(1, 2)))
struct szl_obj *szl_parse_stmts(struct szl_interp *interp,
                                const char *buf,
                                const size_t len)
{
	struct szl_obj *stmts, *stmt;
	char *copy, *line, *next = NULL;
	const char *fmt = NULL;
	size_t i = 0;
	int nbraces = 0, nbrackets = 0;

	copy = strndup(buf, len);
	if (!copy)
		return NULL;

	stmts = szl_new_list(NULL, 0);
	if (!stmts) {
		free(copy);
		return stmts;
	}

	line = copy;
	while (i < len) {
		if (copy[i] == '{')
			++nbraces;
		else if (copy[i] == '}')
			--nbraces;
		else if (copy[i] == '[')
			++nbrackets;
		else if (copy[i] == ']')
			--nbrackets;

		if (i == len - 1) {
			if (nbraces != 0)
				fmt = "unbalanced {}: %s\n";
			else if (nbrackets != 0)
				fmt = "unbalanced []: %s\n";

			if (fmt) {
				szl_set_last_fmt(interp, fmt, buf);
				szl_unref(stmts);
				free(copy);
				return NULL;
			}
		}
		else if ((copy[i] != '\n') || (nbraces != 0) || (nbrackets != 0)) {
			++i;
			continue;
		}

		if (copy[i] == '\n')
			copy[i] = '\0';
		next = &copy[i + 1];

		/* if the line contains nothing but whitespace, skip it */
		for (; szl_isspace(*line); ++line);
		if (line[0] != '\0' && line[0] != SZL_COMMENT_PFIX) {
			stmt = szl_new_str(line, next - line);
			if (!stmt) {
				szl_unref(stmts);
				free(copy);
				return NULL;
			}

			if (!szl_list_append(interp, stmts, stmt)) {
				szl_unref(stmt);
				szl_unref(stmts);
				free(copy);
				return NULL;
			}

			szl_unref(stmt);
		}

		line = next;
		++i;
	}

	free(copy);
	return stmts;
}

__attribute__((nonnull(1, 2)))
enum szl_res szl_run_stmt(struct szl_interp *interp, struct szl_obj *stmt)
{
	struct szl_obj **toks, **objv;
	struct szl_frame *call;
	size_t len, i, objc = SZL_ERR;
	enum szl_res res;

	szl_empty_last(interp);

	if (interp->depth == SZL_MAX_NESTING) {
		szl_set_last_str(interp,
		                 "reached nesting limit",
		                 sizeof("reached nesting limit") - 1);
		return SZL_ERR;
	}

	if (!szl_as_list(interp, stmt, &toks, &len) || (len > UINT_MAX))
		return SZL_ERR;

	/* create a new frame */
	call = szl_new_call(interp, stmt, interp->current);
	if (!call)
		return SZL_ERR;

	/* evaluate each token */
	for (i = 0; i < len; ++i) {
		res = szl_eval(interp, toks[i]);
		if (res != SZL_OK) {
			szl_pop_call(interp);
			return res;
		}

		if (!szl_list_append(interp, call->args, interp->last)) {
			szl_pop_call(interp);
			return SZL_ERR;
		}
	}

	if (!szl_as_list(interp, call->args, &objv, &objc)) {
		szl_pop_call(interp);
		return SZL_ERR;
	}

	if (((objv[0]->min_objc != -1) && (objc < (size_t)objv[0]->min_objc)) ||
	    ((objv[0]->max_objc != -1) && (objc > (size_t)objv[0]->max_objc))) {
		szl_set_last_help(interp, objv[0]);
		szl_pop_call(interp);
		return SZL_ERR;
	}

	/* clear the last return value: it was modified during argument
	 * evaluation */
	szl_empty_last(interp);

	/* call the procedure */
	res = objv[0]->proc(interp, (unsigned int)objc, objv);

	/* update the object holding the last return value */
	if (!szl_set(interp, call->caller, interp->_, interp->last))
		res = SZL_ERR;

	szl_pop_call(interp);

	return res;
}

__attribute__((nonnull(1, 2)))
enum szl_res szl_run(struct szl_interp *interp,
                     const char *buf,
                     const size_t len)
{
	struct szl_obj *obj;
	enum szl_res res = SZL_ERR;

	obj = szl_new_str(buf, (ssize_t)len);
	if (obj) {
		res = szl_run_obj(interp, obj);
		szl_unref(obj);
	}

	return res;
}

__attribute__((nonnull(1, 2)))
enum szl_res szl_run_obj(struct szl_interp *interp, struct szl_obj *obj)
{
	struct szl_obj **stmts;
	size_t len, i;
	enum szl_res res = SZL_ERR;

	if (!szl_as_code(interp, obj, &stmts, &len))
		return SZL_ERR;

	for (i = 0; i < len; ++i) {
		res = szl_run_stmt(interp, stmts[i]);
		if (res != SZL_OK)
			break;
	}

	return res;
}

/*
 * extensions
 */

__attribute__((nonnull(1, 2, 3)))
int szl_new_ext(struct szl_interp *interp,
                const char *name,
                const struct szl_ext_export *exports,
                const unsigned int n)
{
	struct szl_obj *ext, *obj_name, *val;
	unsigned int i;

	ext = szl_new_str(name, -1);
	if (!ext)
		return 0;

	for (i = 0; i < n; ++i) {
		obj_name = szl_new_str(exports[i].name, -1);
		if (!obj_name) {
			szl_unref(ext);
			return 0;
		}

		switch (exports[i].type) {
			case SZL_TYPE_PROC:
				val = szl_new_proc(interp,
				                   obj_name,
				                   exports[i].val.proc.min_objc,
				                   exports[i].val.proc.max_objc,
				                   exports[i].val.proc.help,
				                   exports[i].val.proc.proc,
				                   exports[i].val.proc.del,
				                   exports[i].val.proc.priv);
				break;

			case SZL_TYPE_STR:
				val = szl_new_str(exports[i].val.s.buf, exports[i].val.s.len);
				break;

			case SZL_TYPE_INT:
				val = szl_new_int(interp, exports[i].val.i);
				break;

			case SZL_TYPE_FLOAT:
				val = szl_new_float(exports[i].val.f);
				break;

			default:
				szl_unref(obj_name);
				szl_unref(ext);
				return 0;
		}

		if (!val) {
			szl_unref(obj_name);
			szl_unref(ext);
			return 0;
		}

		if (!szl_set(interp, interp->global, obj_name, val)) {
			szl_unref(val);
			szl_unref(obj_name);
			szl_unref(ext);
			return 0;
		}

		szl_unref(val);
		szl_unref(obj_name);
	}

	if (!szl_list_append(interp, interp->exts, ext)) {
		szl_unref(ext);
		return 0;
	}

	szl_unref(ext);
	return 1;
}

void szl_unload(void *h)
{
	dlclose(h);
}

extern char *szl_builtin_exts[];

__attribute__((nonnull(1, 2)))
int szl_load(struct szl_interp *interp, const char *name)
{
	char *path;
	char **builtin, init_name[SZL_MAX_EXT_INIT_FUNC_NAME_LEN + 1];
	struct szl_obj *lib, *last;
	struct szl_frame *current = interp->current;
	szl_ext_init init;
	int res, loaded;

	for (builtin = szl_builtin_exts; *builtin; ++builtin) {
		if (strcmp(*builtin, name) == 0)
			return 1;
	}

	lib = szl_new_str_fmt(SZL_EXT_PATH_FMT, name);
	if (!lib)
		return 0;

	if (!szl_list_in(interp, lib, interp->libs, &loaded)) {
		szl_unref(lib);
		return 0;
	}

	if (loaded) {
		szl_unref(lib);
		return 1;
	}

	if (!szl_as_str(interp, lib, &path, NULL)) {
		szl_unref(lib);
		return 0;
	}

	lib->priv = dlopen(path, RTLD_LAZY);
	if (!lib->priv) {
		szl_set_last_fmt(interp, "failed to load %s", path);
		szl_unref(lib);
		return 0;
	}

	lib->del = szl_unload;

	snprintf(init_name, sizeof(init_name), SZL_EXT_INIT_FUNC_NAME_FMT, name);
	*((void **)&init) = dlsym(lib->priv, init_name);
	if (!init) {
		szl_set_last_fmt(interp, "failed to locate %s", init_name);
		szl_unref(lib);
		return 0;
	}

	if (!szl_list_append(interp, interp->libs, lib)) {
		szl_unref(lib);
		return 0;
	}
	szl_unref(lib);

	/* run the extension initializer - if it evaluates code or creates
	 * procedures, do this in the global scope and don't let it change _ */
	last = szl_ref(interp->last);
	interp->current = interp->global;
	res = init(interp);
	interp->current = current;
	szl_set_last(interp, last);

	return res;
}

__attribute__((nonnull(1, 2)))
enum szl_res szl_source(struct szl_interp *interp, const char *path)
{
	struct stat stbuf;
	struct szl_frame *current = interp->current;
	char *buf;
	ssize_t len;
	int fd;
	enum szl_res res;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		szl_set_last_fmt(interp, "failed to open %s", path);
		return SZL_ERR;
	}

	if (fstat(fd, &stbuf) < 0) {
		close(fd);
		szl_set_last_fmt(interp, "failed to stat %s", path);
		return SZL_ERR;
	}

	if (stbuf.st_size == (off_t)SIZE_MAX) {
		close(fd);
		return SZL_ERR;
	}

	buf = (char *)malloc(stbuf.st_size + 1);
	if (!buf) {
		close(fd);
		return SZL_ERR;
	}

	len = read(fd, buf, (size_t)stbuf.st_size);
	if (len <= 0) {
		free(buf);
		close(fd);
		szl_set_last_fmt(interp, "failed to read %s", path);
		return SZL_ERR;
	}
	buf[len] = '\0';

	close(fd);

	/* run the script in the global scope */
	interp->current = interp->global;
	res = szl_run(interp, buf, (size_t)len);
	interp->current = current;

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
	int more = 1;

	if (!strm->ops->read) {
		szl_set_last_str(interp,
		                 "read from unsupported stream",
		                 sizeof("read from unsupported stream") - 1);
		return SZL_ERR;
	}

	if (strm->closed)
		return SZL_OK;

	buf = (unsigned char *)malloc(len + 1);
	if (!buf)
		return SZL_ERR;

	out = strm->ops->read(interp, strm->priv, buf, len, &more);
	if (out > 0) {
		buf[out] = '\0';
		obj = szl_new_str_noalloc((char *)buf, (size_t)out);
		if (!obj) {
			free(buf);
			return SZL_ERR;
		}
		szl_set_last(interp, obj);
	}
	else {
		free(buf);

		if (out < 0)
			return SZL_ERR;
		else if (!more) {
			szl_set_last_str(interp,
			                 "read from closed stream",
			                 sizeof("read from closed stream") - 1);
			return SZL_ERR;
		}
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
	int cont = 1;

	if (!strm->ops->read) {
		szl_set_last_str(interp,
		                 "read from unsupported stream",
		                 sizeof("read from unsupported stream") - 1);
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

	tot = strm->ops->read(interp, strm->priv, buf, len, &cont);
	if (tot > 0) {
		/* if we don't know whether there's more data to read, read more data in
		 * SZL_STREAM_BUFSIZ-byte chunks until the read() callback returns 0 */
		if (cont && !strm->ops->size) {
			do {
				len += SZL_STREAM_BUFSIZ;
				nbuf = (unsigned char *)realloc(buf, len);
				if (!nbuf) {
					free(buf);
					return SZL_ERR;
				}
				buf = nbuf;

				cont = 1;
				more = strm->ops->read(interp,
				                       strm->priv,
				                       buf + tot,
				                       SZL_STREAM_BUFSIZ,
				                       &cont);
				if (more < 0) {
					free(buf);
					return SZL_ERR;
				}
				else if (more)
					tot += more;
				else if (!strm->blocking || !cont)
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

		return szl_set_last(interp, obj);
	} else if (!cont)
		szl_set_last_str(interp,
		                 "read from closed stream",
		                 sizeof("read from closed stream") - 1);

	free(buf);
	return (tot < 0) ? SZL_ERR : SZL_OK;
}

static
enum szl_res szl_stream_readln(struct szl_interp *interp,
                               struct szl_stream *strm)
{
	struct szl_obj *obj;
	unsigned char *buf = NULL, *pos, *mbuf;
	size_t buflen = SZL_STREAM_BUFSIZ, tot = 0;
	ssize_t out;
	int more;

	if (!strm->ops->read) {
		szl_set_last_str(interp,
		                 "read from unsupported stream",
		                 sizeof("read from unsupported stream") - 1);
		return SZL_ERR;
	}

	if (strm->closed)
		return SZL_OK;

	buf = (unsigned char *)malloc(buflen + 1);
	if (!buf)
		return SZL_ERR;

	do {
		more = 1;
		pos = buf + tot;
		out = strm->ops->read(interp, strm->priv, pos, 1, &more);
		if (out < 0) {
			free(buf);
			return SZL_ERR;
		}
		tot += out;

		if (!out || !more || (*pos == '\n'))
			break;

		if (tot > buflen) {
			buflen += SZL_STREAM_BUFSIZ;
			mbuf = (unsigned char *)realloc(buf, buflen + 1);
			if (!mbuf) {
				free(buf);
				return SZL_ERR;
			}
		}
	} while (1);

	buf[tot] = '\0';
	obj = szl_new_str_noalloc((char *)buf, tot);
	if (!obj) {
		free(buf);
		return SZL_ERR;
	}

	return szl_set_last(interp, obj);
}

static
enum szl_res szl_stream_write(struct szl_interp *interp,
                              struct szl_stream *strm,
                              const unsigned char *buf,
                              const size_t len)
{
	ssize_t out, chunk;

	if (!strm->ops->write) {
		szl_set_last_str(interp,
		                 "write to unsupported stream",
		                 sizeof("write to unsupported stream") - 1);
		return SZL_ERR;
	}

	if (strm->closed) {
		szl_set_last_str(interp,
		                 "write to closed stream",
		                 sizeof("write to closed stream") - 1);
		return SZL_ERR;
	}

	out = 0;
	while ((size_t)out < len) {
		chunk = strm->ops->write(interp, strm->priv, buf + out, len - out);
		if (chunk < 0)
			return SZL_ERR;

		if (!chunk)
			break;

		out += chunk;
	}

	if (strm->blocking && ((size_t)out != len))
		return SZL_ERR;

	return szl_set_last_int(interp, (szl_int)out);
}

static
enum szl_res szl_stream_writeln(struct szl_interp *interp,
                                struct szl_stream *strm,
                                const unsigned char *buf,
                                const size_t len)
{
	unsigned char *buf2;
	size_t mlen;
	enum szl_res res;

	if (!len)
		res = szl_stream_write(interp, strm, (unsigned char *)"\n", 1);
	else if (buf[len - 1] != '\n') {
		mlen = len + 1;
		buf2 = (unsigned char *)malloc(mlen);
		if (!buf2)
			return SZL_ERR;

		memcpy(buf2, buf, len);
		buf2[len] = '\n';
		res = szl_stream_write(interp, strm, buf2, mlen);
		free(buf2);
	}
	else
		res = szl_stream_write(interp, strm, buf, len);

	return res;
}

static
enum szl_res szl_stream_flush(struct szl_interp *interp,
                              struct szl_stream *strm)
{
	if (!strm->ops->flush)
		return SZL_OK;

	if (strm->closed) {
		szl_set_last_str(interp,
		                 "flush of closed stream",
		                 sizeof("flush of closed stream") - 1);
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

__attribute__((nonnull(1)))
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
		return szl_ref(interp->empty);

	return szl_new_int(interp, strm->ops->handle(strm->priv));
}

static
enum szl_res szl_stream_accept(struct szl_interp *interp,
                               struct szl_stream *strm)
{
	struct szl_obj *csock, *list;
	struct szl_stream *cstrm;

	if (!strm->ops->accept) {
		szl_set_last_str(interp,
		                 "accept from unsupported stream",
		                 sizeof("accept from unsupported stream") - 1);
		return SZL_ERR;
	}

	if (strm->closed) {
		szl_set_last_str(interp,
		                 "accept from closed stream",
		                 sizeof("accept from closed stream") - 1);
		return SZL_ERR;
	}

	list = szl_new_list(NULL, 0);
	if (!list)
		return SZL_ERR;

	do {
		if (!strm->ops->accept(interp, strm->priv, &cstrm)) {
			szl_unref(list);
			return SZL_ERR;
		}

		if (!cstrm)
			break;

		csock = szl_new_stream(interp, cstrm, "stream.client");
		if (!csock) {
			szl_stream_free(cstrm);
			szl_unref(list);
			return SZL_ERR;
		}

		if (!szl_list_append(interp, list, csock)) {
			szl_unref(csock);
			szl_unref(list);
			return SZL_ERR;
		}

		szl_unref(csock);
	} while (!strm->blocking);

	return szl_set_last(interp, list);
}

static
enum szl_res szl_stream_unblock(struct szl_interp *interp,
                                struct szl_stream *strm)
{
	enum szl_res res;

	if (!strm->ops->unblock) {
		szl_set_last_str(interp,
		                 "unblock on unsupported stream",
		                 sizeof("unblock on unsupported stream") - 1);
		return SZL_ERR;
	}

	if (strm->closed) {
		szl_set_last_str(interp,
		                 "unblock on closed stream",
		                 sizeof("unblock on closed stream") -1);
		return SZL_ERR;
	}

	res = strm->ops->unblock(strm->priv);
	if (res == SZL_OK)
		strm->blocking = 0;

	return res;
}

static
enum szl_res szl_stream_rewind(struct szl_interp *interp,
                               struct szl_stream *strm)
{
	if (!strm->ops->rewind) {
		szl_set_last_str(interp,
		                 "rewind on unsupported stream",
		                 sizeof("rewind on unsupported stream") - 1);
		return SZL_ERR;
	}

	if (strm->closed) {
		szl_set_last_str(interp,
		                 "rewind on closed stream",
		                 sizeof("rewind on closed stream") -1);
		return SZL_ERR;
	}

	return strm->ops->rewind(strm->priv);
}

static
enum szl_res szl_stream_setopt(struct szl_interp *interp,
                               struct szl_stream *strm,
                               struct szl_obj *opt,
                               struct szl_obj *val)
{
	if (!strm->ops->setopt) {
		szl_set_last_str(interp,
		                 "setopt on unsupported stream",
		                 sizeof("setopt on unsupported stream") - 1);
		return SZL_ERR;
	}

	if (strm->closed) {
		szl_set_last_str(interp,
		                 "setopt on closed stream",
		                 sizeof("setopt on closed stream") -1);
		return SZL_ERR;
	}

	return strm->ops->setopt(interp, strm->priv, opt, val) ? SZL_OK : SZL_ERR;
}

enum szl_res szl_stream_proc(struct szl_interp *interp,
                             const unsigned int objc,
                             struct szl_obj **objv)
{
	struct szl_stream *strm = (struct szl_stream *)objv[0]->priv;
	struct szl_obj *obj;
	char *op, *buf;
	szl_int req;
	size_t len;

	if (objc == 3) {
		if (!szl_as_str(interp, objv[1], &op, NULL))
			return SZL_ERR;

		if (strcmp("read", op) == 0) {
			if (!szl_as_int(interp, objv[2], &req))
				return SZL_ERR;

			if (!req)
				return SZL_OK;

			if (req > SSIZE_MAX)
				req = SSIZE_MAX;

			return szl_stream_read(interp, strm, (size_t)req);
		}
		else if (strcmp("write", op) == 0) {
			if (!szl_as_str(interp, objv[2], &buf, &len) || (len > SSIZE_MAX))
				return SZL_ERR;

			if (!len)
				return SZL_OK;

			return szl_stream_write(interp,
			                        strm,
			                        (const unsigned char *)buf,
			                        len);
		}
		else if (strcmp("writeln", op) == 0) {
			if (!szl_as_str(interp, objv[2], &buf, &len) || (len > SSIZE_MAX))
				return SZL_ERR;

			return szl_stream_writeln(interp,
			                          strm,
			                          (const unsigned char *)buf,
			                          len);
		}
	}
	else if (objc == 2) {
		if (!szl_as_str(interp, objv[1], &op, NULL))
			return SZL_ERR;

		if (strcmp("read", op) == 0)
			return szl_stream_read_all(interp, strm);
		if (strcmp("readln", op) == 0)
			return szl_stream_readln(interp, strm);
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
			return szl_set_last(interp, obj);
		}
		else if (strcmp("rewind", op) == 0)
			return szl_stream_rewind(interp, strm);
		else if (strcmp("unblock", op) == 0)
			return szl_stream_unblock(interp, strm);
	}
	else if (objc == 4) {
		if (!szl_as_str(interp, objv[1], &op, NULL))
			return SZL_ERR;

		if (strcmp("setopt", op) == 0)
			return szl_stream_setopt(interp, strm, objv[2], objv[3]);
	}

	return szl_set_last_help(interp, objv[0]);
}

__attribute__((nonnull(1)))
void szl_stream_del(void *priv)
{
	szl_stream_close((struct szl_stream *)priv);
	free(priv);
}

__attribute__((nonnull(1, 2, 3)))
struct szl_obj *szl_new_stream(struct szl_interp *interp,
                               struct szl_stream *strm,
                               const char *type)
{
	struct szl_obj *name, *proc;

	name = szl_new_str_fmt("%s:%"PRIxPTR, type, (uintptr_t)strm->priv);
	if (!name)
		return name;

	proc = szl_new_proc(interp,
	                    name,
	                    2,
	                    4,
	                    SZL_STREAM_HELP,
	                    szl_stream_proc,
	                    szl_stream_del,
	                    strm);
	szl_unref(name);
	return proc;
}
