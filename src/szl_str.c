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
#include <ctype.h>

#include "szl.h"

#define SZL_FORMAT_SEQ "{}"

static
enum szl_res szl_str_proc_length(struct szl_interp *interp,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	size_t len;

	if (!szl_len(interp, objv[1], &len))
		return SZL_ERR;

	return szl_set_last_int(interp, (szl_int)len);
}

static
enum szl_res szl_str_proc_in(struct szl_interp *interp,
                             const unsigned int objc,
                             struct szl_obj **objv)
{
	char *s, *sub;
	size_t len;

	if (!szl_as_str(interp, objv[1], &s, NULL) ||
	    !szl_as_str(interp, objv[2], &sub, &len))
		return SZL_ERR;

	if (!len) {
		szl_set_last_str(interp, "empty substr", sizeof("empty substr") - 1);
		return SZL_ERR;
	}

	return szl_set_last_bool(interp, strstr(s, sub) ? 1 : 0);
}

static
enum szl_res szl_str_proc_count(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	char *s, *sub;
	const char *pos, *oc, *end;
	size_t ls, lsub;
	szl_int n;

	if (!szl_as_str(interp, objv[1], &s, &ls) ||
	    !szl_as_str(interp, objv[2], &sub, &lsub))
		return SZL_ERR;

	if (!lsub) {
		szl_set_last_str(interp, "empty substr", sizeof("empty substr") - 1);
		return SZL_ERR;
	}

	n = 0;
	if (ls) {
		pos = s;
		end = s + ls;

		do {
			oc = strstr(pos, sub);
			if (!oc)
				break;

			pos = oc + lsub;
			++n;
		} while (pos < end);
	}

	return szl_set_last_int(interp, n);
}

static
enum szl_res szl_str_proc_range(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	char *s;
	szl_int start, end;
	size_t len;

	if (!szl_as_str(interp, objv[1], &s, &len))
		return SZL_ERR;

	if (!szl_as_int(interp, objv[2], &start) ||
	    !szl_as_int(interp, objv[3], &end))
		return SZL_ERR;

	if ((start < 0) || (start >= len)) {
		szl_set_last_fmt(interp, "bad start: "SZL_INT_FMT, start);
		return SZL_ERR;
	}

	if ((end < start) || (end >= len)) {
		szl_set_last_fmt(interp, "bad end: "SZL_INT_FMT, end);
		return SZL_ERR;
	}

	return szl_set_last_str(interp, s + start, end - start + 1);
}

static
enum szl_res szl_str_proc_tail(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	char *s;
	szl_int n = 1;
	size_t len;

	if (((objc == 3) && !szl_as_int(interp, objv[2], &n)) ||
	    (n <= 0) ||
	    (n >= SIZE_MAX) ||
	    !szl_as_str(interp, objv[1], &s, &len) ||
	    (n > len))
		return SZL_ERR;

	return szl_set_last_str(interp, s + len - n, n);
}

static
enum szl_res szl_str_proc_append(struct szl_interp *interp,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	return szl_str_append(interp, objv[1], objv[2]) ? SZL_OK : SZL_ERR;
}

static
enum szl_res szl_str_proc_split(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	struct szl_obj *list;
	char *s, *delim, *tok, *next;
	size_t slen, dlen, i;

	if (!szl_as_str(interp, objv[1], &s, &slen) ||
	    !szl_as_str(interp, objv[2], &delim, &dlen) ||
	    (dlen > (INT_MAX - 1)))
		return SZL_ERR;

	if (!slen)
		return SZL_OK;

	list = szl_new_list(NULL, 0);
	if (!list)
		return SZL_ERR;

	if (dlen) {
		tok = s;
		next = strstr(s, delim);
		do {
			if (next) {
				next += dlen;
				if (!szl_list_append_str(interp,
				                         list,
				                         tok,
				                         (next - tok) - dlen)) {
					szl_unref(list);
					return SZL_ERR;
				}
			}
			else {
				if (!szl_list_append_str(interp, list, tok, slen - (tok - s))) {
					szl_unref(list);
					return SZL_ERR;
				}
				break;
			}

			tok = next;
			next = strstr(next, delim);
		} while (1);
	}
	else {
		for (i = 0; i < slen; ++i) {
			if (!szl_list_append_str(interp, list, &s[i], 1)) {
				szl_unref(list);
				return SZL_ERR;
			}
		}
	}

	return szl_set_last(interp, list);
}

static
enum szl_res szl_str_proc_join(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	struct szl_obj *obj;

	obj = szl_join(interp, objv[1], &objv[2], (size_t)objc - 2, 0);
	if (obj)
		return szl_set_last(interp, obj);

	return SZL_ERR;
}

static
enum szl_res szl_str_proc_expand(struct szl_interp *interp,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	struct szl_obj *str;
	char *s, *s2;
	size_t len, i, end, out;

	if (!szl_as_str(interp, objv[1], &s, &len))
		return SZL_ERR;

	s2 = (char *)malloc(len + 1);
	if (!s2)
		return SZL_ERR;

	i = 0;
	out = 0;
	end = len - 1;
	while (i < len) {
		if (s[i] != '\\') {
			s2[out] = s[i];
			++i;
			++out;
		}
		else {
			if (len == end) {
				free(s2);
				szl_set_last_str(interp,
				                 "bad escape",
				                 sizeof("bad escape") - 1);
				return SZL_ERR;
			}

			switch (s[i + 1]) {
				case '0':
					s2[out] = '\0';
					i += 2;
					break;

				case '\\':
					s2[out] = '\\';
					i += 2;
					break;

				case 'n':
					s2[out] = '\n';
					i += 2;
					break;

				case 't':
					s2[out] = '\t';
					i += 2;
					break;

				case 'r':
					s2[out] = '\r';
					i += 2;
					break;

				case '[':
					s2[out] = '[';
					i += 2;
					break;

				case ']':
					s2[out] = ']';
					i += 2;

				case 'x':
					if (i + 3 > end) {
						free(s2);
						szl_set_last_str(interp,
						                 "bad hex escape",
						                 sizeof("bad hex escape") - 1);
						return SZL_ERR;
					}

					if ((s[i + 2] >= '0') && (s[i + 2] <= '9'))
						s2[out] = ((s[i + 2] - '0') << 4);
					else if ((s[i + 2] >= 'a') && (s[i + 2] <= 'f'))
						s2[out] = ((s[i + 2] - 'a' + 10) << 4);
					else if ((s[i + 2] >= 'A') && (s[i + 2] <= 'F'))
						s2[out] = ((s[i + 2] - 'A' + 10) << 4);
					else {
						free(s2);
						szl_set_last_fmt(interp,
						                   "bad hex digit: %c",
						                   s[i + 2]);
						return SZL_ERR;
					}

					if ((s[i + 3] >= '0') && (s[i + 3] <= '9'))
						s2[out] |= s[i + 3] - '0';
					else if ((s[i + 3] >= 'a') && (s[i + 3] <= 'f'))
						s2[out] |= s[i + 3] - 'a' + 10;
					else if ((s[i + 3] >= 'A') && (s[i + 3] <= 'F'))
						s2[out] |= s[i + 3] - 'A' + 10;
					else {
						free(s2);
						szl_set_last_fmt(interp, "bad hex digit: %c", s[i + 3]);
						return SZL_ERR;
					}

					i += 4;
					break;

				default:
					free(s2);
					szl_set_last_fmt(interp, "bad escape: \\%c", s[i + 1]);
					return SZL_ERR;
			}

			++out;
			if (out >= INT_MAX) {
				free(s2);
				szl_set_last_str(interp, "reached string len limit", -1);
				return SZL_ERR;
			}
		}
	}

	s2[out] = '\0';
	str = szl_new_str_noalloc(s2, out);
	if (!str) {
		free(s2);
		return SZL_ERR;
	}

	return szl_set_last(interp, str);
}

static
enum szl_res szl_str_proc_format(struct szl_interp *interp,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	struct szl_obj *str;
	char *fmt, *item;
	const char *pos, *prev;
	size_t flen, len, plen;
	int i;

	if (!szl_as_str(interp, objv[1], &fmt, &flen))
		return SZL_ERR;
	if (!flen) {
		szl_set_last_str(interp, "empty fmt", -1);
		return SZL_ERR;
	}

	str = szl_new_empty();
	if (!str)
		return SZL_ERR;

	i = 2;
	prev = fmt;
	do {
		pos = strstr(prev, SZL_FORMAT_SEQ);
		if (!pos) {
			if (i == objc)
				break;

			szl_unref(str);
			szl_set_last_fmt(interp, "extra args for fmt: %s", fmt);
			return SZL_ERR;
		}
		else if (i == objc) {
			szl_unref(str);
			szl_set_last_fmt(interp, "missing args for fmt: %s", fmt);
			return SZL_ERR;
		}

		plen = pos - prev;
		if ((plen && !szl_str_append_str(interp, str, prev, plen)) ||
		    !szl_as_str(interp, objv[i], &item, &len) ||
		    !szl_str_append_str(interp, str, item, len)) {
			szl_unref(str);
			return SZL_ERR;
		}

		prev = pos + sizeof(SZL_FORMAT_SEQ) - 1;

		++i;
	} while (1);

	len = flen - (prev - fmt);
	if (len && !szl_str_append_str(interp, str, prev, len)) {
		szl_unref(str);
		return SZL_ERR;
	}

	return szl_set_last(interp, str);
}

static
enum szl_res szl_str_proc_ltrim(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	char *s;
	size_t len, i;

	if (!szl_as_str(interp, objv[1], &s, &len))
		return SZL_ERR;

	for (i = 0; i < len; ++i) {
		if (!szl_isspace(s[i]))
			return szl_set_last_str(interp, &s[i], len - 1);
	}

	return szl_set_last(interp, szl_ref(objv[1]));
}

static
enum szl_res szl_str_proc_rtrim(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	char *s;
	size_t len;
	ssize_t i;

	if (!szl_as_str(interp, objv[1], &s, &len))
		return SZL_ERR;

	for (i = (ssize_t)len - 1; i >= 0; --i) {
		if (!szl_isspace(s[i]))
			return szl_set_last_str(interp, s, i + 1);
	}

	return szl_set_last(interp, szl_ref(objv[1]));
}

static
enum szl_res szl_str_proc_trim(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	char *s;
	size_t len, si;
	ssize_t ei;

	if (!szl_as_str(interp, objv[1], &s, &len))
		return SZL_ERR;

	if (!len)
		return szl_set_last(interp, szl_ref(objv[1]));

	for (si = 0; (si < len) && szl_isspace(s[si]); ++si);
	for (ei = (ssize_t)len - 1;
	     (ei >= (ssize_t)si) && szl_isspace(s[ei]);
	     --ei);

	return szl_set_last_str(interp, s + si, ei - si + 1);
}

static
enum szl_res szl_str_proc_ord(struct szl_interp *interp,
                              const unsigned int objc,
                              struct szl_obj **objv)
{
	struct szl_obj *list;
	char *s;
	size_t len, i;

	if (!szl_as_str(interp, objv[1], &s, &len))
		return SZL_ERR;

	list = szl_new_list(NULL, 0);
	if (!list)
		return SZL_ERR;

	for (i = 0; i < len; ++i) {
		if (!szl_list_append_int(interp, list, (szl_int)s[i])) {
			szl_unref(list);
			return SZL_ERR;
		}
	}

	return szl_set_last(interp, list);
}

static
const struct szl_ext_export str_exports[] = {
	{
		SZL_PROC_INIT("str.length", "str", 2, 2, szl_str_proc_length, NULL)
	},
	{
		SZL_PROC_INIT("str.in", "str sub", 3, 3, szl_str_proc_in, NULL)
	},
	{
		SZL_PROC_INIT("str.count", "str sub", 3, 3, szl_str_proc_count, NULL)
	},
	{
		SZL_PROC_INIT("str.range",
		              "str start end",
		              4,
		              4,
		              szl_str_proc_range,
		              NULL)
	},
	{
		SZL_PROC_INIT("str.tail", "str ?count?", 2, 3, szl_str_proc_tail, NULL)
	},
	{
		SZL_PROC_INIT("str.append", "str str", 3, 3, szl_str_proc_append, NULL)
	},
	{
		SZL_PROC_INIT("str.split", "str delim", 3, 3, szl_str_proc_split, NULL)
	},
	{
		SZL_PROC_INIT("str.join",
		              "str str ?...?",
		              4,
		              -1,
		              szl_str_proc_join,
		              NULL)
	},
	{
		SZL_PROC_INIT("str.expand", "str", 2, 2, szl_str_proc_expand, NULL)
	},
	{
		SZL_PROC_INIT("str.format",
		              "fmt obj...",
		              3,
		              -1,
		              szl_str_proc_format,
		              NULL)
	},
	{
		SZL_PROC_INIT("str.ltrim", "str", 2, 2, szl_str_proc_ltrim, NULL)
	},
	{
		SZL_PROC_INIT("str.rtrim", "str", 2, 2, szl_str_proc_rtrim, NULL)
	},
	{
		SZL_PROC_INIT("str.trim", "str", 2, 2, szl_str_proc_trim, NULL)
	},
	{
		SZL_PROC_INIT("str.ord", "str", 2, 2, szl_str_proc_ord, NULL)
	},
};

int szl_init_str(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "str",
	                   str_exports,
	                   sizeof(str_exports) / sizeof(str_exports[0]));
}
