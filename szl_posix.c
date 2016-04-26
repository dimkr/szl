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

#include "szl.h"

#define EXEC_BUFSIZ 4096

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

	buf = (char *)malloc(EXEC_BUFSIZ + 1);
	if (!buf) {
		close(fds[0]);
		return SZL_ERR;
	}

	len = mlen = EXEC_BUFSIZ;
	rlen = 0;
	do {
		clen = read(fds[0], buf + rlen, EXEC_BUFSIZ);
		if (clen < 0) {
			res = SZL_ERR;
			break;
		}
		if (clen == 0)
			break;

		mlen = len + EXEC_BUFSIZ;
		mbuf = (char *)realloc(buf, mlen);
		if (!mbuf) {
			free(buf);
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
		return SZL_ERR;
	}

	if (res == SZL_OK) {
		buf[rlen] = '\0';
		*ret = szl_new_str_noalloc(buf, rlen);
		if (*ret)
			return SZL_OK;
	}

	free(buf);
	return res;
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
	                   NULL)))
		return SZL_ERR;

	return SZL_OK;
}
