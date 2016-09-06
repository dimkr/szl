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

/**
 * @file szl.h
 * szl interpreter API
 */

#ifndef _SZL_H_INCLUDED
#	define _SZL_H_INCLUDED

#	include <inttypes.h>
#	include <sys/types.h>
#	include <limits.h>
#	include <stdarg.h>
#	include <stdio.h>
#	include <wchar.h>

/**
 * @mainpage Welcome
 * This is documentation of the external szl interpreter API, for embedding of
 * the interpreter and development of extensions.
 *
 * @defgroup high_level High-level API
 * @defgroup low_level Low-level API
 * @defgroup internals Internal implementation details
 *
 * @defgroup util Utilities
 * @ingroup internals
 * @{
 */

/**
 * @def SZL_PASTE
 * Stringifies a token
 */
#	define _SZL_PASTE(x) # x
#	define SZL_PASTE(x) _SZL_PASTE(x)

/**
 * @def szl_likely
 * Improves branch prediction when a condition is likely to be met
 */
#	define szl_likely(cond) __builtin_expect((cond), 1)

/**
 * @def szl_unlikely
 * Improves branch prediction when a condition is unlikely
 */
#	define szl_unlikely(cond) __builtin_expect((cond), 0)

/**
 * @}
 *
 * @defgroup limits Artificial Limits
 * @ingroup internals
 * @{
 */

/**
 * @def SZL_MAX_EXT_INIT_FUNC_NAME_LEN
 * The maximum length of an extension initialization function name
 */
#	define SZL_MAX_EXT_INIT_FUNC_NAME_LEN 32

/**
 * @def SZL_MAX_NESTING
 * The maximum nesting level of procedure calls
 */
#	define SZL_MAX_NESTING 64

/**
 * @}
 *
 * @defgroup syntax Syntax
 * @ingroup internals
 * @{
 */

/**
 * @def SZL_COMMENT_PFIX
 * The first character of comments
 */
#	define SZL_COMMENT_PFIX '#'

/**
 * @def SZL_OBJV_NAME
 * The name of the special object containing the arguments of a procedure
 */
#	define SZL_OBJV_NAME "@"

/**
 * @def SZL_LAST_NAME
 * The name of the special object containing the previously called procedure's
 * return value
 */
#	define SZL_LAST_NAME "_"

/**
 * @fn int szl_isspace(const char ch)
 * @brief Checks if a character is considered whitespace
 * @param ch [in] A character
 * @return Non-zero if the character is whitespace, otherwise zero
 */
int szl_isspace(const char ch);

/**
 * @}
 *
 * @defgroup types Data types
 * @ingroup internals
 * @{
 */

/**
 * @typedef szl_int
 * The type used for representing szl objects as integers
 */
typedef intmax_t szl_int;

/**
 * @def SZL_INT_MAX
 * The maximum value of an szl_int
 */
#	define SZL_INT_MAX INTMAX_MAX

/**
 * @def SZL_INT_FMT
 * The format string for printing @ref szl_int
 */
#	define SZL_INT_FMT "%jd"

/**
 * @def SZL_INT_SCANF_FMT
 * The format string for converting an @ref szl_int into a string
 */
#	define SZL_INT_SCANF_FMT SZL_INT_FMT

/**
 * @typedef szl_float
 * The type used for representing szl objects as floating-point numbers
 */
typedef double szl_float;

/**
 * @def SZL_FLOAT_FMT
 * The format string for printing @ref szl_float
 */
#	define SZL_FLOAT_FMT "%.12f"

/**
 * @def SZL_FLOAT_SCANF_FMT
 * The format string for converting an @ref szl_float into a string
 */
#	define SZL_FLOAT_SCANF_FMT "%lf"

/**
 * @enum szl_types
 * Object data types
 */
enum szl_types {
	/* must correspond to szl_cast_table[][] */
	SZL_TYPE_STR    = 1, /**< String */
	SZL_TYPE_WSTR   = 2, /**< Wide-character string; used by string procedures */
	SZL_TYPE_LIST   = 3, /**< List */
	SZL_TYPE_INT    = 4, /**< Integer */
	SZL_TYPE_FLOAT  = 5, /**< Floating-point number */

	/* used internally during code evaluation */
	SZL_TYPE_CODE   = 6, /**< Code block */

	/* only used by szl_new_ext() */
	SZL_TYPE_PROC   = 7 /**< Procedure */
};

/**
 * @struct szl_val
 * Values of an object
 */
struct szl_val {
	struct {
		char *buf;
		size_t len;
	} s; /**< String value */
	struct {
		wchar_t *buf;
		size_t len;
	} ws; /**< Wide-character string value */
	struct {
		struct szl_obj **items;
		size_t len;
		int sorted;
	} l; /**< List value */
	struct szl_obj *c; /**< Code value */
	szl_int i; /**< Integer value */
	szl_float f; /**< Floating-point value */
};

/**
 * @enum szl_res
 * Procedure status codes, which control the flow of execution
 */
enum szl_res {
	SZL_OK, /**< Success */
	SZL_ERR, /**< Error: the next statement shall not be executed */
	SZL_BREAK, /**< Success, but the next statement shall not be executed */
	SZL_CONT, /**< Jump back to the first statement in the loop body */
	SZL_RET, /**< Stop execution of statements in the current block and
	              stop the caller with SZL_BREAK */
	SZL_EXIT /**< Exit the script */
};

struct szl_obj;
struct szl_interp;

/**
 * @typedef szl_proc_t
 * A C function implementing a szl procedure
 */
typedef enum szl_res (*szl_proc_t)(struct szl_interp *,
                                   const unsigned int,
                                   struct szl_obj **);

/**
 * @typedef szl_del_t
 * A cleanup function called when an object is freed
 */
typedef void (*szl_del_t)(void *);

/**
 * @struct szl_obj
 * A szl object
 */
struct szl_obj {
	unsigned int refc; /**< The object reference count; the object is freed when it drops to zero */
	uint32_t hash; /**< The object's string representation hash */
	int hashed; /**< A flag set when the object is hashed and unset upon modification */
	int ro; /**< A flag set when an object is marked as read-only */

	unsigned int types; /**< Available representations of the object */
	struct szl_val val; /**< The object values */

	void *priv; /**< Private data used by @ref proc and freed by @ref del */
	szl_proc_t proc; /**< C procedure implementation */
	int min_objc; /**< The minimum number of arguments to @ref proc or -1 */
	int max_objc; /**< The maximum number of arguments to @ref proc or -1 */
	const char *help; /**< A message shown upon incorrect usage */
	szl_del_t del; /**< Cleanup callback which frees @ref priv */

	struct szl_obj *caller; /**< The calling frame */
	struct szl_obj *locals; /**< Local objects */
	struct szl_obj *args; /**< The calling statement */
};

/**
 * @struct szl_interp
 * A szl interpreter
 */
struct szl_interp {
	struct szl_obj *last; /**< The previously called statement's return value */

	struct szl_obj *empty; /**< An empty string singleton */
	struct szl_obj *space; /**< A " " singleton */
	struct szl_obj *nums[16]; /**< Integer singletons */
	struct szl_obj *sep; /**< A "/" singleton */
	struct szl_obj *_; /**< A "_" singleton */
	struct szl_obj *args; /**< A SZL_OBJV_NAME singleton */

	struct szl_obj *global; /**< The global frame */
	struct szl_obj *current; /**< The currently running procedure */
	unsigned int depth; /**< The call stack depth */

	struct szl_obj *exts; /**< Loaded extensions */
	struct szl_obj *libs; /**< Loaded extension shared objects */
};

/**
 * @}
 *
 * @defgroup refc Reference Counting
 * @ingroup low_level
 * @{
 */

/**
 * @fn struct szl_obj *szl_ref(struct szl_obj *obj)
 * @brief Increments the reference count of an object
 * @param obj [in,out] An object
 * @return The passed object pointer
 */
struct szl_obj *szl_ref(struct szl_obj *obj);

/**
 * @fn void szl_unref(struct szl_obj *obj)
 * @brief Decrements the reference count of an object and frees it if the
 *        reference count reaches zero
 * @param obj [in,out] An object
 */
void szl_unref(struct szl_obj *obj);

/**
 * @}
 *
 * @defgroup cast Casting
 * @ingroup high_level
 * @{
 */

/**
 * @fn int szl_as_str(struct szl_interp *interp,
 *                    struct szl_obj *obj,
 *                    char **buf,
 *                    size_t *len)
 * @brief Returns the string representation of an object
 * @param interp [in,out] An interpreter
 * @param obj [in,out] The object
 * @param buf [out] The return value
 * @param len [out] The string length or NULL if not needed
 * @return A C string or NULL
 */
int szl_as_str(struct szl_interp *interp,
               struct szl_obj *obj,
               char **buf,
               size_t *len);

/**
 * @fn int szl_as_wstr(struct szl_interp *interp,
 *                     struct szl_obj *obj,
 *                     wchar_t **ws,
 *                     size_t *len)
 * @brief Returns the wide-character string representation of an object
 * @param interp [in,out] An interpreter
 * @param obj [in,out] The object
 * @param ws [out] The return value
 * @param len [out] The string length or NULL if not needed
 * @return A wide-character C string or NULL
 */
int szl_as_wstr(struct szl_interp *interp,
                struct szl_obj *obj,
                wchar_t **ws,
                size_t *len);


/**
 * @fn int szl_as_list(struct szl_interp *interp,
 *                     struct szl_obj *obj,
 *                     struct szl_obj ***items,
 *                     size_t *len)
 * @brief Returns the list representation of an object
 * @param interp [in,out] An interpreter
 * @param obj [in,out] The object
 * @param items [out] An array of list items
 * @param len [out] The list length
 * @return 1 or 0
 */
int szl_as_list(struct szl_interp *interp,
                struct szl_obj *obj,
                struct szl_obj ***items,
                size_t *len);

/**
 * @fn int szl_as_dict(struct szl_interp *interp,
 *                     struct szl_obj *obj,
 *                     struct szl_obj ***items,
 *                     size_t *len)
 * @brief Returns the dictionary representation of an object
 * @param interp [in,out] An interpreter
 * @param obj [in,out] The object
 * @param items [out] An array of dictionary keys and values
 * @param len [out] The number of dictionary keys and values
 * @return 1 or 0
 */
int szl_as_dict(struct szl_interp *interp,
                struct szl_obj *obj,
                struct szl_obj ***items,
                size_t *len);

/**
 * @fn int szl_as_int(struct szl_interp *interp,
 *                    struct szl_obj *obj,
 *                    szl_int *i)
 * @brief Returns the integer representation of an object
 * @param interp [in,out] An interpreter
 * @param obj [in,out] The object
 * @param i [out] The integer representation
 * @return 1 or 0
 */
int szl_as_int(struct szl_interp *interp,
               struct szl_obj *obj,
               szl_int *i);

/**
 * @fn int szl_as_float(struct szl_interp *interp,
 *                      struct szl_obj *obj,
 *                      szl_float *f)
 * @brief Returns the C floating point number representation of an object
 * @param interp [in,out] An interpreter
 * @param obj [in,out] The object
 * @param f [out] The floating point representation
 * @return 1 or 0
 */
int szl_as_float(struct szl_interp *interp,
                 struct szl_obj *obj,
                 szl_float *f);

/**
 * @fn int szl_as_bool(struct szl_obj *obj, int *b)
 * @brief Returns the truth value of an object
 * @param obj [in,out] The object
 * @param b [out] The return value
 * @return 1 or 0
 */
int szl_as_bool(struct szl_obj *obj, int *b);


/**
 * @}
 *
 * @defgroup new Object Creation
 * @ingroup high_level
 * @{
 */

/**
 * @fn struct szl_obj *szl_new_str(const char *buf, ssize_t len)
 * @brief Creates a new string object, by copying an existing C string
 * @param buf [in] The string
 * @param len [in] The string length or -1 if unknown
 * @return A new reference to the created string object or NULL
 */
struct szl_obj *szl_new_str(const char *buf, ssize_t len);

/**
 * @fn struct szl_obj *szl_new_wstr(const wchar_t *ws, ssize_t len)
 * @brief Creates a new wide-character string object, by copying an existing C
 *        string
 * @param ws [in] The wide-character string
 * @param len [in] The string length or -1 if unknown
 * @return A new reference to the created wide-character string object or NULL
 */
struct szl_obj *szl_new_wstr(const wchar_t *ws, ssize_t len);

/**
 * @fn struct szl_obj *szl_new_str_noalloc(char *buf, const size_t len)
 * @brief Creates a new string object
 * @param buf [in] The string, allocated using malloc()
 * @param len [in] The string length
 * @return A new reference to the created string object or NULL
 * @note buf is freed automatically upon success
 */
struct szl_obj *szl_new_str_noalloc(char *buf, const size_t len);

/**
 * @fn struct szl_obj *szl_new_str_fmt(const char *fmt, ...)
 * @brief Creates a new string object, using a format string
 * @param fmt in] The format string
 * @return A new reference to the created string object or NULL
 */
struct szl_obj *szl_new_str_fmt(const char *fmt, ...);

/**
 * @fn struct szl_obj *szl_new_int(struct szl_interp *interp, const szl_int i)
 * @brief Creates a new integer object
 * @param interp [in,out] An interpreter
 * @param i [in] The integer value
 * @return A new reference to the created integer object or NULL
 */
struct szl_obj *szl_new_int(struct szl_interp *interp, const szl_int i);

/**
 * @fn struct szl_obj *szl_new_float(const szl_float f)
 * @brief Creates a new floating point number object
 * @param f [in] The floating point number value
 * @return A new reference to the created floating point number object or NULL
 */
struct szl_obj *szl_new_float(const szl_float f);

/**
 * @def szl_new_empty
 * Creates a new, empty string object
 */
#	define szl_new_empty() szl_new_str("", 0)

/**
 * @fn struct szl_obj *szl_new_list(struct szl_obj **objv, const size_t len)
 * @brief Creates a new list object
 * @param objv [in,out] The list items
 * @param len [in] The list length
 * @return A new reference to the created list object or NULL
 */
struct szl_obj *szl_new_list(struct szl_obj **objv, const size_t len);

/**
 * @def szl_new_dict
 * Creates a new dictionary object
 */
#	define szl_new_dict szl_new_list

/**
 * @fn struct szl_obj *szl_new_proc(struct szl_interp *interp,
 *                                  struct szl_obj *name,
 *                                  const int min_argc,
 *                                  const int max_argc,
 *                                  const char *help,
 *                                  szl_proc_t proc,
 *                                  szl_del_t del,
 *                                  void *priv)
 * @brief Creates a new procedure object and registers it in the local scope
 * @param interp [in,out] An interpreter
 * @param name [in,out] The procedure name
 * @param min_argc [in] The minimum number of arguments or -1
 * @param max_argc [in] The maximum number of arguments or -1
 * @param help [in] A help message which explains how to use the procedure
 * @param proc [in] A C callback implementing the procedure
 * @param del [in] A cleanup callback for the procedure private data or NULL
 * @param priv [in] Private data for use by proc and del, or NULL
 * @return A new reference to the created procedure object or NULL
 */
struct szl_obj *szl_new_proc(struct szl_interp *interp,
                             struct szl_obj *name,
                             const int min_objc,
                             const int max_objc,
                             const char *help,
                             szl_proc_t proc,
                             szl_del_t del,
                             void *priv);

/**
 * @fn int szl_set_args(struct szl_interp *interp,
 *                      struct szl_obj *call,
 *                      struct szl_obj *args)
 * @brief Sets the arguments of a procedure
 * @param interp [in,out] An interpreter
 * @param call [in,out] The procedure call
 * @param args [in,out] The procedure arguments
 * @return 1 or 0
 */
int szl_set_args(struct szl_interp *interp,
                 struct szl_obj *call,
                 struct szl_obj *args);

/**
 * @}
 *
 * @defgroup op Object Operations
 * @ingroup high_level
 * @{
 */

/**
 * @def szl_set_ro
 * Marks an object as read-only
 */
#	define szl_set_ro(obj) obj->ro = 1

/**
 * @defgroup str_op String Operations
 * @{
 */

/**
 * @fn int szl_len(struct szl_interp *interp, struct szl_obj *obj, size_t *len)
 * @brief Returns the length of the string representation of an object
 * @param interp [in,out] An interpreter
 * @param obj [in,out] The object
 * @param len [out] The string length
 * @return 1 or 0
 */
int szl_len(struct szl_interp *interp, struct szl_obj *obj, size_t *len);

/**
 * @fn char *szl_strdup(struct szl_interp *interp,
 *                      struct szl_obj *obj,
 *                      size_t *len)
 * @brief Copies the string representation of an object
 * @param interp [in,out] An interpreter
 * @param obj [in,out] The object
 * @param len [out] The string length or NULL if not needed
 * @return A newly allocated C string or NULL
 * @note The return value must be freed using free()
 */
char *szl_strdup(struct szl_interp *interp, struct szl_obj *obj, size_t *len);

/**
 * @fn int szl_eq(struct szl_interp *interp,
 *                struct szl_obj *a,
 *                struct szl_obj *b,
 *                int *eq)
 * @brief Determines whether two objects are equal
 * @param interp [in,out] An interpreter
 * @param a [in,out] An object
 * @param b [in,out] An object
 * @param eq [out] The return value
 * @return 1 or 0
 */
int szl_eq(struct szl_interp *interp,
           struct szl_obj *a,
           struct szl_obj *b,
           int *eq);

/**
 * @fn int szl_str_append(struct szl_interp *interp,
 *                        struct szl_obj *dest,
 *                        struct szl_obj *src)
 * @brief Appends a string to a string object
 * @param interp [in,out] An interpreter
 * @param dest [in,out] The string object
 * @param src [in,out] The appended string
 * @return 1 or 0
 */
int szl_str_append(struct szl_interp *interp,
                   struct szl_obj *dest,
                   struct szl_obj *src);

/**
 * @fn int szl_str_append_str(struct szl_interp *interp,
 *                            struct szl_obj *dest,
 *                            const char *src,
 *                            const size_t len)
 * @brief Appends a string to a string object
 * @param interp [in,out] An interpreter
 * @param dest [in,out] The string object
 * @param src [in] The appended string
 * @param len [in] The appended string length
 * @return 1 or 0
 */
int szl_str_append_str(struct szl_interp *interp,
                       struct szl_obj *dest,
                       const char *buf,
                       const size_t len);

/**
 * @}
 *
 * @defgroup list_op List Operations
 * @{
 */

/**
 * @fn int szl_list_append(struct szl_interp *interp,
 *                         struct szl_obj *list,
 *                         struct szl_obj *item)
 * @brief Appends an object to a list object
 * @param interp [in,out] An interpreter
 * @param list [in,out] The list
 * @param item [in,out] The object
 * @return 1 or 0
 */
int szl_list_append(struct szl_interp *interp,
                    struct szl_obj *list,
                    struct szl_obj *item);

/**
 * @fn int szl_list_append_str(struct szl_interp *interp,
 *                             struct szl_obj *list,
 *                             const char *buf,
 *                             const ssize_t len)
 * @brief Appends a string to a list object
 * @param interp [in,out] An interpreter
 * @param list [in,out] The list object
 * @param buf [in] The string
 * @param len [in] The string length or -1
 * @return 1 or 0
 */
int szl_list_append_str(struct szl_interp *interp,
                        struct szl_obj *list,
                        const char *buf,
                        const ssize_t len);

/**
 * @fn int szl_list_append_int(struct szl_interp *interp,
 *                             struct szl_obj *list,
 *                             const szl_int i)
 * @brief Appends an integer to a list object
 * @param interp [in,out] An interpreter
 * @param list [in,out] The list object
 * @param i [in] The integer
 * @return 1 or 0
 */
int szl_list_append_int(struct szl_interp *interp,
                        struct szl_obj *list,
                        const szl_int i);

/**
 * @fn int szl_list_set(struct szl_interp *interp,
 *                      struct szl_obj *list,
 *                      const szl_int index,
 *                      struct szl_obj *item)
 * @brief Replaces a list item
 * @param interp [in,out] An interpreter
 * @param list [in,out] The list
 * @param index [in] The item index
 * @param item [in,out] The new value
 * @return 1 or 0
 */
int szl_list_set(struct szl_interp *interp,
                 struct szl_obj *list,
                 const szl_int index,
                 struct szl_obj *item);

/**
 * @fn int szl_list_extend(struct szl_interp *interp,
 *                         struct szl_obj *dest,
 *                         struct szl_obj *src)
 * @brief Appends all items of a list to another list
 * @param interp [in,out] An interpreter
 * @param dest [in,out] A list
 * @param src [in,out] The appended list
 * @return 1 or 0
 */
int szl_list_extend(struct szl_interp *interp,
                    struct szl_obj *dest,
                    struct szl_obj *src);

/**
 * @fn int szl_list_in(struct szl_interp *interp,
 *                     struct szl_obj *item,
 *                     struct szl_obj *list,
 *                     int *in)
 * @brief Checks whether an object appears in a list
 * @param interp [in,out] An interpreter
 * @param item [in,out] The object
 * @param list [in,out] The list
 * @param in [out] The return value
 * @return 1 or 0
 */
int szl_list_in(struct szl_interp *interp,
                struct szl_obj *item,
                struct szl_obj *list,
                int *in);

/**
 * @fn struct szl_obj *szl_join(struct szl_interp *interp,
 *                              struct szl_obj *delim,
 *                              struct szl_obj **objv,
 *                              const size_t objc,
 *                              const int wrap)
 * @brief Creates a new string by joining objects
 * @param interp [in,out] An interpreter
 * @param delim [in] A delimiter to put between object values
 * @param objv [in,out] The objects to join
 * @param objc [in] The number of objects
 * @param wrap [in] 0 if items that contain spaces should not be surrounded with
 *                  braces
 * @return A new reference to a list object or NULL
 */
struct szl_obj *szl_join(struct szl_interp *interp,
                         struct szl_obj *delim,
                         struct szl_obj **objv,
                         const size_t objc,
                         const int wrap);

/**
 * @}
 *
 * @defgroup dict_op Dictionary Operations
 * @{
 */

/**
 * @fn int szl_dict_set(struct szl_interp *interp,
 *                      struct szl_obj *dict,
 *                      struct szl_obj *k,
 *                      struct szl_obj *v)
 * @brief Sets a dictionary item
 * @param interp [in,out] An interpreter
 * @param dict [in,out] The dictionary object
 * @param k [in,out] The key
 * @param v [in,out] The value
 * @return 1 or 0
 */
int szl_dict_set(struct szl_interp *interp,
                 struct szl_obj *dict,
                 struct szl_obj *k,
                 struct szl_obj *v);

/**
 * @fn int szl_dict_get(struct szl_interp *interp,
 *                      struct szl_obj *dict,
 *                      struct szl_obj *k,
 *                  struct szl_obj **v)
 * @brief Fetches a dictionary item by its key
 * @param interp [in,out] An interpreter
 * @param dict [in,out] The dictionary object
 * @param k [in,out] The key
 * @param v [out] The value
 * @return 1 or 0
 */
int szl_dict_get(struct szl_interp *interp,
                 struct szl_obj *dict,
                 struct szl_obj *k,
                 struct szl_obj **v);

/**
 * @}
 */

/**
 * @}
 *
 * @defgroup interp Interpreters
 * @ingroup high_level
 * @{
 */

/**
 * @fn struct szl_interp *szl_new_interp(int argc, char *argv[])
 * @brief Creates a new interpreter
 * @param argc [in] The number of elements in argv[]
 * @param argv [in] The command-line arguments the interpreter was invoked with
 * @return A new interpreter structure or NULL
 */
struct szl_interp *szl_new_interp(int argc, char *argv[]);

/**
 * @fn void szl_free_interp(struct szl_interp *interp)
 * @brief Frees all memory associated with an interpreter
 * @param interp [in] An interpreter
 */
void szl_free_interp(struct szl_interp *interp);

/**
 * @def szl_caller
 * Returns the frame that called the currently running procedure or the global
 * frame
 */
#	define szl_caller(interp) \
	(interp->current->caller ? interp->current->caller : interp->current)

/**
 * @}
 *
 * @defgroup ret Return Values
 * @ingroup high_level
 * @{
 */

/**
 * @fn enum szl_res szl_set_last(struct szl_interp *interp, struct szl_obj *obj)
 * @brief Sets the return value of a procedure
 * @param interp [in,out] An interpreter
 * @param obj [in,out] A new reference to the return value
 * @return SZL_OK
 */
enum szl_res szl_set_last(struct szl_interp *interp, struct szl_obj *obj);

/**
 * @fn void szl_empty_last(struct szl_interp *interp)
 * @brief Sets the return value of a procedure to an empty string
 * @param interp [in,out] An interpreter
 */
void szl_empty_last(struct szl_interp *interp);

/**
 * @fn enum szl_res szl_set_last_int(struct szl_interp *interp, const szl_int i)
 * @brief Sets the return value of a procedure to a new integer object
 * @param interp [in,out] An interpreter
 * @param i [in] The integer
 * @return SZL_OK or SZL_ERR
 */
enum szl_res szl_set_last_int(struct szl_interp *interp, const szl_int i);

/**
 * @fn enum szl_res szl_set_last_float(struct szl_interp *interp,
 *                                     const szl_float f)
 * @brief Sets the return value of a procedure to a new floating point number
 *        object
 * @param interp [in,out] An interpreter
 * @param f [in] The floating point number
 * @return SZL_OK or SZL_ERR
 */
enum szl_res szl_set_last_float(struct szl_interp *interp, const szl_float f);

/**
 * @fn enum szl_res szl_set_last_bool(struct szl_interp *interp, const int b)
 * @brief Sets the return value of a procedure to an integer object, carrying
 *        the value of 1 or 0
 * @param interp [in,out] An interpreter
 * @param b [in] The boolean value
 * @return SZL_OK
 */
enum szl_res szl_set_last_bool(struct szl_interp *interp, const int b);

/**
 * @fn enum szl_res szl_set_last_str(struct szl_interp *interp,
 *                                   const char *s,
 *                                   const ssize_t len);
 * @brief Sets the return value of a procedure to a new string object
 * @param interp [in,out] An interpreter
 * @param s [in] The string
 * @param len [in] The string length or -1
 * @return SZL_OK or SZL_ERR
 */
enum szl_res szl_set_last_str(struct szl_interp *interp,
                              const char *s,
                              const ssize_t len);

/**
 * @fn enum szl_res szl_set_last_wstr(struct szl_interp *interp,
 *                                    const wchar_t *ws,
 *                                    const ssize_t len);
 * @brief Sets the return value of a procedure to a new wide-character string
 *        object
 * @param interp [in,out] An interpreter
 * @param ws [in] The wide-character string
 * @param len [in] The string length or -1
 * @return SZL_OK or SZL_ERR
 */
enum szl_res szl_set_last_wstr(struct szl_interp *interp,
                               const wchar_t *ws,
                               const ssize_t len);


/**
 * @fn enum szl_res szl_set_last_fmt(struct szl_interp *interp,
 *                                   const char *fmt,
 *                                   ...)
 * @brief Sets the return value of a procedure to a newly created,
 *        printf()-style formatted string object
 * @param interp [in,out] An interpreter
 * @param fmt [in] The format string
 * @return SZL_OK or SZL_ERR
 */
enum szl_res szl_set_last_fmt(struct szl_interp *interp,
                              const char *fmt,
                              ...);

/**
 * @fn enum szl_res szl_set_last_help(struct szl_interp *interp,
 *                                    struct szl_obj *proc)
 * @brief Sets the return value of a procedure a new string object specifying a
 *        help message
 * @param interp [in,out] An interpreter
 * @param proc [in] The calling procedure
 * @return SZL_ERR
 */
enum szl_res szl_set_last_help(struct szl_interp *interp,
                               struct szl_obj *proc);

/**
 * @}
 *
 * @defgroup locals Named Objects
 * @ingroup high_level
 * @{
 */

/**
 * @def szl_set
 * Registers a local object
 */
#	define szl_set(interp, proc, name, val) \
	szl_dict_set(interp, proc->locals, name, val)

/**
 * @fn int szl_get(struct szl_interp *interp,
 *                 struct szl_obj *name,
 *                 struct szl_obj **obj);
 * @brief Searches for an object by name and returns a new reference to it
 * @param interp [in,out] An interpreter
 * @param name [in,out] The object name
 * @param obj [out] The object
 * @return 1 or 0
 */
int szl_get(struct szl_interp *interp,
            struct szl_obj *name,
            struct szl_obj **obj);

/**
 * @fn enum szl_res szl_eval(struct szl_interp *interp, struct szl_obj *obj)
 * @brief Evaluates a single expression
 * @param interp [in,out] An interpreter
 * @param obj [in] The expression
 * @return A member of @ref szl_res
 */
enum szl_res szl_eval(struct szl_interp *interp, struct szl_obj *obj);

/**
 * @}
 *
 * @defgroup eval Parsing and Evaluation
 * @ingroup high_level
 * @{
 */

/**
 * @fn enum enum szl_res szl_run(struct szl_interp *interp,
 *                               const char *buf,
 *                               const size_t len)
 * @brief Runs a szl snippet
 * @param interp [in,out] An interpreter
 * @param buf [in] The snippet
 * @param len [in] The snippet length
 * @return A member of @ref szl_res
 * @note This function should not be used when the snippet should run more than
 *       once; use @ref szl_run_obj instead
 */
enum szl_res szl_run(struct szl_interp *interp,
                     const char *buf,
                     const size_t len);

/**
 * @fn enum enum szl_res szl_run_obj(struct szl_interp *interp,
 *                                   struct szl_obj *obj)
 * @brief Runs a szl snippet contained in a string object
 * @param interp [in,out] An interpreter
 * @param obj [in,out] The snippet
 * @return A member of @ref szl_res
 */
enum szl_res szl_run_obj(struct szl_interp *interp, struct szl_obj *obj);

/**
 * @}
 *
 * @defgroup ext Extensions
 * @ingroup low_level
 * @{
 */

/**
 * @struct szl_ext_export
 * An object exported by an extension
 */
struct szl_ext_export {
	const char *name; /**< The object name */
	enum szl_types type; /**< The object type */
	union {
		struct {
			int min_objc;
			int max_objc;
			const char *help;
			szl_proc_t proc;
			szl_del_t del;
			void *priv;
		} proc; /**< Procedure */
		struct {
			const char *buf;
			size_t len;
		} s; /**< String */
		szl_int i; /**< Integer */
		szl_float f; /**< Floating-point number */
	} val; /**< The object value */
};

/**
 * @def SZL_STR_INIT
 * Initializes a string @ref szl_ext_export
 */
#	define SZL_STR_INIT(_name, _buf) \
	.name = _name,                   \
	.type = SZL_TYPE_STR,            \
	.val.s.buf = _buf,               \
	.val.s.len = sizeof(_buf) - 1

/**
 * @def SZL_INT_INIT
 * Initializes an integer @ref szl_ext_export
 */
#	define SZL_INT_INIT(_name, _i) \
	.name = _name,                 \
	.type = SZL_TYPE_INT,          \
	.val.i = _i

/**
 * @def SZL_PROC_INIT
 * Initializes a procedure @ref szl_ext_export
 */
#	define SZL_PROC_INIT(_name, _help, _min_objc, _max_argc, _proc, _del) \
	.name = _name,                                                        \
	.type = SZL_TYPE_PROC,                                                \
	.val.proc.min_objc = _min_objc,                                       \
	.val.proc.max_objc = _max_argc,                                       \
	.val.proc.help = _help,                                               \
	.val.proc.proc = _proc,                                               \
	.val.proc.del = _del

/**
 * @fn int szl_new_ext(struct szl_interp *interp,
 *                     const char *name,
 *                     const struct szl_ext_export *exports,
 *                     const unsigned int n)
 * @brief Initializes a C extension
 * @param interp [in,out] An interpreter
 * @param name [in] The extension name
 * @param exports [in] Objects exported by the extension
 * @param n [in] The number of objects exported by the extension
 * @return 1 or 0
 */
int szl_new_ext(struct szl_interp *interp,
                const char *name,
                const struct szl_ext_export *exports,
                const unsigned int n);

/**
 * @def SZL_EXT_INIT_FUNC_NAME_FMT
 * The format of extension initialization function names
 */
#	define SZL_EXT_INIT_FUNC_NAME_FMT "szl_init_%s"

/**
 * @def SZL_EXT_PATH_FMT
 * The format of extension paths
 */
#	define SZL_EXT_PATH_FMT SZL_EXT_DIR"/szl_%s.so"

/**
 * @typedef szl_ext_init
 * The prototype of an extension initialization function
 */
typedef int (*szl_ext_init)(struct szl_interp *);

/**
 * @fn int szl_load(struct szl_interp *interp, const char *name)
 * @brief Loads an extension
 * @param interp [in,out] An interpreter
 * @param name [in] The extension name
 * @return 1 or 0
 */
int szl_load(struct szl_interp *interp, const char *name);

/**
 * @fn void szl_unload(void *h)
 * @brief Unloads an extension
 * @param h [in]: The extension shared object handle
 * @note Should be used by extensions that load shared objects, as a
 *       @ref szl_del_t
 */
void szl_unload(void *h);

/**
 * @fn enum szl_res szl_source(struct szl_interp *interp, const char *path)
 * @brief Runs a script in the current scope
 * @param interp [in,out] An interpreter
 * @param path [in] The script path
 * @return A member of @ref szl_res
 */
enum szl_res szl_source(struct szl_interp *interp, const char *path);

/**
 * @}
 *
 * @defgroup strm I/O streams
 * @ingroup low_level
 * @{
 */

/**
 * @def SZL_STREAM_BUFSIZ
 * The internal buffer size of binary streams
 */
#	if BUFSIZ > 64 * 1024
#		define SZL_STREAM_BUFSIZ BUFSIZ
#	else
#		define SZL_STREAM_BUFSIZ 64 * 1024
#	endif

struct szl_stream;

/**
 * @struct szl_stream_ops
 * The underlying, transport-specific implementation of an I/O stream
 */
struct szl_stream_ops {
	ssize_t (*read)(struct szl_interp *,
	                void *,
	                unsigned char *,
	                const size_t,
	                int *); /**< Reads a buffer from the stream */
	ssize_t (*write)(struct szl_interp *,
	                 void *,
	                 const unsigned char *,
	                 const size_t); /**< Writes a buffer to the stream */
	enum szl_res (*flush)(void *); /**< Optional, flushes the output buffer */
	void (*close)(void *); /**< Closes the stream */
	int (*accept)(struct szl_interp *, void *, struct szl_stream **); /**< Optional, accepts a client */
	szl_int (*handle)(void *); /**< Returns the underlying file descriptor */
	ssize_t (*size)(void *); /**< Returns the total amount of incoming bytes */
	enum szl_res (*unblock)(void *); /**< Enables non-blocking I/O */
};

/**
 * @struct szl_stream
 * An I/O stream
 */
struct szl_stream {
	const struct szl_stream_ops *ops; /**< The underlying implementation */
	int keep; /**< A flag set for streams that should not be closed */
	int closed; /**< A flag set once the stream is closed */
	void *priv; /**< Private, implementation-specific data */
	void *buf; /**< A chunked I/O buffer */
	int blocking; /**< A flag set if the stream becomes non-blocking */
};

/**
 * @fn struct szl_obj *szl_new_stream(struct szl_interp *interp,
 *                                    struct szl_stream *strm,
 *                                    const char *type)
 * @brief Registers a new I/O stream
 * @param interp [in,out] An interpreter
 * @param strm [in,out] The new stream
 * @param type [in] The stream type
 * @return A new reference to the created object or NULL
 */
struct szl_obj *szl_new_stream(struct szl_interp *interp,
                               struct szl_stream *strm,
                               const char *type);

/**
 * @fn void szl_stream_free(struct szl_stream *strm)
 * @brief Frees all resources associated with an I/O stream
 * @param strm [in,out] The stream
 */
void szl_stream_free(struct szl_stream *strm);

/**
 * @fn enum szl_res szl_stream_proc(struct szl_interp *interp,
 *                                  const unsigned int objc,
 *                                  struct szl_obj **objv)
 * @brief The procedure implementation of streams
 * @param interp [in,out] An interpreter
 * @param objc [in]The number of arguments
 * @param objv [in,out] The procedure arguments
 * @return A member of @ref szl_res
 */
enum szl_res szl_stream_proc(struct szl_interp *interp,
                             const unsigned int objc,
                             struct szl_obj **objv);

/**
 * @fn void szl_stream_del(void *priv)
 * @brief The stream cleanup function of streams
 * @param priv [in,out] A @ref szl_stream structure
 */
void szl_stream_del(void *priv);

/**
 * @def SZL_STREAM_HELP
 * The help message of @ref szl_stream_proc
 */
#	define SZL_STREAM_HELP \
	"read|readln|write|writeln|flush|handle|close|unblock|accept ?len?"

/**
 * @def SZL_STREAM_INIT
 * Initializes a stream @ref szl_ext_export
 */
#	define SZL_STREAM_INIT(_name)     \
	.name = _name,                    \
	.type = SZL_TYPE_PROC,            \
	.val.proc.min_objc = 1,           \
	.val.proc.max_objc = 3,           \
	.val.proc.help = SZL_STREAM_HELP, \
	.val.proc.proc = szl_stream_proc, \
	.val.proc.del = szl_stream_del

/**
 * @}
 */

#endif
