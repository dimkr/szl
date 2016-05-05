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
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

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

#define SZL_FFI_NEW_OBJ_PROC_ALIAS(type, alias)       \
	szl_new_proc(interp,                              \
	             "ffi."#alias,                        \
	             1,                                   \
	             2,                                   \
	             "ffi."#alias" ?val?",                \
	             SZL_FFI_NEW_PROC_NAME(type),         \
	             NULL,                                \
	             NULL)
#define SZL_FFI_NEW_OBJ_PROC(type) SZL_FFI_NEW_OBJ_PROC_ALIAS(type, type)

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

struct szl_ffi_func {
	ffi_cif cif;
	ffi_type *rtype;
	ffi_type **atypes;
	void *p;
	unsigned int argc; /* used to verify of the number of arguments, when called */
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
                               const szl_proc proc,
                               const szl_delproc del)
{
	char buf[sizeof("ffi.pointer.FFFFFFFFF")];
	struct szl_ffi_obj *ffi_obj;
	struct szl_obj *obj;

	ffi_obj = (struct szl_ffi_obj *)malloc(sizeof(*ffi_obj));
	if (!ffi_obj)
		return NULL;

	filler(ffi_obj, p, size);
	ffi_obj->to_str = to_str;

	szl_new_obj_name(interp, type, buf, sizeof(buf));
	obj = szl_new_proc(interp,
	                   buf,
	                   2,
	                   2,
	                   "$obj address|size|value",
	                   proc,
	                   del,
	                   ffi_obj);
	if (!obj) {
		free(ffi_obj);
		return NULL;
	}

	return szl_obj_ref(obj);
}

static enum szl_res szl_ffi_scalar_proc(struct szl_interp *interp,
                                        const int objc,
                                        struct szl_obj **objv,
                                        struct szl_obj **ret)
{
	struct szl_ffi_obj *ffi_obj = (struct szl_ffi_obj *)(objv[0]->priv);
	const char *op;
	size_t len;

	*ret = NULL;

	op = szl_obj_str(objv[1], &len);
	if (!op || !len)
		return SZL_ERR;

	if (strcmp("value", op) == 0)
		szl_set_result(ret, ffi_obj->to_str(interp, ffi_obj));
	else if (strcmp("raw", op) == 0)
		szl_set_result(ret,
		               szl_new_str((char *)ffi_obj->addr, (int)ffi_obj->size));
	else if (strcmp("address", op) == 0)
		//szl_set_result_fmt(interp, ret, "%p", ffi_obj->addr);
		szl_set_result_fmt(interp, ret, SZL_INT_FMT, (szl_int)(intptr_t)ffi_obj->addr);
	else if (strcmp("size", op) == 0)
		szl_set_result_fmt(interp, ret, "%zd", ffi_obj->size);
	else {
		szl_usage(interp, ret, objv[0]);
		return SZL_ERR;
	}

	if (!*ret)
		return SZL_ERR;

	return SZL_OK;
}

#define SZL_NEW_TOSTR(szl_name, val_memb, fmt, cast)                           \
static                                                                         \
struct szl_obj *SZL_FFI_TOSTR_NAME(szl_name)(                                  \
                                            struct szl_interp *interp,         \
                                            const struct szl_ffi_obj *ffi_obj) \
{                                                                              \
	char *s;                                                                   \
	int len;                                                                   \
	                                                                           \
	len = asprintf(&s, fmt, cast(ffi_obj->val.val_memb));                      \
	if ((len <= 0) || (len > INT_MAX - 1))                                     \
		return NULL;                                                           \
	                                                                           \
	return szl_new_str_noalloc(s, (size_t)len);                                \
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
enum szl_res SZL_FFI_NEW_AT_NAME(szl_name)(struct szl_interp *interp,          \
                                           void *p,                            \
                                           struct szl_obj **out)               \
{                                                                              \
	*out = szl_ffi_new_at(interp,                                              \
	                      "ffi."# szl_name,                                    \
	                      SZL_FFI_FILLER_NAME(szl_name),                       \
	                      p,                                                   \
	                      size,                                                \
	                      SZL_FFI_TOSTR_NAME(szl_name),                        \
	                      szl_ffi_scalar_proc,                                 \
	                      szl_ffi_obj_del);                                    \
	if (!*out)                                                                 \
		return SZL_ERR;                                                        \
	                                                                           \
	return SZL_OK;                                                             \
}                                                                              \

/* scalar types */

#define SZL_FFI_NEW_INT(szl_name, val_memb, c_type, min, max, ffi_type)        \
SZL_NEW_FILLER(szl_name, val_memb, c_type, ffi_type)                           \
SZL_NEW_AT(szl_name, sizeof(c_type))                                           \
                                                                               \
static                                                                         \
enum szl_res SZL_FFI_NEW_PROC_NAME(szl_name)(struct szl_interp *interp,        \
                                             const int objc,                   \
                                             struct szl_obj **objv,            \
                                             struct szl_obj **ret)             \
{                                                                              \
	szl_int i = 0;                                                             \
	c_type tmp;                                                                \
	                                                                           \
	if (objc == 2) {                                                           \
		if (szl_obj_int(objv[1], &i) != SZL_OK)                                \
			return SZL_ERR;                                                    \
                                                                               \
		if ((i < min) || (i > max)) {                                          \
			szl_set_result_fmt(interp,                                         \
			                   ret,                                            \
			                   "bad "#szl_name" value: "SZL_INT_FMT,           \
			                   i);                                             \
			return SZL_ERR;                                                    \
		}                                                                      \
	}                                                                          \
	                                                                           \
	tmp = (c_type)i;                                                           \
	return SZL_FFI_NEW_AT_NAME(szl_name)(interp, &tmp, ret);                   \
}

SZL_NEW_TOSTR(int8, i8, SZL_INT_FMT, (szl_int))
SZL_FFI_NEW_INT(int8, i8, int8_t, INT8_MIN, INT8_MAX, &ffi_type_sint8)

SZL_NEW_TOSTR(uint8, ui8, SZL_INT_FMT, (szl_int))
SZL_FFI_NEW_INT(uint8, ui8, uint8_t, 0, UINT8_MAX, &ffi_type_uint8)

SZL_NEW_TOSTR(int16, i16, SZL_INT_FMT, (szl_int))
SZL_FFI_NEW_INT(int16, i16, int16_t, INT16_MIN, INT16_MAX, &ffi_type_sint16)

SZL_NEW_TOSTR(uint16, ui16, SZL_INT_FMT, (szl_int))
SZL_FFI_NEW_INT(uint16, ui16, uint16_t, 0, UINT16_MAX, &ffi_type_uint16)

SZL_NEW_TOSTR(int32, i32, SZL_INT_FMT, (szl_int))
SZL_FFI_NEW_INT(int32, i32, int32_t, INT32_MIN, INT32_MAX, &ffi_type_sint32)

SZL_NEW_TOSTR(uint32, ui32, SZL_INT_FMT, (szl_int))
SZL_FFI_NEW_INT(uint32, ui32, uint32_t, 0, UINT32_MAX, &ffi_type_uint32)

SZL_NEW_TOSTR(int64, i64, SZL_INT_FMT, (szl_int))
SZL_FFI_NEW_INT(int64, i64, int64_t, INT64_MIN, INT64_MAX, &ffi_type_sint64)

SZL_NEW_TOSTR(uint64, ui64, SZL_INT_FMT, (szl_int))
SZL_FFI_NEW_INT(uint64, ui64, uint64_t, 0, UINT64_MAX, &ffi_type_uint64)

SZL_NEW_TOSTR(short, s, SZL_INT_FMT, (szl_int))
SZL_FFI_NEW_INT(short, s, short, SHRT_MIN, SHRT_MAX, &ffi_type_sshort)

SZL_NEW_TOSTR(ushort, us, SZL_INT_FMT, (szl_int))
SZL_FFI_NEW_INT(ushort, us, unsigned short, 0, USHRT_MAX, &ffi_type_ushort)

SZL_NEW_TOSTR(int, i, SZL_INT_FMT, (szl_int))
SZL_FFI_NEW_INT(int, i, int, INT_MIN, INT_MAX, &ffi_type_sint)

SZL_NEW_TOSTR(uint, ui, SZL_INT_FMT, (szl_int))
SZL_FFI_NEW_INT(uint, ui, unsigned int, 0, UINT_MAX, &ffi_type_uint)

SZL_NEW_TOSTR(long, l, SZL_INT_FMT, (szl_int))
SZL_FFI_NEW_INT(long, l, long, LONG_MIN, LONG_MAX, &ffi_type_slong)

SZL_NEW_TOSTR(ulong, ul, SZL_INT_FMT, (szl_int))
SZL_FFI_NEW_INT(ulong, ul, unsigned long, 0, ULONG_MAX, &ffi_type_ulong)

/* void */

static
struct szl_obj *SZL_FFI_TOSTR_NAME(void)(struct szl_interp *interp,
                                         const struct szl_ffi_obj *ffi_obj)
{
	return szl_empty(interp);
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
                                         const int objc,
                                         struct szl_obj **objv,
                                         struct szl_obj **ret)
{
	*ret = szl_ffi_new_at(interp,
	                      "ffi.void",
	                      SZL_FFI_FILLER_NAME(void),
	                      NULL,
	                      0,
	                      SZL_FFI_TOSTR_NAME(void),
	                      szl_ffi_scalar_proc,
	                      szl_ffi_obj_del);
	if (!*ret)
		return SZL_ERR;

	return SZL_OK;
}

/* vector types */

//SZL_NEW_TOSTR(pointer, vp, "%p", void *)
SZL_NEW_TOSTR(pointer, vp, SZL_INT_FMT, (szl_int)(intptr_t))
SZL_NEW_FILLER(pointer, vp, void *, &ffi_type_pointer)
SZL_NEW_AT(pointer, sizeof(void *))

static
enum szl_res SZL_FFI_NEW_PROC_NAME(pointer)(struct szl_interp *interp,
                                            const int objc,
                                            struct szl_obj **objv,
                                            struct szl_obj **ret)
{
	szl_int i = 0;
	void *tmp = NULL;

	if (objc == 2) {
		if (szl_obj_int(objv[1], &i) != SZL_OK)
			return SZL_ERR;

		tmp = (void *)(intptr_t)i;
	}

	return SZL_FFI_NEW_AT_NAME(pointer)(interp, &tmp, ret);
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
enum szl_res szl_ffi_new_string_at(struct szl_interp *interp,
                                   void **s,
                                   const size_t len,
                                   struct szl_obj **out)
{
	*out = szl_ffi_new_at(interp,
	                      "ffi.string",
	                      SZL_FFI_FILLER_NAME(string),
	                      s,
	                      len + 1,
	                      SZL_FFI_TOSTR_NAME(string),
	                      szl_ffi_scalar_proc,
	                      szl_ffi_string_del);
	if (!*out)
		return SZL_ERR;

	return SZL_OK;
}

static
enum szl_res SZL_FFI_NEW_PROC_NAME(string)(struct szl_interp *interp,
                                           const int objc,
                                           struct szl_obj **objv,
                                           struct szl_obj **ret)
{
	const char *op;
	szl_int addr, len = -1;
	char *s2;
	size_t slen;
	enum szl_res res;

	op = szl_obj_str(objv[1], NULL);
	if (!op)
		return SZL_ERR;

	if (strcmp("at", op) == 0) {
		if ((objc == 4) &&
		    ((szl_obj_int(objv[3], &len) != SZL_OK) ||
		     (len == 0) ||
		     (len < -1) ||
		     (len > INT_MAX)))
			goto usage;

		if (szl_obj_int(objv[2], &addr) != SZL_OK)
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
		s2 = szl_obj_strdup(objv[2], &slen);
		if (!s2)
			return SZL_ERR;
	}
	else {
usage:
		szl_usage(interp, ret, objv[0]);
		return SZL_ERR;
	}

	res = szl_ffi_new_string_at(interp, (void **)&s2, slen, ret);
	if (res != SZL_OK)
		free(s2);

	return res;
}

/* casting */

static
enum szl_res szl_ffi_new_scalar_at(struct szl_interp *interp,
                                   void *p,
                                   const char *type,
                                   struct szl_obj **out)
{
	union szl_ffi_obj_val val;

	if ((strcmp("int8", type) == 0) || (strcmp("char", type) == 0)) {
		val.i8 = *((int8_t *)p);
		return SZL_FFI_NEW_AT_NAME(int8)(interp, &val.i8, out);
	}
	else if ((strcmp("uint8", type) == 0) || (strcmp("uchar", type) == 0)) {
		val.ui8 = *((uint8_t *)p);
		return SZL_FFI_NEW_AT_NAME(uint8)(interp, &val.ui8, out);
	}
	else if (strcmp("int16", type) == 0) {
		val.i16 = *((int16_t *)p);
		return SZL_FFI_NEW_AT_NAME(int16)(interp, &val.i16, out);
	}
	else if (strcmp("uint16", type) == 0) {
		val.ui16 = *((uint16_t *)p);
		return SZL_FFI_NEW_AT_NAME(uint16)(interp, &val.ui16, out);
	}
	else if (strcmp("int32", type) == 0) {
		val.i32 = *((int32_t *)p);
		return SZL_FFI_NEW_AT_NAME(int32)(interp, &val.i32, out);
	}
	else if ((strcmp("uint32", type) == 0) || (strcmp("dword", type) == 0)) {
		val.ui32 = *((uint32_t *)p);
		return SZL_FFI_NEW_AT_NAME(uint32)(interp, &val.ui32, out);
	}
	else if (strcmp("int64", type) == 0) {
		val.i64 = *((int64_t *)p);
		return SZL_FFI_NEW_AT_NAME(int64)(interp, &val.i64, out);
	}
	else if (strcmp("uint64", type) == 0) {
		val.ui64 = *((uint64_t *)p);
		return SZL_FFI_NEW_AT_NAME(uint64)(interp, &val.ui64, out);
	}
	else if (strcmp("short", type) == 0) {
		val.s = *((short *)p);
		return SZL_FFI_NEW_AT_NAME(short)(interp, &val.s, out);
	}
	else if (strcmp("ushort", type) == 0) {
		val.us = *((unsigned short *)p);
		return SZL_FFI_NEW_AT_NAME(ushort)(interp, &val.us, out);
	}
	else if (strcmp("int", type) == 0) {
		val.i = *((int *)p);
		return SZL_FFI_NEW_AT_NAME(int)(interp, &val.i, out);
	}
	else if (strcmp("uint", type) == 0) {
		val.ui = *((unsigned int *)p);
		return SZL_FFI_NEW_AT_NAME(uint)(interp, &val.ui, out);
	}
	else if (strcmp("long", type) == 0) {
		val.l = *((long *)p);
		return SZL_FFI_NEW_AT_NAME(long)(interp, &val.l, out);
	}
	else if (strcmp("ulong", type) == 0) {
		val.ul = *((unsigned long *)p);
		return SZL_FFI_NEW_AT_NAME(ulong)(interp, &val.ul, out);
	}
	else if (strcmp("pointer", type) == 0) {
		val.vp = *((void **)p);
		return SZL_FFI_NEW_AT_NAME(pointer)(interp, &val.vp, out);
	}

	szl_set_result_fmt(interp, out, "bad ffi type: %s", type);
	return SZL_ERR;
}

static
enum szl_res szl_ffi_proc_cast(struct szl_interp *interp,
                               const int objc,
                               struct szl_obj **objv,
                               struct szl_obj **ret)
{
	const char *type;
	szl_int addr;
	size_t len;

	if (szl_obj_int(objv[1], &addr) != SZL_OK)
		return SZL_ERR;

	type = szl_obj_str(objv[2], &len);
	if (!type || !len)
		return SZL_ERR;

	return szl_ffi_new_scalar_at(interp, (void *)(intptr_t)addr, type, ret);
}

/* libraries */

static
enum szl_res szl_ffi_library_proc(struct szl_interp *interp,
                                  const int objc,
                                  struct szl_obj **objv,
                                  struct szl_obj **ret)
{
	const char *op, *sym;
	void *p;
	size_t len;

	op = szl_obj_str(objv[1], NULL);
	if (!op)
		return SZL_ERR;

	if (objc == 3) {
		if (strcmp("dlsym", op) != 0)
			goto usage;

		sym = szl_obj_str(objv[2], &len);
		if (!sym || !len)
			return SZL_ERR;

		p = dlsym(objv[0]->priv, sym);
	}
	else if (strcmp("handle", op) == 0)
		p = objv[0]->priv;
	else
		goto usage;

	*ret = szl_new_int((szl_int)(intptr_t)p);
	if (!*ret)
		return SZL_ERR;

	return SZL_OK;

usage:
	szl_usage(interp, ret, objv[0]);
	return SZL_OK;
}

static
enum szl_res szl_ffi_proc_dlopen(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv,
                                 struct szl_obj **ret)
{
	char buf[sizeof("ffi.libraryFFFFFFFFF")];
	void *h;
	const char *path;
	struct szl_obj *obj;
	size_t len;

	path = szl_obj_str(objv[1], &len);
	if (!path)
		return SZL_ERR;
	if (!len)
		path = NULL;

	h = dlopen(path, RTLD_LAZY);
	if (!h)
		return SZL_ERR;

	szl_new_obj_name(interp, "ffi.library", buf, sizeof(buf));
	obj = szl_new_proc(interp,
	                   buf,
	                   2,
	                   3,
	                   "$lib handle|dlsym ?name?",
	                   szl_ffi_library_proc,
	                   szl_unload,
	                   h);
	if (!obj) {
		dlclose(h);
		return SZL_ERR;
	}

	szl_set_result(ret, szl_obj_ref(obj));
	return SZL_OK;
}

/* functions */

static
enum szl_res szl_ffi_function_proc(struct szl_interp *interp,
                                   const int objc,
                                   struct szl_obj **objv,
                                   struct szl_obj **ret)
{
	struct szl_ffi_func *f = (struct szl_ffi_func *)(objv[0]->priv);
	void *out;
	void **args;
	szl_int tmp, out_int;
	int argc;
	int i, j;

	argc = objc - 2;
	if ((unsigned int)argc != f->argc) {
		szl_set_result_fmt(interp, ret, "wrong # of arguments: %d", argc);
		return SZL_ERR;
	}

	if (szl_obj_int(objv[1], &out_int) != SZL_OK)
		return SZL_ERR;

	out = (void *)(intptr_t)out_int;
	if (!out) {
		szl_set_result_str(interp, ret, "NULL return value address");
		return SZL_ERR;
	}

	args = (void **)malloc(sizeof(void *) * f->argc);
	if (!args)
		return SZL_ERR;

	if (f->argc) {
		for (i = 2; i < objc; ++i) {
			if (szl_obj_int(objv[i], &tmp) != SZL_OK) {
				free(args);
				return SZL_ERR;
			}

			j = i - 2;
			args[j] = (void *)(intptr_t)tmp;
			if (!args[j]) {
				free(args);
				szl_set_result_str(interp, ret, "NULL argument address");
				return SZL_ERR;
			}
		}
	}
	else
		args = NULL;

	ffi_call(&f->cif, FFI_FN(f->p), out, args);
	free(args);

	/* use the return value address as the return value, to allow one-liners */
	szl_set_result_int(interp, ret, out_int);
	return SZL_OK;
}

static
void szl_ffi_function_del(void *priv)
{
	ffi_type **atypes = ((struct szl_ffi_func *)priv)->atypes;

	if (atypes)
		free(atypes);
	free(priv);
}

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

static
enum szl_res szl_ffi_proc_function(struct szl_interp *interp,
                                   const int objc,
                                   struct szl_obj **objv,
                                   struct szl_obj **ret)
{
	char buf[sizeof("ffi.functionFFFFFFFFF")];
	struct szl_ffi_func *f;
	const char *s;
	szl_int addr;
	size_t len;
	int i, j;

	if (szl_obj_int(objv[1], &addr) != SZL_OK)
		return SZL_ERR;

	if (!addr) {
		szl_set_result_str(interp, ret, "NULL function address");
		return SZL_ERR;
	}

	s = szl_obj_str(objv[2], &len);
	if (!s || !len)
		return SZL_ERR;

	f = (struct szl_ffi_func *)malloc(sizeof(*f));
	if (!f)
		return SZL_ERR;

	f->rtype = szl_ffi_get_type(s);
	if (!f->rtype) {
		szl_set_result_fmt(interp, ret, "bad ret type: %s", s);
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
			s = szl_obj_str(objv[i], &len);
			if (!s || !len) {
				free(f->atypes);
				free(f);
				return SZL_ERR;
			}

			j = i - 3;
			f->atypes[j] = szl_ffi_get_type(s);
			if (!f->atypes[j] || (f->atypes[j] == &ffi_type_void)) {
				szl_set_result_fmt(interp, ret, "bad arg type: %s", s);
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

	szl_new_obj_name(interp, "ffi.function", buf, sizeof(buf));
	*ret = szl_new_proc(interp,
	                    buf,
	                    2 + f->argc, /* procedure, return value and arguments */
	                    2 + f->argc,
	                    "$func ret ?arg...?",
	                    szl_ffi_function_proc,
	                    szl_ffi_function_del,
	                    f);
	if (!*ret) {
		if (f->atypes)
			free(f->atypes);
		free(f);
		return SZL_ERR;
	}

	f->p = (void *)(intptr_t)addr;
	szl_obj_ref(*ret);
	return SZL_OK;
}

enum szl_res szl_init_ffi(struct szl_interp *interp)
{
	if ((!SZL_FFI_NEW_OBJ_PROC(int8)) ||
	    (!SZL_FFI_NEW_OBJ_PROC_ALIAS(int8, char)) ||
	    (!SZL_FFI_NEW_OBJ_PROC(uint8)) ||
	    (!SZL_FFI_NEW_OBJ_PROC_ALIAS(uint8, uchar)) ||
	    (!SZL_FFI_NEW_OBJ_PROC(int16)) ||
	    (!SZL_FFI_NEW_OBJ_PROC(uint16)) ||
	    (!SZL_FFI_NEW_OBJ_PROC_ALIAS(uint32, dword)) ||
	    (!SZL_FFI_NEW_OBJ_PROC(int32)) ||
	    (!SZL_FFI_NEW_OBJ_PROC(uint32)) ||
	    (!SZL_FFI_NEW_OBJ_PROC(int64)) ||
	    (!SZL_FFI_NEW_OBJ_PROC(uint64)) ||
	    (!SZL_FFI_NEW_OBJ_PROC(short)) ||
	    (!SZL_FFI_NEW_OBJ_PROC(ushort)) ||
	    (!SZL_FFI_NEW_OBJ_PROC(int)) ||
	    (!SZL_FFI_NEW_OBJ_PROC(uint)) ||
	    (!SZL_FFI_NEW_OBJ_PROC(long)) ||
	    (!SZL_FFI_NEW_OBJ_PROC(ulong)) ||
	    (!SZL_FFI_NEW_OBJ_PROC(pointer)) ||
	    (!szl_new_proc(interp,
	                   "ffi.void",
	                   1,
	                   1,
	                   "ffi.void",
	                   SZL_FFI_NEW_PROC_NAME(void),
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "ffi.string",
	                   3,
	                   4,
	                   "ffi.string at|copy addr|obj ?len?",
	                   SZL_FFI_NEW_PROC_NAME(string),
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "ffi.cast",
	                   3,
	                   3,
	                   "ffi.cast addr type",
	                   szl_ffi_proc_cast,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "ffi.dlopen",
	                   2,
	                   2,
	                   "ffi.dlopen path",
	                   szl_ffi_proc_dlopen,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "ffi.function",
	                   3,
	                   2 + SZL_FFI_MAX_FUNC_ARGC,
	                   "ffi.function addr rtype ?type...?",
	                   szl_ffi_proc_function,
	                   NULL,
	                   NULL)))
		return SZL_ERR;

	return SZL_OK;
}
