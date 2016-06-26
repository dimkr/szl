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

#include "szl.h"

#define SZL_FORMAT_SEQ "{}"

static const char szl_str_inc[] = {
#include "szl_str.inc"
};

static
enum szl_res szl_str_proc_length(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv)
{
	size_t len;

	if (!szl_obj_len(interp, objv[1], &len) || (len > INT_MAX))
		return SZL_ERR;

	return szl_set_result_int(interp, (szl_int)len);
}

static
enum szl_res szl_str_proc_range(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	const char *s;
	szl_int start, end;
	size_t len;

	s = szl_obj_str(interp, objv[1], &len);
	if (!s)
		return SZL_ERR;

	if (!szl_obj_int(interp, objv[2], &start) ||
	    (start < 0) ||
	    (start >= len) ||
	    !szl_obj_int(interp, objv[3], &end) ||
	    (end < start) ||
	    (end >= len))
		return SZL_ERR;

	return szl_set_result_str(interp, s + start, end - start + 1);
}

static
enum szl_res szl_str_proc_append(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv)
{
	struct szl_obj *obj;
	const char *name;
	const char *s;
	size_t len;
	enum szl_res res = SZL_OK;

	name = szl_obj_str(interp, objv[1], NULL);
	if (!name)
		return SZL_ERR;

	obj = szl_get(interp, name);
	if (!obj)
		return SZL_ERR;

	s = szl_obj_str(interp, objv[2], &len);
	if (s) {
		if (len) {
			if (!szl_append(interp, obj, s, len))
				res = SZL_ERR;
		}
	}
	else
		res = SZL_ERR;

	szl_obj_unref(obj);
	return res;
}

static
enum szl_res szl_str_proc_split(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	struct szl_obj *list;
	char *s, *delim, *tok, *pos;
	size_t slen, dlen;

	s = szl_obj_str(interp, objv[1], &slen);
	if (!s)
		return SZL_ERR;

	delim = szl_obj_str(interp, objv[2], &dlen);
	if (!delim)
		return SZL_ERR;

	if (!dlen)
		return szl_usage(interp, objv[0]);

	if (!slen)
		return SZL_OK;

	list = szl_new_empty();
	if (!list)
		return SZL_ERR;

	tok = strtok_r(s, delim, &pos);
	while (tok) {
		if (!szl_lappend_str(interp, list, tok)) {
			szl_obj_unref(list);
			return SZL_ERR;
		}

		tok = strtok_r(NULL, delim, &pos);
	}

	return szl_set_result(interp, list);
}

static
enum szl_res szl_str_proc_join(struct szl_interp *interp,
                               const int objc,
                               struct szl_obj **objv)
{
	struct szl_obj *obj;

	obj = szl_join(interp, objv[1], &objv[2], objc - 2, 0);
	if (obj)
		return szl_set_result(interp, obj);

	return SZL_ERR;
}

static
enum szl_res szl_str_proc_expand(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv)
{
	struct szl_obj *str;
	const char *s;
	char *s2;
	size_t len, i, end, out;

	s = szl_obj_str(interp, objv[1], &len);
	if (!s)
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
				szl_set_result_str(interp, "bad escape sequence", -1);
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

				case '{':
					s2[out] = '{';
					i += 2;
					break;

				case '}':
					s2[out] = '}';
					i += 2;
					break;

				case 'x':
					if ((i + 3 > end) ||
					    !(((s[i + 2] >= '0') && (s[i + 2] <= '9')) ||
					      ((s[i + 2] >= 'a') && (s[i + 2] <= 'f')) ||
					      ((s[i + 2] >= 'A') && (s[i + 2] <= 'F'))) ||
					    !(((s[i + 3] >= '0') && (s[i + 3] <= '9')) ||
					      ((s[i + 3] >= 'a') && (s[i + 3] <= 'f')) ||
					      ((s[i + 3] >= 'A') && (s[i + 3] <= 'F')))) {
						free(s2);
						szl_set_result_str(interp, "bad hex escape", -1);
						return SZL_ERR;
					}

					s2[out] = ((s[i + 2] - '0') << 4) | (s[i + 3] - '0');
					i += 4;
					break;

				default:
					s2[out] = s[i];
					++i;
					break;
			}

			++out;
			if (out >= INT_MAX) {
				free(s2);
				szl_set_result_str(interp, "reached string len limit", -1);
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

	return szl_set_result(interp, str);
}

static
enum szl_res szl_str_proc_format(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv)
{
	struct szl_obj *str;
	const char *fmt, *pos, *prev, *item;
	size_t flen, len, plen;
	int i;

	fmt = szl_obj_str(interp, objv[1], &flen);
	if (!fmt)
		return SZL_ERR;

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

			szl_obj_unref(str);
			szl_set_result_fmt(interp, "too many args for fmt: %s", fmt);
			return SZL_ERR;
		}
		else if (i == objc) {
			szl_obj_unref(str);
			szl_set_result_fmt(interp, "bad fmt: %s", fmt);
			return SZL_ERR;
		}

		plen = pos - prev;
		if (plen && !szl_append(interp, str, prev, plen)) {
			szl_obj_unref(str);
			return SZL_ERR;
		}

		item = szl_obj_str(interp, objv[i], &len);
		if (!item || !szl_append(interp, str, item, len)) {
			szl_obj_unref(str);
			return SZL_ERR;
		}

		pos += sizeof(SZL_FORMAT_SEQ) - 1;
		prev = pos;

		++i;
	} while (1);

	len = flen - (prev - fmt);
	if (len && !szl_append(interp, str, prev, len)) {
		szl_obj_unref(str);
		return SZL_ERR;
	}

	return szl_set_result(interp, str);
}

static
enum szl_res szl_str_proc_ltrim(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	struct szl_obj *obj;
	const char *s;
	size_t len, i;

	s = szl_obj_str(interp, objv[1], &len);
	if (!s)
		return SZL_ERR;

	for (i = 0; i < len; ++i) {
		if (!szl_isspace(s[i])) {
			obj = szl_new_str(&s[i], len - i);
			if (!obj)
				return SZL_ERR;

			return szl_set_result(interp, obj);
		}
	}

	return szl_set_result(interp, szl_obj_ref(objv[1]));
}

static
enum szl_res szl_str_proc_rtrim(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	struct szl_obj *obj;
	const char *s;
	size_t len;
	ssize_t i;

	s = szl_obj_str(interp, objv[1], &len);
	if (!s)
		return SZL_ERR;

	for (i = (ssize_t)len - 1; i >= 0; --i) {
		if (!szl_isspace(s[i])) {
			obj = szl_new_str(s, (size_t)i + 1);
			if (!obj)
				return SZL_ERR;

			return szl_set_result(interp, obj);
		}
	}

	return szl_set_result(interp, szl_obj_ref(objv[1]));
}

int szl_init_str(struct szl_interp *interp)
{
	return ((szl_new_proc(interp,
	                      "str.length",
	                      2,
	                      2,
	                      "str.length str",
	                      szl_str_proc_length,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "str.range",
	                      4,
	                      4,
	                      "str.range str start end",
	                      szl_str_proc_range,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "str.append",
	                      3,
	                      3,
	                      "str.append name str",
	                      szl_str_proc_append,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "str.split",
	                      3,
	                      3,
	                      "str.split str delim",
	                      szl_str_proc_split,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "str.join",
	                      4,
	                      -1,
	                      "str.join delim str str ?...?",
	                      szl_str_proc_join,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "str.expand",
	                      2,
	                      2,
	                      "str.expand str",
	                      szl_str_proc_expand,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "str.format",
	                      3,
	                      -1,
	                      "str.format fmt obj...",
	                      szl_str_proc_format,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "ltrim",
	                      2,
	                      2,
	                      "ltrim str",
	                      szl_str_proc_ltrim,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "rtrim",
	                      2,
	                      2,
	                      "rtrim str",
	                      szl_str_proc_rtrim,
	                      NULL,
	                      NULL)) &&
	        (szl_run(interp, szl_str_inc, sizeof(szl_str_inc) - 1) == SZL_OK));
}
