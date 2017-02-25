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
#include <signal.h>
#include <sys/signalfd.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "szl.h"

struct szl_sig {
	sigset_t set;
	int fd;
};

static
ssize_t szl_signal_read(struct szl_interp *interp,
                        void *priv,
                        unsigned char *buf,
                        const size_t len,
                        int *more)
{
	struct signalfd_siginfo si;
	ssize_t out;
	int outc;

	out = read(((struct szl_sig *)priv)->fd, &si, sizeof(si));
	if (out == sizeof(si)) {
		outc = snprintf((char *)buf, len, "%"PRIu32, si.ssi_signo);
		if ((outc >= len) || (outc < 0))
			return -1;
		out = (ssize_t)outc;
		*more = 0;
	} else if (!out)
		*more = 0;
	else {
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			return 0;

		szl_set_last_strerror(interp, errno);
		return -1;
	}

	return out;
}

static
enum szl_res szl_signal_unblock(struct szl_interp *interp, void *priv)
{
	int fl, fd = ((struct szl_sig *)priv)->fd;

	fl = fcntl(fd, F_GETFL);
	if ((fl < 0) || (fcntl(fd, F_SETFL, fl | O_NONBLOCK) < 0))
		return szl_set_last_strerror(interp, errno);

	return SZL_OK;
}

static
void szl_signal_close(void *priv)
{
	struct szl_sig *sig = (struct szl_sig *)priv;

	sigprocmask(SIG_UNBLOCK, &sig->set, NULL);
	close(sig->fd);
	free(sig);
}

static
szl_int szl_signal_handle(void *priv)
{
	return (szl_int)(((struct szl_sig *)priv)->fd);
}

static
const struct szl_stream_ops szl_signal_ops = {
	.read = szl_signal_read,
	.unblock = szl_signal_unblock,
	.close = szl_signal_close,
	.handle = szl_signal_handle
};

static
enum szl_res szl_signal_proc_signal(struct szl_interp *interp,
                                    const unsigned int objc,
                                    struct szl_obj **objv)
{
	struct szl_sig *sig;
	struct szl_stream *strm;
	struct szl_obj *obj;
	szl_int signo;
	unsigned int i;

	sig = (struct szl_sig *)szl_malloc(interp,sizeof(*sig));
	if (!sig)
		return SZL_ERR;

	if (sigemptyset(&sig->set) < 0) {
		free(sig);
		return SZL_ERR;
	}

	for (i = 1; i < objc; ++i) {
		if (!szl_as_int(interp, objv[i], &signo) ||
		    (signo < INT_MIN) ||
		    (signo > INT_MAX) ||
		    (sigaddset(&sig->set, (int)signo) < 0)) {
			free(sig);
			return SZL_ERR;
		}
	}

	if (sigprocmask(SIG_BLOCK, &sig->set, NULL) < 0) {
		free(sig);
		return SZL_ERR;
	}

	sig->fd = signalfd(-1, &sig->set, 0);
	if (sig->fd < 0) {
		sigprocmask(SIG_UNBLOCK, &sig->set, NULL);
		free(sig);
		return SZL_ERR;
	}

	strm = (struct szl_stream *)szl_malloc(interp, sizeof(*strm));
	if (!strm) {
		close(sig->fd);
		sigprocmask(SIG_UNBLOCK, &sig->set, NULL);
		free(sig);
		return SZL_ERR;
	}

	strm->ops = &szl_signal_ops;
	strm->flags = SZL_STREAM_BLOCKING;
	strm->priv = (void *)sig;
	strm->buf = NULL;

	obj = szl_new_stream(interp, strm, "signal");
	if (!obj) {
		szl_stream_free(strm);
		return SZL_ERR;
	}

	return szl_set_last(interp, obj);
}

static
enum szl_res szl_signal_proc_kill(struct szl_interp *interp,
                                  const unsigned int objc,
                                  struct szl_obj **objv)
{
	szl_int pid, sig = SIGTERM;

	if (!szl_as_int(interp, objv[1], &pid) ||
	    (pid < LONG_MIN) ||
	    (pid > LONG_MAX) ||
	    ((objc == 3) &&
	     (!szl_as_int(interp, objv[2], &sig) ||
	      (sig < INT_MIN) ||
	      (sig > INT_MAX))))
		return SZL_ERR;

	if (kill((pid_t)(long)pid, (int)sig) < 0)
		return szl_set_last_strerror(interp, errno);

	return SZL_OK;
}

static
enum szl_res szl_signal_proc_wait(struct szl_interp *interp,
                                  const unsigned int objc,
                                  struct szl_obj **objv)
{
	int status;

	if (wait(&status) < 0)
		return SZL_ERR;

	if (WIFEXITED(status))
		return szl_set_last_int(interp, (szl_int)WEXITSTATUS(status));

	return SZL_OK;
}

static
const struct szl_ext_export signal_exports[] = {
	{
		SZL_PROC_INIT("signal",
		              "sig...",
		              2,
		              -1,
		              szl_signal_proc_signal,
		              NULL)
	},
	{
		SZL_INT_INIT("sigint", SIGINT)
	},
	{
		SZL_INT_INIT("sigterm", SIGTERM)
	},
	{
		SZL_INT_INIT("sigchld", SIGCHLD)
	},
	{
		SZL_INT_INIT("sigusr1", SIGUSR1)
	},
	{
		SZL_INT_INIT("sigusr2", SIGUSR2)
	},
	{
		SZL_PROC_INIT("kill",
		              "kill pid ?sig?",
		              2,
		              3,
		              szl_signal_proc_kill,
		              NULL)
	},
	{
		SZL_PROC_INIT("wait",
		              "wait",
		              1,
		              1,
		              szl_signal_proc_wait,
		              NULL)
	},
};

int szl_init_signal(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "signal",
	                   signal_exports,
	                   sizeof(signal_exports) / sizeof(signal_exports[0]));
}
