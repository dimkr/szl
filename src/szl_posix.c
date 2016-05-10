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
#include <time.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <paths.h>
#include <stdio.h>

#include "szl.h"

#define SZL_EXEC_BUFSIZ BUFSIZ
#define SZL_EXIT_CODE_OBJ_NAME "?"

static enum szl_res szl_posix_proc_sleep(struct szl_interp *interp,
                                         const int objc,
                                         struct szl_obj **objv,
                                         struct szl_obj **ret)
{
	struct timespec req, rem;
	szl_double d;

	/* we assume sizeof(time_t) == 4 */
	if ((szl_obj_double(objv[1], &d) != SZL_OK) || (d < 0) || (d > INT_MAX))
		return SZL_ERR;

	req.tv_sec = (time_t)floor(d);
	req.tv_nsec = labs((long)(1000000000 * (d - (szl_double)req.tv_sec)));

	do {
		if (nanosleep(&req, &rem) == 0)
			break;

		if (errno != EINTR)
			return SZL_ERR;

		req.tv_sec = rem.tv_sec;
		req.tv_nsec = rem.tv_nsec;
	} while(1);

	return SZL_OK;
}

static enum szl_res szl_posix_proc_exec(struct szl_interp *interp,
                                        const int objc,
                                        struct szl_obj **objv,
                                        struct szl_obj **ret)
{
	char *buf, *mbuf;
	const char *cmd;
	struct szl_obj *exit_code;
	size_t len, mlen, rlen;
	pid_t pid;
	int fds[2], out, status;
	ssize_t clen;
	enum szl_res res = SZL_OK;

	cmd = szl_obj_str(objv[1], &len);
	if (!cmd || !len)
		return SZL_ERR;

	if (pipe(fds) < 0)
		return SZL_ERR;

	pid = fork();
	switch (pid) {
		case -1:
			close(fds[1]);
			close(fds[0]);
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
		if (buf)
			free(buf);
		return SZL_ERR;
	}

	if (res != SZL_OK) {
		if (buf)
			free(buf);
		return res;
	}

	if (!WIFEXITED(status)) {
		if (buf)
			free(buf);
		return SZL_ERR;
	}

	exit_code = szl_new_int(WEXITSTATUS(status));
	if (!exit_code) {
		if (buf)
			free(buf);
		return SZL_ERR;
	}

	res = szl_local(interp,
	                interp->caller,
	                SZL_EXIT_CODE_OBJ_NAME,
	                exit_code);
	szl_obj_unref(exit_code);
	if (res != SZL_OK) {
		free(buf);
		return SZL_ERR;
	}

	buf[rlen] = '\0';
	*ret = szl_new_str_noalloc(buf, rlen);
	if (*ret)
		return SZL_OK;

	return SZL_ERR;
}

static enum szl_res szl_posix_proc_getpid(struct szl_interp *interp,
                                          const int objc,
                                          struct szl_obj **objv,
                                          struct szl_obj **ret)
{
	szl_set_result_int(interp, ret, (szl_int)getpid());
	if (!*ret)
		return SZL_ERR;

	return SZL_OK;
}

enum szl_res szl_init_posix(struct szl_interp *interp)
{
	if ((!szl_new_proc(interp,
	                   "sleep",
	                   2,
	                   2,
	                   "sleep sec",
	                   szl_posix_proc_sleep,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "exec",
	                   2,
	                   2,
	                   "exec cmd",
	                   szl_posix_proc_exec,
	                   NULL,
	                   NULL)) ||
	    (!szl_new_proc(interp,
	                   "getpid",
	                   1,
	                   1,
	                   "getpid",
	                   szl_posix_proc_getpid,
	                   NULL,
	                   NULL)))
		return SZL_ERR;

	return SZL_OK;
}
