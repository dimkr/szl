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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "szl.h"

struct szl_flock {
	char *path;
	int fd;
	int locked;
};

static
enum szl_res szl_file_proc_size(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	struct stat stbuf;
	char *path;
	size_t len;

	if (!szl_as_str(interp, objv[1], &path, &len) ||
	    !len ||
	    (stat(path, &stbuf) < 0) ||
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
	    (stbuf.st_size > SZL_INT_MAX))
#ifdef __clang__
#pragma clang diagnostic pop
#endif
		return SZL_ERR;

	return szl_set_last_int(interp, (szl_int)stbuf.st_size);
}

static
enum szl_res szl_file_proc_delete(struct szl_interp *interp,
                                  const unsigned int objc,
                                  struct szl_obj **objv)
{
	char *path;
	size_t len;

	if (!szl_as_str(interp, objv[1], &path, &len) || !len || (unlink(path) < 0))
		return SZL_ERR;

	return SZL_OK;
}

static
void szl_file_lock_del(void *priv)
{
	struct szl_flock *lock = (struct szl_flock *)priv;

	if (lock->locked)
		lockf(lock->fd, F_ULOCK, 0);
	close(lock->fd);
	unlink(lock->path);
	free(lock->path);

	free(priv);
}

static
enum szl_res szl_file_lock_proc(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	struct szl_flock *lock = (struct szl_flock *)objv[0]->priv;
	char *op;
	size_t len;

	if (!szl_as_str(interp, objv[1], &op, &len) || !len)
		return SZL_ERR;

	if ((objc == 2) && (strcmp("unlock", op) == 0)) {
		if (lock->locked) {
			if (lockf(lock->fd, F_ULOCK, 0) < 0)
				return SZL_ERR;
			lock->locked = 0;
		}

		return SZL_OK;
	}

	return szl_set_last_help(interp, objv[0]);
}

static
enum szl_res szl_file_proc_lock(struct szl_interp *interp,
                                const unsigned int objc,
                                struct szl_obj **objv)
{
	struct szl_obj *name, *proc;
	struct szl_flock *lock;
	char *path;
	size_t len;

	if (!szl_as_str(interp, objv[1], &path, &len) || !len)
		return SZL_ERR;

	lock = (struct szl_flock *)malloc(sizeof(*lock));
	if (!lock)
		return SZL_ERR;

	lock->path = szl_strdup(interp, objv[1], NULL);
	if (!lock->path) {
		free(lock);
		return SZL_ERR;
	}

	lock->fd = open(path,
	                O_WRONLY | O_CREAT,
	                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (lock->fd < 0) {
		szl_set_last_fmt(interp, "failed to open %s", path);
		free(lock->path);
		free(lock);
		return SZL_ERR;
	}

	if (lockf(lock->fd, F_LOCK, 0) < 0) {
		szl_set_last_fmt(interp, "failed to lock %s", path);
		close(lock->fd);
		free(lock->path);
		free(lock);
		return SZL_ERR;
	}

	name = szl_new_str_fmt("file.lock:%d", lock->fd);
	if (!name) {
		lockf(lock->fd, F_ULOCK, 0);
		close(lock->fd);
		free(lock->path);
		free(lock);
		return SZL_ERR;
	}

	proc = szl_new_proc(interp,
	                    name,
	                    1,
	                    3,
	                    "lock unlock",
	                    szl_file_lock_proc,
	                    szl_file_lock_del,
	                    lock);
	szl_unref(name);
	if (!proc) {
		lockf(lock->fd, F_ULOCK, 0);
		close(lock->fd);
		free(lock->path);
		free(lock);
		return SZL_ERR;
	}

	lock->locked = 1;
	return szl_set_last(interp, proc);
}

static
enum szl_res szl_file_proc_locked(struct szl_interp *interp,
                                  const unsigned int objc,
                                  struct szl_obj **objv)
{
	char *path;
	size_t len;
	int fd;

	if (!szl_as_str(interp, objv[1], &path, &len) || !len)
		return SZL_ERR;

	fd = open(path, O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd < 0) {
		if (errno != ENOENT) {
			szl_set_last_fmt(interp, "failed to open %s", path);
			return SZL_ERR;
		}

		return szl_set_last_bool(interp, 0);
	}

	if (lockf(fd, F_TEST, 0) == 0) {
		close(fd);
		return szl_set_last_bool(interp, 0);
	}

	if ((errno != EAGAIN) && (errno != EACCES)) {
		close(fd);
		szl_set_last_fmt(interp, "failed to check whether %s is locked", path);
		return SZL_ERR;
	}

	return szl_set_last_bool(interp, 1);
}

static
const struct szl_ext_export file_exports[] = {
	{
		SZL_PROC_INIT("file.size",
		              "path",
		              2,
		              2,
		              szl_file_proc_size,
		              NULL)
	},
	{
		SZL_PROC_INIT("file.delete",
		              "path",
		              2,
		              2,
		              szl_file_proc_delete,
		              NULL)
	},
	{
		SZL_PROC_INIT("file.lock",
		              "path",
		              2,
		              2,
		              szl_file_proc_lock,
		              NULL)
	},
	{
		SZL_PROC_INIT("file.locked",
		              "path",
		              2,
		              2,
		              szl_file_proc_locked,
		              NULL)
	}
};

int szl_init_file(struct szl_interp *interp)
{
	return szl_new_ext(interp,
	                   "file",
	                   file_exports,
	                   sizeof(file_exports) / sizeof(file_exports[0]));
}
