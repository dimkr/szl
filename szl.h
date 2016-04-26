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

#	include <stdint.h>
#	include <sys/types.h>

#	include <zlib.h>

#	include "szl_conf.h"

/**
 * @mainpage Welcome
 * This is documentation of the external szl interpreter API, for embedding of
 * the interpreter and development of extensions.
 *
 * @defgroup high_level High-level API
 * @defgroup low_level Low-level API
 * @defgroup internals Internal implementation details
 *
 * @defgroup limits Artificial Limits
 * @ingroup internals
 * @{
 */

/**
 * @def SZL_MAX_PROC_OBJC
 * The maximum number of procedure arguments
 */
#	define SZL_MAX_PROC_OBJC 12

/**
 * @def SZL_MAX_OBJC_DIGITS
 * The maximum number of digits of the maximum number of procedure arguments
 */
#	define SZL_MAX_OBJC_DIGITS sizeof("12") - 1

/**
 * @}
 *
 * @defgroup syntax Syntax
 * @ingroup internals
 * @{
 */

/**
 * @def SZL_COMMENT_PREFIX
 * The first character of comments
 */
#	define SZL_COMMENT_PREFIX '#'

/**
 * @def SZL_EXC_OBJ_NAME
 * The name of the special object containing the exception object in an except
 * block
 */
#	define SZL_EXC_OBJ_NAME "ex"

/**
 * @def SZL_OBJC_OBJECT_NAME
 * The name of the special object containing the argument count of a procedure
 */
#	define SZL_OBJC_OBJECT_NAME "#"

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
 * @typedef szl_double
 * The type used for representing szl objects as floating-point numbers
 */
typedef double szl_double;

/**
 * @typedef szl_bool
 * The type used for representing szl objects as boolean values
 */
typedef unsigned char szl_bool;

/**
 * @def SZL_INT_FMT
 * The format string for printing @ref szl_int
 */
#	define SZL_INT_FMT "%jd"

/**
 * @def SZL_DOUBLE_FMT
 * The format string for printing @ref szl_double
 */
#	define SZL_DOUBLE_FMT "%.12f"

/**
 * @def SZL_DOUBLE_SCANF_FMT
 * The format string for converting strings to @ref szl_double
 */
#	define SZL_DOUBLE_SCANF_FMT "%lf"

/**
 * @typedef szl_hash
 * The type of szl object name hashes
 */
typedef uLong szl_hash;

/**
 * @enum szl_res
 * Procedure status codes, which control the flow of execution
 */
enum szl_res {
	SZL_OK, /**< Success */
	SZL_ERR, /**< Error: the next statement shall not be executed */
	SZL_BREAK, /**< Success, but the next statement shall not be executed */
	SZL_CONT /**< Jump back to the first statement in the loop body */
};

/**
 * @enum szl_type
 * Object data types
 */
enum szl_type {
	SZL_TYPE_STR    = 1 << 1, /**< String */
	SZL_TYPE_INT    = 1 << 2, /**< Integer */
	SZL_TYPE_DOUBLE = 1 << 3, /**< Floating-point number */
	SZL_TYPE_BOOL   = 1 << 4 /**< Boolean value */
};

struct szl_obj;
struct szl_interp;

/**
 * @typedef szl_proc
 * A C function implementing a szl procedure
 */
typedef enum szl_res (*szl_proc)(struct szl_interp *,
                                 const int,
                                 struct szl_obj **,
                                 struct szl_obj **);
/**
 * @typedef szl_delproc
 * A cleanup function called when a procedure object is freed
 */
typedef void (*szl_delproc)(void *);

/**
 * @struct szl_obj
 * A szl object
 */
struct szl_obj {
	unsigned int refc; /**< The object reference count; the object is freed when it drops to zero */
	enum szl_type type; /**< Available representations of the object */
	char *s; /**< SZL_TYPE_STR representation */
	size_t len; /**< The length of s */
	szl_bool b; /**< SZL_TYPE_BOOL representation */
	szl_int i; /**< SZL_TYPE_INT representation */
	szl_double d; /**< SZL_TYPE_DOUBLE representation */

	szl_proc proc; /**< C procedure implementation */
	void *priv; /**< Private data used by proc */
	szl_delproc del; /**< Cleanup callback which frees @ref priv */
	int min_argc; /**< The minimum number of arguments to @ref proc or -1 */
	int max_argc; /**< The maximum number of arguments to @ref proc or -1 */
	const char *help; /**< A usage message shown if the number of arguments is below @ref min_argc or above @ref max_argc */
	struct szl_local **locals; /**< Local procedure variables */
	size_t nlocals; /**< The number of elements in @ref locals */
};

/**
 * @struct szl_ext
 * A szl extension
 */
struct szl_ext {
	struct szl_obj **objs; /**< Objects defined by the extension */
	unsigned int nobjs; /**< The number of elements in objs */
	void *handle; /**< The extension shared object handle */
};

/**
 * @struct szl_interp
 * A szl interpreter
 */
struct szl_interp {
	struct szl_obj *empty; /**< An empty string singleton */
	struct szl_obj *zero; /**< A 0 integer singleton */
	struct szl_obj *one; /**< A 1 integer singleton */

	uLong init; /**< The initial CRC32 checksum */
	unsigned int seed; /**< A PRNG seed */

	struct szl_obj *global; /**< The frame of statements executed in the global scope, outside of a procedure */

	struct szl_obj *current; /**< The currently running procedure */
	struct szl_obj *caller; /**< The procedure that called current */

	struct szl_ext *exts; /**< Loaded extensions */
	unsigned int nexts; /**< The number of elements in exts */
};

/**
 * @struct szl_local
 * A local variable
 */
struct szl_local {
	szl_hash name; /**< The CRC32 hash of the variable name */
	struct szl_obj *obj; /**< The variable value */
};

/**
 * @typedef szl_ext_init
 * The prototype of an extension initialization function
 */
typedef enum szl_res (*szl_ext_init)(struct szl_interp *);


/**
 * @def SZL_EXT_INIT
 * A "decorator" for external extension initialization functions
 */
#	define SZL_EXT_INIT __attribute__((__visibility__("default")))

/**
 * @}
 *
 * @defgroup interp Interpreters
 * @ingroup high_level
 * @{
 */

/**
 * @fn struct szl_interp *szl_interp_new(void)
 * @brief Creates a new interpreter
 */
struct szl_interp *szl_interp_new(void);

/**
 * @fn void szl_interp_free(struct szl_interp *interp)
 * @brief Frees all memory associated with an interpreter
 * @param interp [in] An interpreter
 */
void szl_interp_free(struct szl_interp *interp);

/**
 * @}
 *
 * @defgroup refc Reference counting
 * @ingroup internals
 * @{
 */

/**
 * @fn struct szl_obj *szl_obj_ref(struct szl_obj *obj)
 * @brief Increments the reference count of an object
 * @param obj [in,out] An object
 * @return The passed object pointer
 */
struct szl_obj *szl_obj_ref(struct szl_obj *obj);

/**
 * @fn void szl_obj_unref(struct szl_obj *obj)
 * @brief Decrements the reference count of an object and frees it when the
 *        reference count reaches zero
 * @param obj [in,out] An object
 */
void szl_obj_unref(struct szl_obj *obj);

/**
 * @}
 *
 * @defgroup obj Object creation
 * @ingroup low_level
 * @{
 */

/**
 * @fn void szl_new_obj_name(struct szl_interp *interp,
 *                           const char *pfix,
 *                           char *buf,
 *                           const size_t len)
 * @brief Creates a random name for a newly created variable
 * @param interp [in,out] An interpreter
 * @param pfix [in] A prefix for the variable name
 * @param buf [out] The output buffer
 * @param len [in] The buffer size
 *
 * The returned name is in the format "prefix.number".
 */
void szl_new_obj_name(struct szl_interp *interp,
                      const char *pfix,
                      char *buf,
                      const size_t len);

/**
 * @fn struct szl_obj *szl_new_str_noalloc(char *s, const size_t len)
 * @brief Creates a new string object
 * @param s [in] The string
 * @param len [in,out] The string length
 * @return A new reference to the created string object or NULL
 * @note s is used to represent the object value; it is freed automatically
 */
struct szl_obj *szl_new_str_noalloc(char *s, size_t len);

/**
 * @fn struct szl_obj *szl_new_str(const char *s, int len)
 * @brief Creates a new string object, by copying an existing C string
 * @param s [in] The string
 * @param len [in] The string length or -1 if unknown
 * @return A new reference to the created string object or NULL
 */
struct szl_obj *szl_new_str(const char *s, int len);

/**
 * @fn struct szl_obj *szl_new_int(const szl_int i)
 * @brief Creates a new integer object
 * @param i [in] The integer value
 * @return A new reference to the created integer object or NULL
 */
struct szl_obj *szl_new_int(const szl_int i);

/**
 * @fn struct szl_obj *szl_new_double(const szl_double d)
 * @brief Creates a new floating point number object
 * @param d [in] The floating point number value
 * @return A new reference to the created floating point number object or NULL
 */
struct szl_obj *szl_new_double(const szl_double d);

/**
 * @fn struct szl_obj *szl_new_proc(struct szl_interp *interp,
 *                                  const char *name,
 *                                  const int min_argc,
 *                                  const int max_argc,
 *                                  const char *help,
 *                                  const szl_proc proc,
 *                                  const szl_delproc del,
 *                                  void *priv)
 * @brief Creates a new procedure object and registers it in the global scope
 * @param interp [in,out] An interpreter
 * @param name [in] The procedure name
 * @param min_argc [in] The minimum number of arguments or -1
 * @param max_argc [in] The maximum number of arguments or -1
 * @param help [in] A help message which explains how to use the procedure
 * @param proc [in] A C callback implementing the procedure or NULL
 * @param del [in] A cleanup callback for the procedure private data or NULL
 * @param priv [in] Private data for use by proc and del, or NULL
 * @return A new reference to the created procedure object or NULL
 * @note If name is an empty string, the procedure is not registered in the
 *       global scope and its reference count is zero
 */
struct szl_obj *szl_new_proc(struct szl_interp *interp,
                             const char *name,
                             const int min_argc,
                             const int max_argc,
                             const char *help,
                             const szl_proc proc,
                             const szl_delproc del,
                             void *priv);

/**
 * @def szl_empty
 * Returns a new reference to the empty string singleton
 */
#	define szl_empty(interp) szl_obj_ref(interp->empty)

/**
 * @def szl_zero
 * Returns a new reference to the 0 integer singleton
 */
#	define szl_zero(interp) szl_obj_ref(interp->zero)

/**
 * @def szl_one
 * Returns a new reference to the 1 integer singleton
 */
#	define szl_one(interp) szl_obj_ref(interp->one)

/**
 * @def szl_false
 * @see szl_zero
 */
#	define szl_false szl_zero

/**
 * @def szl_true
 * @see szl_one
 */
#	define szl_true szl_one

/**
 * @}
 *
 * @defgroup obj_ops Object operations
 * @ingroup low_level
 * @{
 */

/**
 * @fn enum szl_res szl_append(struct szl_obj *obj,
 *                             const char *s,
 *                             const size_t len)
 * @brief Appends a string to an existing object
 * @param obj [in,out] The object
 * @param s [in] The string
 * @param len [in] The string length
 * @return SZL_OK or SZL_ERR
 */
enum szl_res szl_append(struct szl_obj *obj,
                        const char *s,
                        const size_t len);

/**
 * @fn char *szl_obj_str(struct szl_obj *obj, size_t *len)
 * @brief Returns the C string representation of an object
 * @param obj [in,out] The object
 * @param len [out] The string length or NULL if not needed
 * @return A C string or NULL
 */
char *szl_obj_str(struct szl_obj *obj, size_t *len);

/**
 * @fn char *szl_obj_strdup(struct szl_obj *obj, size_t *len)
 * @brief Copies the C string representation of an object
 * @param obj [in,out] The object
 * @param len [out] The string length or NULL if not needed
 * @return A newly allocated C string or NULL
 * @note The return value must be freed using free()
 */
char *szl_obj_strdup(struct szl_obj *obj, size_t *len);

/**
 * @fn enum szl_res szl_obj_len(struct szl_obj *obj, size_t *len)
 * @brief Returns the length of the string representation of an object
 * @param obj [in,out] The object
 * @param len [out] The string length
 * @return SZL_OK or SZL_ERR
 */
enum szl_res szl_obj_len(struct szl_obj *obj, size_t *len);

/**
 * @fn enum szl_res szl_obj_int(struct szl_obj *obj, szl_int *i)
 * @brief Returns the C integer representation of an object
 * @param obj [in,out] The object
 * @param i [out] The integer representation
 * @return SZL_OK or SZL_ERR
 */
enum szl_res szl_obj_int(struct szl_obj *obj, szl_int *i);

/**
 * @fn enum szl_res szl_obj_double(struct szl_obj *obj, szl_double *d)
 * @brief Returns the C floating point number representation of an object
 * @param obj [in,out] The object
 * @param d [out] The floating point representation
 * @return SZL_OK or SZL_ERR
 */
enum szl_res szl_obj_double(struct szl_obj *obj, szl_double *d);

/**
 * @fn szl_bool szl_obj_istrue(struct szl_obj *obj)
 * @brief Returns the truth value of an object
 * @param obj [in,out] The object
 * @return 1 or 0
 */
szl_bool szl_obj_istrue(struct szl_obj *obj);

/**
 * @def szl_obj_isfalse
 * The opposite of @ref szl_obj_istrue
 */
#	define szl_obj_isfalse !szl_obj_istrue

/**
 * @}
 *
 * @defgroup retval Procedure return values
 * @ingroup low_level
 * @{
 */

/**
 * @fn void szl_set_result(struct szl_obj **out, struct szl_obj *obj)
 * @brief Replaces the existing return value object of a procedure
 * @param out [in,out] The procedure return value object
 * @param obj [in,out] The new return value object
 */
void szl_set_result(struct szl_obj **out, struct szl_obj *obj);

/**
 * @fn void szl_empty_result(struct szl_interp *interp, struct szl_obj **out)
 * @brief Replaces the existing return value object of a procedure with an empty
 *        string
 * @param interp [in,out] An interpreter
 * @param out [in,out] The procedure return value object
 */
void szl_empty_result(struct szl_interp *interp, struct szl_obj **out);

/**
 * @fn void szl_set_result_str(struct szl_interp *interp,
 *                             struct szl_obj **out,
 *                             const char *s)
 * @brief Replaces the existing return value object of a procedure with a newly
 *        created string object
 * @param interp [in,out] An interpreter
 * @param out [in,out] The procedure return value object
 * @param s [in] The string
 */
void szl_set_result_str(struct szl_interp *interp,
                        struct szl_obj **out,
                        const char *s);

/**
 * @fn void szl_set_result_fmt(struct szl_interp *interp,
 *                             struct szl_obj **out,
 *                             const char *fmt,
 *                             ...)
 * @brief Replaces the existing return value object of a procedure with a newly
 *        created, printf()-style formatted string object
 * @param interp [in,out] An interpreter
 * @param out [in,out] The procedure return value object
 * @param fmt [in] The format string
 */
void szl_set_result_fmt(struct szl_interp *interp,
                        struct szl_obj **out,
                        const char *fmt,
                        ...);

/**
 * @fn void szl_set_result_int(struct szl_interp *interp,
 *                             struct szl_obj **out,
 *                             const szl_int i)
 * @brief Replaces the existing return value object of a procedure with a newly
 *        created integer object
 * @param interp [in,out] An interpreter
 * @param out [in,out] The procedure return value object
 * @param i [in] The integer
 */
void szl_set_result_int(struct szl_interp *interp,
                        struct szl_obj **out,
                        const szl_int i);

/**
 * @fn void szl_set_result_double(struct szl_interp *interp,
 *                                struct szl_obj **out,
 *                                const szl_double d)
 * @brief Replaces the existing return value object of a procedure with a newly
 *        created floating point number object
 * @param interp [in,out] An interpreter
 * @param out [in,out] The procedure return value object
 * @param d [in] The floating point number
 */
void szl_set_result_double(struct szl_interp *interp,
                           struct szl_obj **out,
                           const szl_double d);

/**
 * @fn void szl_set_result_bool(struct szl_interp *interp,
 *                              struct szl_obj **out,
 *                              const szl_bool b)
 * @brief Replaces the existing return value object of a procedure with a
 *        boolean object
 * @param interp [in,out] An interpreter
 * @param out [in,out] The procedure return value object
 * @param b [in] The boolean value
 */
void szl_set_result_bool(struct szl_interp *interp,
                         struct szl_obj **out,
                         const szl_bool b);

/**
 * @fn void szl_usage(struct szl_interp *interp,
 *                    struct szl_obj **out,
 *                    struct szl_obj *proc)
 * @brief Replaces the existing return value object of a procedure with a help
 *        message
 * @param interp [in,out] An interpreter
 * @param out [in,out] The procedure return value object
 * @param proc [in] The calling procedure
 */
void szl_usage(struct szl_interp *interp,
               struct szl_obj **out,
               struct szl_obj *proc);

/**
 * @}
 *
 * @defgroup vars Variables
 * @ingroup low_level
 * @{
 */

/**
 * @fn enum szl_res szl_get(struct szl_interp *interp,
 *                          struct szl_obj **out,
 *                          const char *name)
 * @brief Searches for an object by name and returns a new reference to it
 * @param interp [in,out] An interpreter
 * @param out [out] The object
 * @param name [in] The object name
 * @return SZL_OK or SZL_ERR
 */
enum szl_res szl_get(struct szl_interp *interp,
                     struct szl_obj **out,
                     const char *name);

/**
 * @fn enum szl_set_in_proc(struct szl_interp *interp,
 *                          const char *name,
 *                          struct szl_obj *obj,
 *                          struct szl_obj *proc)
 * @brief Registers an existing object with a given name, in a given scope
 * @param interp [in,out] An interpreter
 * @param name [in] The object name
 * @param obj [in,out] The object
 * @param proc [in,out] The scope
 * @return SZL_OK or SZL_ERR
 */
enum szl_res szl_set_in_proc(struct szl_interp *interp,
                             const char *name,
                             struct szl_obj *obj,
                             struct szl_obj *proc);

/**
 * @fn enum szl_res szl_set(struct szl_interp *interp,
 *                          const char *name,
 *                          struct szl_obj *obj)
 * @brief Registers an existing object with a given name, in the global scope
 * @param interp [in,out] An interpreter
 * @param name [in] The object name
 * @param obj [in,out] The object
 * @return SZL_OK or SZL_ERR
 */
enum szl_res szl_set(struct szl_interp *interp,
                     const char *name,
                     struct szl_obj *obj);

/**
 * @fn enum szl_res szl_local(struct szl_interp *interp,
 *                            struct szl_obj *proc,
 *                            const char *name,
 *                            struct szl_obj *obj)
 * @brief Registers an existing object with a given name, in the scope of a
 *        procedure
 * @param interp [in,out] An interpreter
 * @param proc [in] The procedure
 * @param name [in] The object name
 * @param obj [in,out] The object
 * @return SZL_OK or SZL_ERR
 */
enum szl_res szl_local(struct szl_interp *interp,
                       struct szl_obj *proc,
                       const char *name,
                       struct szl_obj *obj);

/**
 * @}
 *
 * @defgroup exec Statement execution
 * @ingroup high_level
 * @{
 */

/**
 * @fn enum szl_res szl_eval(struct szl_interp *interp,
 *                           struct szl_obj **out,
 *                           const char *s)
 * @brief Evaluates a single expression
 * @param interp [in,out] An interpreter
 * @param out [in,out] The evaluation result
 * @param s [in] The expression
 * @return A member of @ref szl_res
 *
 * This function expands variable values and procedure calls in an expression
 * and returns a new object representing the evaluated expression.
 */
enum szl_res szl_eval(struct szl_interp *interp,
                      struct szl_obj **out,
                      const char *s);

/**
 * @fn enum enum szl_res szl_run(struct szl_interp *interp,
 *                               struct szl_obj **out,
 *                               char *s,
 *                               const size_t len)
 * @brief Runs a szl snippet
 * @param interp [in,out] An interpreter
 * @param out [in,out] The snippet return value
 * @param s [in,out] The snippet
 * @param len [in] The snippet length
 * @return A member of @ref szl_res
 *
 * This function executes multiple statements and returns both the return value
 * and status code of the last statement executed.
 */
enum szl_res szl_run(struct szl_interp *interp,
                     struct szl_obj **out,
                     char *s,
                     const size_t len);

/**
 * @fn enum enum szl_res szl_run_const(struct szl_interp *interp,
 *                                     struct szl_obj **out,
 *                                     const char *s,
 *                                     const size_t len)
 * @brief Copies a szl snippet and runs it
 * @param interp [in,out] An interpreter
 * @param out [in,out] The snippet return value
 * @param s [in] The snippet
 * @param len [in] The snippet length
 * @return A member of @ref szl_res
 *
 * This function a wrapper to @ref szl_run, which does not modify the passed
 * string.
 */
enum szl_res szl_run_const(struct szl_interp *interp,
                           struct szl_obj **out,
                           const char *s,
                           const size_t len);

/**
 * @}
 *
 * @defgroup ext External
 * @ingroup high_level
 * @{
 */

/**
 * @def SZL_EXT_INIT_FUNC_NAME_FMT
 * The format of extension initialization function names
 */
#	define SZL_EXT_INIT_FUNC_NAME_FMT "szl_init_%s"

/**
 * @def SZL_MAX_EXT_INIT_FUNC_NAME_LEN
 * The maximum length of an extension initialization function name
 */
#	define SZL_MAX_EXT_INIT_FUNC_NAME_LEN 32

/**
 * @def SZL_EXT_PATH_FMT
 * The format of extension paths
 */
#	define SZL_EXT_PATH_FMT SZL_EXT_DIR"/szl_%s.so"

/**
 * @fn enum szl_res szl_load(struct szl_interp *interp, const char *name)
 * @brief Loads an extension
 * @param interp [in,out] An interpreter
 * @param name [in] The extension name
 * @return SZL_OK or SZL_ERR
 */
enum szl_res szl_load(struct szl_interp *interp, const char *name);

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
 * @defgroup front Frontend
 * @ingroup high_level
 * @{
 */

/**
 * @fn enum szl_res szl_main(int argc, char *argv[])
 * @brief Runs the main szlsh logic
 * @param argc [in,out] The number of elements in argv
 * @param argv [in,out] The interpreter arguments
 * @return A member of @ref szl_res
 */
enum szl_res szl_main(int argc, char *argv[]);

/**
 * @}
 */

#endif
