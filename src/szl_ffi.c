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
#include <stdio.h>
#include <string.h>
#ifndef SZL_NO_DL
#	include <dlfcn.h>
#endif

#include <ffi.h>

#include "szl.h"

#define SZL_FFI_MAX_FUNC_ARGC 10

#define SZL_FFI__TOSTR_NAME(szl_name) szl_ffi_ ## szl_name ## _to_str
#define SZL_FFI_TOSTR_NAME(szl_name) SZL_FFI__TOSTR_NAME(szl_name)

#define SZL_FFI__NEW_AT_NAME(szl_name) szl_ffi_new_ ## szl_name ## _at
#define SZL_FFI_NEW_AT_NAME(szl_name) SZL_FFI__NEW_AT_NAME(szl_name)

#define SZL_FFI__FILLER_NAME(szl_name) szl_ffi_ ## szl_name ## _fill
#define SZL_FFI_FILLER_NAME(szl_name) SZL_FFI__FILLER_NAME(szl_name)

#define SZL_FFI_NEW__PROC(szl_name) szl_ffi_proc_ ## szl_name
#define SZL_FFI_NEW_PROC_NAME(szl_name) SZL_FFI_NEW__PROC(szl_name)

#define SZL_FFI_NEW_ALIAS_INIT(type, alias)    \
	SZL_PROC_INIT("ffi."#alias,                \
	              "?val?",                     \
	              1,                           \
	              2,                           \
	              SZL_FFI_NEW_PROC_NAME(type), \
	              NULL)
#define SZL_FFI_NEW_INIT(type) SZL_FFI_NEW_ALIAS_INIT(type, type)

struct szl_ffi_obj;
typedef struct szl_obj *(*szl_ffi_to_str)(struct szl_interp *,
                                          const struct szl_ffi_obj *);

union szl_ffi_obj_val {
	void *vp;

	uint64_t ui64;
	int64_t i64;
	uint32_t ui32;
	int32_t i32;
	uint16_t ui16;
	int16_t i16;
	uint8_t ui8;
	int8_t i8;

	short s;
	unsigned short us;
	int i;
	unsigned int ui;
	long l;
	unsigned long ul;
};

struct szl_ffi_obj {
    /* may be different than the libffi type size - i.e the size of a buffer
	 * object is its actual size, not the size of a pointer */
	size_t size;
	union szl_ffi_obj_val val;
	ffi_type *type;
	szl_ffi_to_str to_str;
	void *addr; /* points to the val member that holds the value */
};

struct szl_ffi_struct {
	ffi_type type;
	int size;
	int nmemb;
	int *offs;
	unsigned char *buf;
};

struct szl_ffi_func {
	ffi_cif cif;
	ffi_type *rtype;
	ffi_type **atypes;
	void *p;
	unsigned int argc; /* used to verify of the number of arguments, when
	                    * called */
	struct szl_obj *addr; /* the address object, which holds a reference to the
	                       * library object */
};


static void szl_ffi_obj_del(void *priv)
{
	free(priv);
}

static
struct szl_obj *szl_ffi_new_at(struct szl_interp *interp,
                               const char *type,
                               void (*filler)(struct szl_ffi_obj *,
                                              const void *,
                                              const size_t),
                               const void *p,
                               const size_t size,
                               const szl_ffi_to_str to_str,
                               szl_proc_t proc,
                               szl_del_t del)
{
	struct szl_ffi_obj *ffi_obj;
	struct szl_obj *name, *obj;

	if (!p) {
		szl_set_last_str(interp, "NULL deref", sizeof("NULL deref") - 1);
		return NULL;
	}

	ffi_obj = (struct szl_ffi_obj *)malloc(sizeof(*ffi_obj));
	if (!ffi_obj)
		return NULL;

	name = szl_new_str_fmt("%s:%"PRIxPTR, type, (uintptr_t)ffi_obj);
	if (!name)
		return NULL;

	filler(ffi_obj, p, size);
	ffi_obj->to_str = to_str;

	obj = szl_new_proc(interp,
	                   name,
	                   2,
	                   2,
	                   "address|size|value",
	                   proc,
	                   del,
	                   ffi_obj);
	if (obj)
		szl_unref(name);
	else {
		szl_free(name);
		free(ffi_obj);
	}

	return obj;
}

static
void szl_ffi_address_del(void *priv)
{
	szl_unref((struct szl_obj *)priv);
}

/* a "subclass" of SZL_TYPE_INT, which holds a reference to the object the
 * address originates from (i.e. the library a symbol belongs to); this
 * increases safety and simplifies memory management */
static
struct szl_obj *szl_ffi_new_address(struct szl_interp *interp,
                                    const szl_int i,
                                    struct szl_obj *origin)
{
	struct szl_obj *obj;

	obj = szl_new_int(interp, i);
	if (!obj)
		return obj;

	obj->del = szl_ffi_address_del;
	obj->priv = szl_ref(origin);
	return obj;
}

static enum szl_res szl_ffi_scalar_proc(struct szl_interp *interp,
                                        const unsigned int objc,
                                        struct szl_obj **objv)
{
	struct szl_obj *obj;
	struct szl_ffi_obj *ffi_obj = (struct szl_ffi_obj *)(objv[0]->priv);
	char *op;
	size_t len;

	if (!szl_as_str(interp, objv[1], &op, &len) || !len)
		return SZL_ERR;

	if (strcmp("value", op) == 0)
		return szl_set_last(interp, ffi_obj->to_str(interp, ffi_obj));
	else if (strcmp("raw", op) == 0) {
		obj = szl_new_str((char *)ffi_obj->addr, (ssize_t)ffi_obj->size);
		if (!obj)
			return SZL_ERR;

		return szl_set_last(interp, obj);
	}
	else if (strcmp("address", op) == 0) {
		obj = szl_ffi_new_address(interp,
		                          (szl_int)(intptr_t)ffi_obj->addr,
		                          objv[0]);
		if (!obj)
			return SZL_ERR;

		return szl_set_last(interp, obj);
	}
	else if (strcmp("size", op) == 0)
		return szl_set_last_fmt(interp, "%zu", ffi_obj->size);

	return szl_set_last_help(interp, objv[0]);
}

#define SZL_NEW_TOSTR(szl_name, val_memb, fmt, cast)                           \
static                                                                         \
struct szl_obj *SZL_FFI_TOSTR_NAME(szl_name)(                                  \
                                            struct szl_interp *interp,         \
                                            const struct szl_ffi_obj *ffi_obj) \
{                                                                              \
	struct szl_obj *obj;                                                       \
	char *s = NULL;                                                            \
	int len;                                                                   \
	                                                                           \
	len = asprintf(&s, fmt, cast(ffi_obj->val.val_memb));                      \
	if ((len <= 0) || (len >= INT_MAX - 1)) {                                  \
		if (s)                                                                 \
			free(s);                                                           \
		return NULL;                                                           \
	}                                                                          \
	                                                                           \
	obj = szl_new_str_noalloc(s, (size_t)len);                                 \
	if (!obj)                                                                  \
		free(s);                                                               \
	                                                                           \
	return obj;                                                                \
}                                                                              \

#define SZL_NEW_FILLER(szl_name, val_memb, c_type, ffi_type)                   \
static                                                                         \
void SZL_FFI_FILLER_NAME(szl_name)(struct szl_ffi_obj *ffi_obj,                \
                                   const void *p,                              \
                                   const size_t size)                          \
{                                                                              \
	ffi_obj->val.val_memb = *((c_type *)p);                                    \
	ffi_obj->type = ffi_type;                                                  \
	ffi_obj->to_str = SZL_FFI_TOSTR_NAME(szl_name);                            \
	ffi_obj->addr = &ffi_obj->val.val_memb;                                    \
	ffi_obj->size = size;                                                      \
}                                                                              \

#define SZL_NEW_AT(szl_name, size)                                             \
static                                                                         \
struct szl_obj *SZL_FFI_NEW_AT_NAME(szl_name)(struct szl_interp *interp,       \
                                              void *p)                         \
{                                                                              \
	return szl_ffi_new_at(interp,                                              \
	                      "ffi."# szl_name,                                    \
	                      SZL_FFI_FILLER_NAME(szl_name),                       \
	                      p,                                                   \
	                      size,                                                \
	                      SZL_FFI_TOSTR_NAME(szl_name),                        \
	                      szl_ffi_scalar_proc,                                 \
	                      szl_ffi_obj_del);                                    \
}                                                                              \

/* scalar types */

#define SZL_FFI_NEW_INT(szl_name, val_memb, c_type, min, max, ffi_type)        \
SZL_NEW_FILLER(szl_name, val_memb, c_type, ffi_type)                           \
SZL_NEW_AT(szl_name, sizeof(c_type))                                           \
                                                                               \
static                                                                         \
enum szl_res SZL_FFI_NEW_PROC_NAME(szl_name)(struct szl_interp *interp,        \
                                             const unsigned int objc,          \
                                             struct szl_obj **objv)            \
{                                                                              \
	struct szl_obj *obj;                                                       \
	szl_int i = 0;                                                             \
	c_type tmp;                                                                \
	                                                                           \
	if (objc == 2) {                                                           \
		if (!szl_as_int(interp, objv[1], &i))                                  \
			return SZL_ERR;                                                    \
                                                                               \
		if ((i < min) || (i > max)) {                                          \
			szl_set_last_fmt(interp,                                           \
			                   "bad "#szl_name" value: "SZL_INT_FMT"d",        \
			                   i);                                             \
			return SZL_ERR;                                                    \
		}                                                                      \
	}                                                                          \
	                                                                           \
	tmp = (c_type)i;                                                           \
	obj = SZL_FFI_NEW_AT_NAME(szl_name)(interp, &tmp);                         \
	if (!obj)                                                                  \
		return SZL_ERR;                                                        \
	                                                                           \
	return szl_set_last(interp, obj);                                          \
}

SZL_NEW_TOSTR(int8, i8, SZL_INT_FMT"d", (szl_int))
SZL_FFI_NEW_INT(int8, i8, int8_t, INT8_MIN, INT8_MAX, &ffi_type_sint8)

SZL_NEW_TOSTR(uint8, ui8, SZL_INT_FMT"d", (szl_int))
SZL_FFI_NEW_INT(uint8, ui8, uint8_t, 0, UINT8_MAX, &ffi_type_uint8)

SZL_NEW_TOSTR(int16, i16, SZL_INT_FMT"d", (szl_int))
SZL_FFI_NEW_INT(int16, i16, int16_t, INT16_MIN, INT16_MAX, &ffi_type_sint16)

SZL_NEW_TOSTR(uint16, ui16, SZL_INT_FMT"d", (szl_int))
SZL_FFI_NEW_INT(uint16, ui16, uint16_t, 0, UINT16_MAX, &ffi_type_uint16)

SZL_NEW_TOSTR(int32, i32, SZL_INT_FMT"d", (szl_int))
SZL_FFI_NEW_INT(int32, i32, int32_t, INT32_MIN, INT32_MAX, &ffi_type_sint32)

SZL_NEW_TOSTR(uint32, ui32, SZL_INT_FMT"d", (szl_int))
SZL_FFI_NEW_INT(uint32, ui32, uint32_t, 0, UINT32_MAX, &ffi_type_uint32)

SZL_NEW_TOSTR(int64, i64, SZL_INT_FMT"d", (szl_int))
SZL_FFI_NEW_INT(int64, i64, int64_t, INT64_MIN, INT64_MAX, &ffi_type_sint64)

SZL_NEW_TOSTR(uint64, ui64, SZL_INT_FMT"d", (szl_int))
SZL_FFI_NEW_INT(uint64, ui64, uint64_t, 0, UINT64_MAX, &ffi_type_uint64)

SZL_NEW_TOSTR(short, s, SZL_INT_FMT"d", (szl_int))
SZL_FFI_NEW_INT(short, s, short, SHRT_MIN, SHRT_MAX, &ffi_type_sshort)

SZL_NEW_TOSTR(ushort, us, SZL_INT_FMT"d", (szl_int))
SZL_FFI_NEW_INT(ushort, us, unsigned short, 0, USHRT_MAX, &ffi_type_ushort)

SZL_NEW_TOSTR(int, i, SZL_INT_FMT"d", (szl_int))
SZL_FFI_NEW_INT(int, i, int, INT_MIN, INT_MAX, &ffi_type_sint)

SZL_NEW_TOSTR(uint, ui, SZL_INT_FMT"d", (szl_int))
SZL_FFI_NEW_INT(uint, ui, unsigned int, 0, UINT_MAX, &ffi_type_uint)

SZL_NEW_TOSTR(long, l, SZL_INT_FMT"d", (szl_int))
SZL_FFI_NEW_INT(long, l, long, LONG_MIN, LONG_MAX, &ffi_type_slong)

SZL_NEW_TOSTR(ulong, ul, SZL_INT_FMT"d", (szl_int))
SZL_FFI_NEW_INT(ulong, ul, unsigned long, 0, ULONG_MAX, &ffi_type_ulong)

static
ffi_type *szl_ffi_get_type(const char *name)
{
	if ((strcmp("int8", name) == 0) || (strcmp("char", name) == 0))
		return &ffi_type_sint8;
	else if ((strcmp("uint8", name) == 0) || (strcmp("uchar", name) == 0))
		return &ffi_type_uint8;
	else if (strcmp("int16", name) == 0)
		return &ffi_type_sint16;
	else if (strcmp("uint16", name) == 0)
		return &ffi_type_uint16;
	else if (strcmp("int32", name) == 0)
		return &ffi_type_sint32;
	else if (strcmp("uint32", name) == 0)
		return &ffi_type_uint32;
	else if (strcmp("int64", name) == 0)
		return &ffi_type_sint64;
	else if (strcmp("uint64", name) == 0)
		return &ffi_type_uint64;
	else if (strcmp("short", name) == 0)
		return &ffi_type_sshort;
	else if (strcmp("ushort", name) == 0)
		return &ffi_type_ushort;
	else if (strcmp("int", name) == 0)
		return &ffi_type_sint;
	else if (strcmp("uint", name) == 0)
		return &ffi_type_uint;
	else if (strcmp("long", name) == 0)
		return &ffi_type_slong;
	else if (strcmp("ulong", name) == 0)
		return &ffi_type_ulong;
	else if (strcmp("pointer", name) == 0)
		return &ffi_type_pointer;
	else if (strcmp("void", name) == 0)
		return &ffi_type_void;
	else
		return NULL;
}

/* void */

static
struct szl_obj *SZL_FFI_TOSTR_NAME(void)(struct szl_interp *interp,
                                         const struct szl_ffi_obj *ffi_obj)
{
	return szl_ref(interp->empty);
}

static
void SZL_FFI_FILLER_NAME(void)(struct szl_ffi_obj *ffi_obj,
                               const void *p,
                               const size_t size)
{
	ffi_obj->val.vp = (void *)p;
	ffi_obj->type = &ffi_type_void;
	ffi_obj->to_str = SZL_FFI_TOSTR_NAME(void);
	ffi_obj->addr = &ffi_obj->val.vp;
	ffi_obj->size = size;
}

static
enum szl_res SZL_FFI_NEW_PROC_NAME(void)(struct szl_interp *interp,
                                         const unsigned int objc,
                                         struct szl_obj **objv)
{
	struct szl_obj *obj;

	obj = szl_ffi_new_at(interp,
	                     "ffi.void",
	                     SZL_FFI_FILLER_NAME(void),
	                     NULL,
	                     0,
	                     SZL_FFI_TOSTR_NAME(void),
	                     szl_ffi_scalar_proc,
	                     szl_ffi_obj_del);
	if (!obj)
		return SZL_ERR;

	return szl_set_last(interp, obj);
}

/* vector types */

SZL_NEW_TOSTR(pointer, vp, SZL_INT_FMT"d", (szl_int)(intptr_t))
SZL_NEW_FILLER(pointer, vp, void *, &ffi_type_pointer)
SZL_NEW_AT(pointer, sizeof(void *))

static
enum szl_res SZL_FFI_NEW_PROC_NAME(pointer)(struct szl_interp *interp,
                                            const unsigned int objc,
                                            struct szl_obj **objv)
{
	struct szl_obj *obj;
	szl_int i = 0;
	void *tmp = NULL;

	if (objc == 2) {
		if (!szl_as_int(interp, objv[1], &i))
			return SZL_ERR;

		tmp = (void *)(intptr_t)i;
	}

	obj = SZL_FFI_NEW_AT_NAME(pointer)(interp, &tmp);
	if (!obj)
		return SZL_ERR;

	return szl_set_last(interp, obj);
}

static
struct szl_obj *SZL_FFI_TOSTR_NAME(string)(struct szl_interp *interp,
                                           const struct szl_ffi_obj *ffi_obj)
{
	return szl_new_str((char *)ffi_obj->val.vp, ffi_obj->size - 1);
}

static
void SZL_FFI_FILLER_NAME(string)(struct szl_ffi_obj *ffi_obj,
                                 const void *p,
                                 const size_t size)
{
	ffi_obj->val.vp = *((void **)p);
	ffi_obj->type = &ffi_type_pointer;
	ffi_obj->to_str = SZL_FFI_TOSTR_NAME(string);
	ffi_obj->addr = &ffi_obj->val.vp;
	ffi_obj->size = size;
}

static
void szl_ffi_string_del(void *priv)
{
	free(((struct szl_ffi_obj *)priv)->val.vp);
	free(priv);
}

static
struct szl_obj *szl_ffi_new_string_at(struct szl_interp *interp,
                                      void **s,
                                      const size_t len)
{
	return szl_ffi_new_at(interp,
	                      "ffi.string",
	                      SZL_FFI_FILLER_NAME(string),
	                      s,
	                      len + 1,
	                      SZL_FFI_TOSTR_NAME(string),
	                      szl_ffi_scalar_proc,
	                      szl_ffi_string_del);
}

static
enum szl_res SZL_FFI_NEW_PROC_NAME(string)(struct szl_interp *interp,
                                           const unsigned int objc,
                                           struct szl_obj **objv)
{
	struct szl_obj *obj;
	char *op, *s2;
	szl_int addr, len = -1;
	size_t slen;

	if (!szl_as_str(interp, objv[1], &op, NULL))
		return SZL_ERR;

	if (strcmp("at", op) == 0) {
		if ((objc == 4) &&
		    ((!szl_as_int(interp, objv[3], &len)) ||
		     (len == 0) ||
		     (len < -1) ||
		     (len > INT_MAX)))
			goto usage;

		if (!szl_as_int(interp, objv[2], &addr))
			goto usage;

		if (len == -1) {
			s2 = strdup((char *)(intptr_t)addr);
			slen = strlen(s2);
			if (!s2)
				return SZL_ERR;
		}
		else {
			slen = (size_t)len;
			s2 = (char *)malloc(slen + 1);
			if (!s2)
				return SZL_ERR;
			memcpy(s2, (char *)(intptr_t)addr, slen);
			s2[slen] = '\0';
		}
	}
	else if ((objc == 3) && (strcmp("copy", op) == 0)) {
		s2 = szl_strdup(interp, objv[2], &slen);
		if (!s2)
			return SZL_ERR;
	}
	else {
usage:
		return szl_set_last_help(interp, objv[0]);
	}

	obj = szl_ffi_new_string_at(interp, (void **)&s2, slen);
	if (!obj) {
		free(s2);
		return SZL_ERR;
	}

	return szl_set_last(interp, obj);
}

/* casting */

static
struct szl_obj *szl_ffi_new_scalar_at(struct szl_interp *interp,
                                      void *p,
                                      const char *type)
{
	union szl_ffi_obj_val val;

	if ((strcmp("int8", type) == 0) || (strcmp("char", type) == 0)) {
		val.i8 = *((int8_t *)p);
		return SZL_FFI_NEW_AT_NAME(int8)(interp, &val.i8);
	}
	else if ((strcmp("uint8", type) == 0) || (strcmp("uchar", type) == 0)) {
		val.ui8 = *((uint8_t *)p);
		return SZL_FFI_NEW_AT_NAME(uint8)(interp, &val.ui8);
	}
	else if (strcmp("int16", type) == 0) {
		val.i16 = *((int16_t *)p);
		return SZL_FFI_NEW_AT_NAME(int16)(interp, &val.i16);
	}
	else if (strcmp("uint16", type) == 0) {
		val.ui16 = *((uint16_t *)p);
		return SZL_FFI_NEW_AT_NAME(uint16)(interp, &val.ui16);
	}
	else if (strcmp("int32", type) == 0) {
		val.i32 = *((int32_t *)p);
		return SZL_FFI_NEW_AT_NAME(int32)(interp, &val.i32);
	}
	else if ((strcmp("uint32", type) == 0) || (strcmp("dword", type) == 0)) {
		val.ui32 = *((uint32_t *)p);
		return SZL_FFI_NEW_AT_NAME(uint32)(interp, &val.ui32);
	}
	else if (strcmp("int64", type) == 0) {
		val.i64 = *((int64_t *)p);
		return SZL_FFI_NEW_AT_NAME(int64)(interp, &val.i64);
	}
	else if (strcmp("uint64", type) == 0) {
		val.ui64 = *((uint64_t *)p);
		return SZL_FFI_NEW_AT_NAME(uint64)(interp, &val.ui64);
	}
	else if (strcmp("short", type) == 0) {
		val.s = *((short *)p);
		return SZL_FFI_NEW_AT_NAME(short)(interp, &val.s);
	}
	else if (strcmp("ushort", type) == 0) {
		val.us = *((unsigned short *)p);
		return SZL_FFI_NEW_AT_NAME(ushort)(interp, &val.us);
	}
	else if (strcmp("int", type) == 0) {
		val.i = *((int *)p);
		return SZL_FFI_NEW_AT_NAME(int)(interp, &val.i);
	}
	else if (strcmp("uint", type) == 0) {
		val.ui = *((unsigned int *)p);
		return SZL_FFI_NEW_AT_NAME(uint)(interp, &val.ui);
	}
	else if (strcmp("long", type) == 0) {
		val.l = *((long *)p);
		return SZL_FFI_NEW_AT_NAME(long)(interp, &val.l);
	}
	else if (strcmp("ulong", type) == 0) {
		val.ul = *((unsigned long *)p);
		return SZL_FFI_NEW_AT_NAME(ulong)(interp, &val.ul);
	}
	else if (strcmp("pointer", type) == 0) {
		val.vp = *((void **)p);
		return SZL_FFI_NEW_AT_NAME(pointer)(interp, &val.vp);
	}

	szl_set_last_fmt(interp, "bad ffi type: %s", type);
	return NULL;
}

static
enum szl_res szl_ffi_proc_cast(struct szl_interp *interp,
                               const unsigned int objc,
                               struct szl_obj **objv)
{
	struct szl_obj *obj;
	char *type;
	szl_int addr;
	size_t len;

	if (!szl_as_int(interp, objv[1], &addr) ||
	    !szl_as_str(interp, objv[2], &type, &len) ||
	    !len)
		return SZL_ERR;

	obj = szl_ffi_new_scalar_at(interp, (void *)(intptr_t)addr, type);
	if (!obj)
		return SZL_ERR;

	return szl_set_last(interp, obj);
}

/* complex types */

static
struct szl_obj *szl_struct_member(struct szl_interp *interp,
                                  const struct szl_ffi_struct *s,
                                  const szl_int i)
{
	union szl_ffi_obj_val val;

	if (s->type.elements[i] == &ffi_type_sint8) {
		val.i8 = *((int8_t *)(s->buf + s->offs[i]));
		return SZL_FFI_NEW_AT_NAME(int8)(interp, &val.i8);
	}
	else if (s->type.elements[i] == &ffi_type_uint8) {
		val.ui8 = *((uint8_t *)(s->buf + s->offs[i]));
		return SZL_FFI_NEW_AT_NAME(uint8)(interp, &val.ui8);
	}
	else if (s->type.elements[i] == &ffi_type_sint16) {
		val.i16 = *((int16_t *)(s->buf + s->offs[i]));
		return SZL_FFI_NEW_AT_NAME(int16)(interp, &val.i16);
	}
	else if (s->type.elements[i] == &ffi_type_uint16) {
		val.ui16 = *((uint16_t *)(s->buf + s->offs[i]));
		return SZL_FFI_NEW_AT_NAME(uint16)(interp, &val.ui16);
	}
	else if (s->type.elements[i] == &ffi_type_sint32) {
		val.i32 = *((int32_t *)(s->buf + s->offs[i]));
		return SZL_FFI_NEW_AT_NAME(int32)(interp, &val.i32);
	}
	else if (s->type.elements[i] == &ffi_type_uint32) {
		val.ui32 = *((uint32_t *)(s->buf + s->offs[i]));
		return SZL_FFI_NEW_AT_NAME(uint32)(interp, &val.ui32);
	}
	else if (s->type.elements[i] == &ffi_type_sint64) {
		val.i64 = *((int64_t *)(s->buf + s->offs[i]));
		return SZL_FFI_NEW_AT_NAME(int64)(interp, &val.i64);
	}
	else if (s->type.elements[i] == &ffi_type_uint64) {
		val.ui64 = *((uint64_t *)(s->buf + s->offs[i]));
		return SZL_FFI_NEW_AT_NAME(uint64)(interp, &val.ui64);
	}
	else if (s->type.elements[i] == &ffi_type_sshort) {
		val.s = *((short *)(s->buf + s->offs[i]));
		return SZL_FFI_NEW_AT_NAME(short)(interp, &val.i64);
	}
	else if (s->type.elements[i] == &ffi_type_ushort) {
		val.us = *((unsigned short *)(s->buf + s->offs[i]));
		return SZL_FFI_NEW_AT_NAME(ushort)(interp, &val.ui64);
	}
	else if (s->type.elements[i] == &ffi_type_sint) {
		val.i = *((int *)(s->buf + s->offs[i]));
		return SZL_FFI_NEW_AT_NAME(int)(interp, &val.i64);
	}
	else if (s->type.elements[i] == &ffi_type_uint) {
		val.ui = *((unsigned int *)(s->buf + s->offs[i]));
		return SZL_FFI_NEW_AT_NAME(uint)(interp, &val.ui64);
	}
	else if (s->type.elements[i] == &ffi_type_slong) {
		val.l = *((long *)(s->buf + s->offs[i]));
		return SZL_FFI_NEW_AT_NAME(long)(interp, &val.i64);
	}
	else if (s->type.elements[i] == &ffi_type_ulong) {
		val.ul = *((unsigned long *)(s->buf + s->offs[i]));
		return SZL_FFI_NEW_AT_NAME(ulong)(interp, &val.ui64);
	}
	else {
		val.vp = *((void **)(s->buf + s->offs[i]));
		return SZL_FFI_NEW_AT_NAME(pointer)(interp, &val.vp);
	}
}

static
enum szl_res szl_ffi_struct_proc(struct szl_interp *interp,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	struct szl_obj *obj;
	struct szl_ffi_struct *s = (struct szl_ffi_struct *)(objv[0]->priv);
	char *op;
	szl_int i;
	size_t len;

	if (!szl_as_str(interp, objv[1], &op, &len) || !len)
		return SZL_ERR;

	if (objc == 2) {
		if ((strcmp("raw", op) == 0) || (strcmp("value", op) == 0)) {
			obj = szl_new_str((char *)s->buf, (ssize_t)s->size);
			if (!obj)
				return SZL_ERR;

			return szl_set_last(interp, obj);
		}
		else if (strcmp("address", op) == 0) {
			obj = szl_ffi_new_address(interp,
			                          (szl_int)(intptr_t)s->buf,
			                          objv[0]);
			if (!obj)
				return SZL_ERR;

			return szl_set_last(interp, obj);
		}
		else if (strcmp("size", op) == 0)
			return szl_set_last_fmt(interp, "%d", s->size);

		goto usage;
	}
	else if (strcmp("member", op) == 0) {
		if (!szl_as_int(interp, objv[2], &i))
			return SZL_ERR;

		if ((i < 0) || (i >= s->nmemb - 1)) {
			szl_set_last_fmt(interp, "bad member index: "SZL_INT_FMT"d", i);
			return SZL_ERR;
		}

		obj = szl_struct_member(interp, s, i);
		if (!obj)
			return SZL_ERR;

		return szl_set_last(interp, obj);
	}
	else {
usage:
		return szl_set_last_help(interp, objv[0]);
	}

	return SZL_ERR;
}

static
void szl_ffi_struct_del(void *priv)
{
	struct szl_ffi_struct *s = (struct szl_ffi_struct *)priv;

	free(s->offs);
	free(s->type.elements);
	free(s->buf);
	free(priv);
}

static
enum szl_res szl_ffi_proc_struct(struct szl_interp *interp,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	struct szl_obj *name, *obj;
	struct szl_ffi_struct *s;
	char *type, *raw;
	size_t len;
	unsigned int i, j;

	s = (struct szl_ffi_struct *)malloc(sizeof(*s));
	if (!s)
		return SZL_ERR;

	s->nmemb = objc - 1;
	s->type.elements = (ffi_type **)malloc(sizeof(ffi_type *) * (s->nmemb + 1));
	if (!s->type.elements) {
		free(s);
		return SZL_ERR;
	}
	s->offs = (int *)malloc(sizeof(int) * s->nmemb);
	if (!s->offs) {
		free(s->type.elements);
		free(s);
		return SZL_ERR;
	}

	s->size = 0;
	for (i = 2; i < objc; ++i) {
		if (!szl_as_str(interp, objv[i], &type, &len) || !len) {
			free(s->offs);
			free(s->type.elements);
			free(s);
			return SZL_ERR;
		}

		j = i - 2;

		s->type.elements[j] = szl_ffi_get_type(type);
		if (!s->type.elements[j]) {
			free(s->offs);
			free(s->type.elements);
			free(s);
			return SZL_ERR;
		}

		/* cache the member offset inside the raw struct */
		s->offs[j] = s->size;

		s->size += s->type.elements[j]->size;
		if (s->size >= (INT_MAX - s->type.elements[j]->size)) {
			free(s->offs);
			free(s->type.elements);
			free(s);
			szl_set_last_str(interp, "bad struct size", -1);
			return SZL_ERR;
		}
	}

	if (!szl_as_str(interp, objv[1], &raw, &len)) {
		free(s->offs);
		free(s->type.elements);
		free(s);
		return SZL_ERR;
	}

	/* if an initializer is specified, it must be the same size as the struct */
	if ((len != 0) && ((size_t)s->size != len)) {
		free(s->offs);
		free(s->type.elements);
		free(s);
		szl_set_last_str(interp, "bad struct initializer", -1);
		return SZL_ERR;
	}

	s->buf = (unsigned char*)malloc((size_t)s->size);
	if (!s->buf) {
		free(s->offs);
		free(s->type.elements);
		free(s);
		return SZL_ERR;
	}

	if (len == 0) {
		memset(s->buf, 0, (size_t)s->size);
	} else {
		/* copy the initializer */
		memcpy(s->buf, raw, (size_t)s->size);
	}
	s->type.size = 0;
	s->type.alignment = 0;
	s->type.type = FFI_TYPE_STRUCT;
	s->type.elements[s->nmemb] = NULL;

	name = szl_new_str_fmt("ffi.struct:%"PRIxPTR, (uintptr_t)s);
	if (!name) {
		free(s->offs);
		free(s->type.elements);
		free(s);
		return SZL_ERR;
	}

	obj = szl_new_proc(interp,
	                   name,
	                   2,
	                   3,
	                   "address|size|value|member ?index?",
	                   szl_ffi_struct_proc,
	                   szl_ffi_struct_del,
	                   s);
	if (!obj) {
		szl_free(name);
		free(s->offs);
		free(s->type.elements);
		free(s);
		return SZL_ERR;
	}

	szl_unref(name);
	return szl_set_last(interp, obj);
}

#ifndef SZL_NO_DL

/* libraries */

static
enum szl_res szl_ffi_library_proc(struct szl_interp *interp,
                                  const unsigned int objc,
                                  struct szl_obj **objv)
{
	struct szl_obj *obj;
	char *op, *sym;
	void *p;
	size_t len;

	if (!szl_as_str(interp, objv[1], &op, NULL))
		return SZL_ERR;

	if (objc == 3) {
		if (strcmp("dlsym", op) != 0)
			return szl_set_last_help(interp, objv[0]);

		if (!szl_as_str(interp, objv[2], &sym, &len) || !len)
			return SZL_ERR;

		p = dlsym(objv[0]->priv, sym);
	}
	else if (strcmp("handle", op) == 0)
		p = objv[0]->priv;
	else
		return szl_set_last_help(interp, objv[0]);

	obj = szl_ffi_new_address(interp, (szl_int)(intptr_t)p, objv[0]);
	if (!obj)
		return SZL_ERR;

	return szl_set_last(interp, obj);
}

static
enum szl_res szl_ffi_proc_dlopen(struct szl_interp *interp,
                                 const unsigned int objc,
                                 struct szl_obj **objv)
{
	void *h;
	char *path;
	struct szl_obj *name, *obj;
	size_t len;

	if (!szl_as_str(interp, objv[1], &path, &len))
		return SZL_ERR;
	if (!len)
		path = NULL;

	h = dlopen(path, RTLD_LAZY);
	if (!h)
		return SZL_ERR;

	name = szl_new_str_fmt("ffi.library:%"PRIxPTR, (uintptr_t)h);
	if (!name) {
		dlclose(h);
		return SZL_ERR;
	}

	obj = szl_new_proc(interp,
	                   name,
	                   2,
	                   3,
	                   "handle|dlsym ?name?",
	                   szl_ffi_library_proc,
	                   szl_unload,
	                   h);
	if (!obj) {
		szl_free(name);
		dlclose(h);
		return SZL_ERR;
	}

	szl_unref(name);
	return szl_set_last(interp, obj);
}

#endif

/* functions */

static
enum szl_res szl_ffi_function_proc(struct szl_interp *interp,
                                   const unsigned int objc,
                                   struct szl_obj **objv)
{
	struct szl_ffi_func *f = (struct szl_ffi_func *)(objv[0]->priv);
	void *out;
	void **args;
	szl_int tmp, out_int;
	int argc;
	unsigned int i, j;

	argc = objc - 2;
	if ((unsigned int)argc != f->argc) {
		szl_set_last_fmt(interp, "wrong # of arguments: %d", argc);
		return SZL_ERR;
	}

	if (!szl_as_int(interp, objv[1], &out_int))
		return SZL_ERR;

	out = (void *)(intptr_t)out_int;
	if (!out) {
		szl_set_last_str(interp, "NULL return value address", -1);
		return SZL_ERR;
	}

	args = (void **)malloc(sizeof(void *) * f->argc);
	if (!args)
		return SZL_ERR;

	if (f->argc) {
		for (i = 2; i < objc; ++i) {
			if (!szl_as_int(interp, objv[i], &tmp)) {
				free(args);
				return SZL_ERR;
			}

			j = i - 2;
			args[j] = (void *)(intptr_t)tmp;
			if (!args[j]) {
				free(args);
				szl_set_last_str(interp, "NULL argument address", -1);
				return SZL_ERR;
			}
		}
	}
	else
		args = NULL;

	ffi_call(&f->cif, FFI_FN(f->p), out, args);
	free(args);

	/* use the return value address as the return value, to allow one-liners */
	return szl_set_last_int(interp, out_int);
}

static
void szl_ffi_function_del(void *priv)
{
	struct szl_ffi_func *f = (struct szl_ffi_func *)priv;

	szl_unref(f->addr);

	if (f->atypes)
		free(f->atypes);

	free(priv);
}

static
enum szl_res szl_ffi_proc_function(struct szl_interp *interp,
                                   const unsigned int objc,
                                   struct szl_obj **objv)
{
	struct szl_obj *name, *obj;
	struct szl_ffi_func *f;
	char *s;
	szl_int addr;
	size_t len;
	unsigned int i, j;

	if (!szl_as_int(interp, objv[1], &addr))
		return SZL_ERR;

	if (!addr) {
		szl_set_last_str(interp, "NULL function address", -1);
		return SZL_ERR;
	}

	if (!szl_as_str(interp, objv[2], &s, &len) || !len)
		return SZL_ERR;

	f = (struct szl_ffi_func *)malloc(sizeof(*f));
	if (!f)
		return SZL_ERR;

	f->rtype = szl_ffi_get_type(s);
	if (!f->rtype) {
		szl_set_last_fmt(interp, "bad ret type: %s", s);
		free(f);
		return SZL_ERR;
	}

	f->argc = objc - 3;
	f->atypes = (ffi_type **)malloc(sizeof(ffi_type *) * f->argc);
	if (!f->atypes) {
		free(f);
		return SZL_ERR;
	}

	if (f->argc)
		for (i = 3; i < objc; ++i) {
			if (!szl_as_str(interp, objv[i], &s, &len) || !len) {
				free(f->atypes);
				free(f);
				return SZL_ERR;
			}

			j = i - 3;
			f->atypes[j] = szl_ffi_get_type(s);
			if (!f->atypes[j] || (f->atypes[j] == &ffi_type_void)) {
				szl_set_last_fmt(interp, "bad arg type: %s", s);
				free(f->atypes);
				free(f);
				return SZL_ERR;
			}
		}
	else
		f->atypes = NULL;

	if (ffi_prep_cif(&f->cif,
	                 FFI_DEFAULT_ABI,
	                 f->argc,
	                 f->rtype,
	                 f->atypes) != FFI_OK) {
		if (f->atypes)
			free(f->atypes);
		free(f);
		return SZL_ERR;
	}

	name = szl_new_str_fmt("ffi.function:%"PRIxPTR, (uintptr_t)f);
	if (!name) {
		if (f->atypes)
			free(f->atypes);
		free(f);
		return SZL_ERR;
	}

	obj = szl_new_proc(interp,
	                   name,
	                   2 + f->argc, /* procedure, return value and arguments */
	                   2 + f->argc,
	                   "ret ?arg...?",
	                   szl_ffi_function_proc,
	                   szl_ffi_function_del,
	                   f);
	if (!obj) {
		szl_free(name);
		if (f->atypes)
			free(f->atypes);
		free(f);
		return SZL_ERR;
	}

	szl_unref(name);
	f->p = (void *)(intptr_t)addr;
	f->addr = szl_ref(objv[1]);
	return szl_set_last(interp, obj);
}

static
const struct szl_ext_export ffi_exports[] = {
	{
		SZL_FFI_NEW_INIT(int8)
	},
	{
		SZL_FFI_NEW_ALIAS_INIT(int8, char)
	},
	{
		SZL_FFI_NEW_INIT(uint8)
	},
	{
		SZL_FFI_NEW_ALIAS_INIT(uint8, uchar)
	},
	{
		SZL_FFI_NEW_INIT(int16)
	},
	{
		SZL_FFI_NEW_INIT(uint16)
	},
	{
		SZL_FFI_NEW_INIT(int32)
	},
	{
		SZL_FFI_NEW_INIT(uint32)
	},
	{
		SZL_FFI_NEW_ALIAS_INIT(uint32, dword)
	},
	{
		SZL_FFI_NEW_INIT(int64)
	},
	{
		SZL_FFI_NEW_INIT(uint64)
	},
	{
		SZL_FFI_NEW_INIT(short)
	},
	{
		SZL_FFI_NEW_INIT(ushort)
	},
	{
		SZL_FFI_NEW_INIT(int)
	},
	{
		SZL_FFI_NEW_INIT(uint)
	},
	{
		SZL_FFI_NEW_INIT(long)
	},
	{
		SZL_FFI_NEW_INIT(ulong)
	},
	{
		SZL_FFI_NEW_INIT(pointer)
	},
	{
		SZL_PROC_INIT("ffi.void",
		              NULL,
		              1,
		              1,
		              SZL_FFI_NEW_PROC_NAME(void),
		              NULL)
	},
	{
		SZL_PROC_INIT("ffi.string",
		              "at|copy addr|obj ?len?",
		              3,
		              4,
		              SZL_FFI_NEW_PROC_NAME(string),
		              NULL)
	},
	{
		SZL_PROC_INIT("ffi.cast",
		              "addr type",
		              3,
		              3,
		              szl_ffi_proc_cast,
		              NULL)
	},
	{
		SZL_PROC_INIT("ffi.struct",
		              "raw type...",
		              3,
		              -1,
		              szl_ffi_proc_struct,
		              NULL)
	},
#ifndef SZL_NO_DL
	{
		SZL_PROC_INIT("ffi.dlopen",
		              "path",
		              2,
		              2,
		              szl_ffi_proc_dlopen,
		              NULL)
	},
#endif
	{
		SZL_PROC_INIT("ffi.function",
		              "addr rtype ?type...?",
		              3,
		              2 + SZL_FFI_MAX_FUNC_ARGC,
		              szl_ffi_proc_function,
		              NULL)
	},
};

int szl_init_ffi(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "ffi",
	                   ffi_exports,
	                   sizeof(ffi_exports) / sizeof(ffi_exports[0]));
}
