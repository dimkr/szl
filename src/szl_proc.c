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
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <paths.h>

#include "szl.h"

#define SZL_EXEC_BUFSIZ BUFSIZ

static
enum szl_res szl_proc_proc_exec(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	struct szl_obj *list, *str;
	char *buf, *mbuf;
	const char *cmd;
	struct szl_obj *exit_code;
	size_t len, mlen, rlen;
	pid_t pid;
	int fds[2], out, status;
	ssize_t clen;
	enum szl_res res = SZL_OK;

	cmd = szl_obj_str(interp, objv[1], &len);
	if (!cmd || !len)
		return SZL_ERR;

	list = szl_new_empty();
	if (!list)
		return SZL_ERR;

	if (pipe(fds) < 0) {
		szl_obj_unref(list);
		return SZL_ERR;
	}

	pid = fork();
	switch (pid) {
		case -1:
			close(fds[1]);
			close(fds[0]);
			szl_obj_unref(list);
			return SZL_ERR;

		case 0:
			close(fds[0]);
			out = dup2(fds[1], STDOUT_FILENO);
			close(fds[1]);
			if (out == STDOUT_FILENO)
				execl(_PATH_BSHELL, _PATH_BSHELL, "-c", cmd, (char *)NULL);
			exit(EXIT_FAILURE);
	}

	close(fds[1]);

	buf = (char *)malloc(SZL_EXEC_BUFSIZ + 1);
	if (!buf) {
		close(fds[0]);
		szl_obj_unref(list);
		return SZL_ERR;
	}

	len = mlen = SZL_EXEC_BUFSIZ;
	rlen = 0;
	do {
		clen = read(fds[0], buf + rlen, SZL_EXEC_BUFSIZ);
		if (clen < 0) {
			res = SZL_ERR;
			break;
		}
		if (clen == 0)
			break;

		mlen = len + SZL_EXEC_BUFSIZ;
		mbuf = (char *)realloc(buf, mlen);
		if (!mbuf) {
			free(buf);
			buf = NULL;
			res = SZL_ERR;
			break;
		}
		buf = mbuf;
		len = mlen;

		rlen += clen;
	} while (rlen <= SIZE_MAX);

	close(fds[0]);

	if (waitpid(pid, &status, 0) != pid) {
		free(buf);
		szl_obj_unref(list);
		return SZL_ERR;
	}

	if (res != SZL_OK) {
		free(buf);
		szl_obj_unref(list);
		return res;
	}

	if (!WIFEXITED(status)) {
		free(buf);
		szl_obj_unref(list);
		return SZL_ERR;
	}

	exit_code = szl_new_int(WEXITSTATUS(status));
	if (!exit_code) {
		free(buf);
		szl_obj_unref(list);
		return SZL_ERR;
	}

	if (!szl_lappend(interp, list, exit_code)) {
		szl_obj_unref(exit_code);
		free(buf);
		szl_obj_unref(list);
		return SZL_ERR;
	}

	szl_obj_unref(exit_code);

	buf[rlen] = '\0';
	str = szl_new_str_noalloc(buf, rlen);
	if (!str) {
		free(buf);
		szl_obj_unref(list);
		return SZL_ERR;
	}

	if (!szl_lappend(interp, list, str)) {
		szl_obj_unref(str);
		free(buf);
		szl_obj_unref(list);
		return SZL_ERR;
	}

	szl_obj_unref(str);
	return szl_set_result(interp, list);
}

static
enum szl_res szl_proc_proc_getpid(struct szl_interp *interp,
                                  const int objc,
                                  struct szl_obj **objv)
{
	return szl_set_result_int(interp, (szl_int)getpid());
}

static
enum szl_res szl_proc_eval_proc(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	struct szl_obj *argv_obj, *arg_obj;
	const char *s, *exp;
	size_t len, elen;
	int i, resi;
	enum szl_res res;

	s = szl_obj_str(interp, (struct szl_obj *)objv[0]->priv, &len);
	if (!s)
		return SZL_ERR;

	/* get the currently running expression */
	exp = szl_obj_str(interp, interp->current, &elen);
	if (!exp || !elen)
		return SZL_ERR;

	/* create the $@ object */
	argv_obj = szl_new_empty();
	if (!argv_obj)
		return SZL_ERR;

	for (i = 1; i < objc; ++i) {
		if (!szl_lappend(interp, argv_obj, objv[i])) {
			szl_obj_unref(argv_obj);
			return SZL_ERR;
		}
	}

	resi = szl_local(interp, interp->current, interp->objv_name, argv_obj);
	szl_obj_unref(argv_obj);
	if (!resi)
		return SZL_ERR;

	/* create the argument objects ($0, $1, ...) */
	for (i = 0; i < objc; ++i) {
		arg_obj = szl_new_int((szl_int)i);
		if (!arg_obj)
			return SZL_ERR;

		if (!szl_local(interp, interp->current, arg_obj, objv[i]))
			return SZL_ERR;
	}

	res = szl_run(interp, s, len);

	if (res == SZL_RET)
		return SZL_OK;

	return res;
}

static
void szl_proc_del(void *priv)
{
	szl_obj_unref((struct szl_obj *)priv);
}

static
enum szl_res szl_proc_proc_proc(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	const char *name;

	name = szl_obj_str(interp, objv[1], NULL);
	if (!name)
		return SZL_ERR;

	if (szl_new_proc(interp,
	                 name,
	                 -1,
	                 -1,
	                 NULL,
	                 szl_proc_eval_proc,
	                 szl_proc_del,
	                 objv[2])) {
		szl_obj_ref(objv[2]);
		return SZL_OK;
	}

	return SZL_ERR;
}

static
enum szl_res szl_proc_proc_return(struct szl_interp *interp,
                                  const int objc,
                                  struct szl_obj **objv)
{
	if (objc == 2)
		szl_set_result(interp, szl_obj_ref(objv[1]));

	return SZL_RET;
}

static
enum szl_res szl_proc_proc_stack(struct szl_interp *interp,
                                 const int objc,
                                 struct szl_obj **objv)
{
	struct szl_obj *stack, *call;
	szl_int lim = SZL_INT_MAX, i;

	if (objc == 2) {
		if (!szl_obj_int(interp, objv[1], &lim))
			return SZL_ERR;

		if (lim <= 0) {
			szl_set_result_fmt(interp, "bad limit: "SZL_INT_FMT, lim);
			return SZL_ERR;
		}
	}

	stack = szl_new_empty();
	if (!stack)
		return SZL_ERR;

	call = interp->current;
	for (i = 0; call && (i < lim); ++i) {
		if (!szl_lappend(interp, stack, call)) {
			szl_obj_unref(stack);
			return SZL_ERR;
		}

		call = call->caller;
	}

	return szl_set_result(interp, stack);
}

int szl_init_proc(struct szl_interp *interp)
{
	return ((szl_new_proc(interp,
	                      "exec",
	                      2,
	                      2,
	                      "exec cmd",
	                      szl_proc_proc_exec,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "getpid",
	                      1,
	                      1,
	                      "getpid",
	                      szl_proc_proc_getpid,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "proc",
	                      3,
	                      3,
	                      "proc name exp",
	                      szl_proc_proc_proc,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "return",
	                      1,
	                      2,
	                      "return ?obj?",
	                      szl_proc_proc_return,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "stack",
	                      1,
	                      2,
	                      "stack ?lim?",
	                      szl_proc_proc_stack,
	                      NULL,
	                      NULL)));
}
