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
#include <signal.h>
#include <string.h>

#include "szl.h"

struct szl_sigmask {
	sigset_t set;
	sigset_t oldset;
};

static const struct {
	const char *name;
	int sig;
} sigs[] = {
	{"term", SIGTERM},
	{"int", SIGINT},
	{"usr1", SIGUSR1},
	{"usr2", SIGUSR2}
};

static
enum szl_res szl_signal_sigmask_proc(struct szl_interp *interp,
                                     const int objc,
                                     struct szl_obj **objv)
{
	struct szl_sigmask *mask = (struct szl_sigmask *)objv[0]->priv;
	const char *s;
	size_t len;
	int i, out;

	s = szl_obj_str(objv[1], &len);
	if (!s || !len)
		return SZL_ERR;

	if (strcmp("check", s) != 0)
		return SZL_ERR;

	if (sigpending(&mask->set) < 0)
		return SZL_ERR;

	for (i = 0; i < sizeof(sigs) / sizeof(sigs[0]); ++i) {
		out = sigismember(&mask->set, sigs[i].sig);
		if (out < 0)
			return SZL_ERR;

		if (out) {
			szl_set_result_fmt(interp, "received a signal: %s", sigs[i].name);
			return SZL_ERR;
		}
	}

	return SZL_OK;
}

static
void szl_signal_sigmask_del(void *priv)
{
	sigprocmask(SIG_UNBLOCK, &((struct szl_sigmask *)priv)->oldset, NULL);
	free(priv);
}

static
enum szl_res szl_signal_proc_block(struct szl_interp *interp,
                                   const int objc,
                                   struct szl_obj **objv)
{
	char name[sizeof("signal.mask:"SZL_PASTE(SZL_INT_MIN))];
	struct szl_obj *proc;
	const char *s;
	struct szl_sigmask *mask;
	size_t len;
	int i, j;

	mask = (struct szl_sigmask *)malloc(sizeof(*mask));
	if (!mask)
		return SZL_ERR;

	if (sigemptyset(&mask->set) < 0) {
		free(mask);
		return SZL_ERR;
	}

	for (i = 1; i < objc; ++i) {
		s = szl_obj_str(objv[i], &len);
		if (!s || !len) {
			free(mask);
			return SZL_ERR;
		}

		for (j = 0; j < sizeof(sigs) / sizeof(sigs[0]); ++j) {
			if (strcmp(sigs[j].name, s) == 0) {
				if (sigaddset(&mask->set, sigs[j].sig) < 0) {
					free(mask);
					return SZL_ERR;
				}
				break;
			}
		}

	}

	if (sigprocmask(SIG_BLOCK, &mask->set, &mask->oldset) < 0) {
		free(mask);
		return SZL_ERR;
	}

	szl_new_obj_name(interp, "signal.mask", name, sizeof(name), mask);
	proc = szl_new_proc(interp,
	                    name,
	                    2,
	                    2,
	                    "mask check",
	                    szl_signal_sigmask_proc,
	                    szl_signal_sigmask_del,
	                    mask);
	if (!proc) {
		sigprocmask(SIG_UNBLOCK, &mask->oldset, NULL);
		free(mask);
		return SZL_ERR;
	}

	return szl_set_result(interp, szl_obj_ref(proc));
}

int szl_init_signal(struct szl_interp *interp)
{
	return szl_new_proc(interp,
	                    "signal.block",
	                    2,
	                    -1,
	                    "signal.block sig...",
	                    szl_signal_proc_block,
	                    NULL,
	                    NULL) ? 1 : 0;
}

