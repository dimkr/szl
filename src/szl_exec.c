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
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <paths.h>
#include <signal.h>

#include "szl.h"

struct szl_exec {
	int out;
	int in;
	pid_t pid;
	int reap;
};

static
ssize_t szl_exec_read(struct szl_interp *interp,
                      void *priv,
                      unsigned char *buf,
                      const size_t len,
                      int *more)
{
	struct szl_exec *exec = (struct szl_exec *)priv;
	ssize_t out;
	int pid;

	out = read(exec->out, buf, len);
	if (out < 0) {
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			return 0;

		szl_set_last_str(interp, strerror(errno), -1);
		return -1;
	} else if (out == 0) {
		if (exec->reap) {
			/* if the pipe has been closed, this could mean the process has
			 * terminated, so we want to try to reap it now */
			pid = waitpid(exec->pid, NULL, WNOHANG);
			if (pid == exec->pid)
				exec->reap = 0;
			else if ((pid < 0) && (errno != ECHILD)) {
				szl_set_last_str(interp, strerror(errno), -1);
				return -1;
			}
		}
		*more = 0;
	}

	return out;
}

static
ssize_t szl_exec_write(struct szl_interp *interp,
                       void *priv,
                       const unsigned char *buf,
                       size_t len)
{
	ssize_t out;

	out = write(((struct szl_exec *)priv)->in, buf, len);
	if (out < 0) {
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			return 0;

		szl_set_last_str(interp, strerror(errno), -1);
		return -1;
	}

	return (ssize_t)out;
}

static
enum szl_res szl_exec_unblock(void *priv)
{
	struct szl_exec *exec = (struct szl_exec *)priv;
	int fl;

	fl = fcntl(exec->in, F_GETFL);
	if ((fl < 0) || (fcntl(exec->in, F_SETFL, fl | O_NONBLOCK) < 0))
		return SZL_ERR;

	fl = fcntl(exec->out, F_GETFL);
	if ((fl < 0) || (fcntl(exec->out, F_SETFL, fl | O_NONBLOCK) < 0))
		return SZL_ERR;

	return SZL_OK;
}

static
void szl_exec_close(void *priv)
{
	struct szl_exec *exec = (struct szl_exec *)priv;
	close(exec->out);
	close(exec->in);
	if (exec->reap)
		waitpid(exec->pid, NULL, WNOHANG);
	free(exec);
}

static
szl_int szl_exec_handle(void *priv)
{
	return (szl_int)(((struct szl_exec *)priv)->pid);
}

static
const struct szl_stream_ops szl_exec_ops = {
	.read = szl_exec_read,
	.write = szl_exec_write,
	.close = szl_exec_close,
	.handle = szl_exec_handle,
	.unblock = szl_exec_unblock
};

static
enum szl_res szl_exec_proc_exec(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	int in[2], out[2];
	char *cmd;
	struct szl_obj *obj;
	struct szl_stream *strm;
	struct szl_exec *exec;
	size_t len;

	if (!szl_as_str(interp, objv[1], &cmd, &len) || !len)
		return SZL_ERR;

	exec = (struct szl_exec *)malloc(sizeof(*exec));
	if (!exec)
		return SZL_ERR;

	strm = (struct szl_stream *)malloc(sizeof(struct szl_stream));
	if (!strm) {
		free(exec);
		return SZL_ERR;
	}

	if (pipe(in) < 0) {
		free(strm);
		free(exec);
		return SZL_ERR;
	}

	if (pipe(out) < 0) {
		close(in[1]);
		close(in[0]);
		free(strm);
		free(exec);
		return SZL_ERR;
	}

	exec->pid = fork();
	if (exec->pid < 0) {
		close(out[1]);
		close(out[0]);
		close(in[1]);
		close(in[0]);
		free(strm);
		free(exec);
		return SZL_ERR;
	}
	else if (exec->pid == 0) {
		close(in[1]);
		close(out[0]);

		if (dup2(in[0], STDIN_FILENO) < 0) {
			close(out[1]);
			close(in[0]);
			exit(EXIT_FAILURE);
		}

		if (dup2(out[1], STDOUT_FILENO) < 0) {
			close(out[1]);
			close(in[0]);
			exit(EXIT_FAILURE);
		}

		close(out[1]);
		close(in[0]);
		execl(_PATH_BSHELL, _PATH_BSHELL, "-c", cmd, (char *)NULL);
		exit(EXIT_FAILURE);
	}

	close(out[1]);
	close(in[0]);
	exec->in = in[1];
	exec->out = out[0];
	exec->reap = 1;

	strm->ops = &szl_exec_ops;
	strm->keep = 0;
	strm->closed = 0;
	strm->priv = exec;
	strm->buf = NULL;
	strm->blocking = 1;

	obj = szl_new_stream(interp, strm, "exec");
	if (!obj) {
		szl_stream_free(strm);
		return SZL_ERR;
	}

	return szl_set_last(interp, obj);
}

static
const struct szl_ext_export exec_exports[] = {
	{
		SZL_PROC_INIT("exec", "cmd", 2, 2, szl_exec_proc_exec, NULL)
	}
};

int szl_init_exec(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                    "exec",
	                    exec_exports,
	                    sizeof(exec_exports) / sizeof(exec_exports[0]));
}
