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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "szl.h"

#if BUFSIZ > 64 * 1024
#	define SZL_BINARY_FILE_BUFSIZ BUFSIZ
#else
#	define SZL_BINARY_FILE_BUFSIZ 64 * 1024
#endif

static const char szl_io_inc[] = {
#include "szl_io.inc"
};

struct szl_flock {
	char *path;
	int fd;
	int locked;
};

static
ssize_t szl_file_read(void *priv, unsigned char *buf, const size_t len)
{
	size_t ret;

	ret = fread(buf, 1, len, (FILE *)priv);
	if (!ret) {
		if (ferror((FILE *)priv))
			return -1;
	}

	return (ssize_t)ret;
}

static
ssize_t szl_file_write(void *priv, const unsigned char *buf, const size_t len)
{
	size_t total = 0;
	size_t chunk;

	do {
		chunk = fwrite(buf + total, 1, len - total, (FILE *)priv);
		if (chunk == 0) {
			if (ferror((FILE *)priv))
				return -1;
			break;
		}
		total += chunk;
	} while (total < len);

	return total;
}

static
enum szl_res szl_file_flush(void *priv)
{
	if (fflush((FILE *)priv) == EOF)
		return SZL_ERR;

	return SZL_OK;
}

static
void szl_file_close(void *priv)
{
	fclose((FILE *)priv);
}

static
szl_int szl_file_handle(void *priv)
{
	return (szl_int)fileno((FILE *)priv);
}

static const struct szl_stream_ops szl_file_ops = {
	szl_file_read,
	szl_file_write,
	szl_file_flush,
	szl_file_close,
	NULL,
	szl_file_handle
};

static
int szl_io_enable_buffering(struct szl_stream *strm, FILE *fp, const char *mode)
{
	if (strchr(mode, 'b')) {
		strm->buf = malloc(SZL_BINARY_FILE_BUFSIZ);
		if (!strm->buf)
			return 0;

		if (setvbuf(fp, strm->buf, _IOFBF, SZL_BINARY_FILE_BUFSIZ) != 0) {
			free(strm->buf);
			return 0;
		}
	}
	else
		strm->buf = NULL;

	return 1;
}

static
int szl_new_file(struct szl_interp *interp, FILE *fp, const char *mode)
{
	struct szl_obj *obj;
	struct szl_stream *strm;

	strm = (struct szl_stream *)malloc(sizeof(struct szl_stream));
	if (!strm)
		return 0;

	strm->ops = &szl_file_ops;
	strm->closed = 0;
	strm->priv = fp;

	obj = szl_new_stream(interp, strm, "file");
	if (!obj) {
		szl_stream_free(strm);
		return 0;
	}

	if (!szl_io_enable_buffering(strm, fp, mode)) {
		szl_stream_free(strm);
		return 0;
	}

	szl_set_result(interp, obj);
	return 1;
}

static
enum szl_res szl_io_proc_open(struct szl_interp *interp,
                              const int objc,
                              struct szl_obj **objv)
{
	const char *path, *mode;
	FILE *fp;
	size_t len;

	path = szl_obj_str(objv[1], &len);
	if (!path || !len)
		return SZL_ERR;

	mode = szl_obj_str(objv[2], &len);
	if (!mode || !len)
		return SZL_ERR;

	fp = fopen(path, mode);
	if (!fp)
		return SZL_ERR;

	if (!szl_new_file(interp, fp, mode)) {
		fclose(fp);
		return SZL_ERR;
	}

	return SZL_OK;
}

static
enum szl_res szl_io_proc_fdopen(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	const char *mode;
	FILE *fp;
	szl_int fd;
	size_t len;

	if ((!szl_obj_int(objv[1], &fd)) || (fd < 0) || (fd > INT_MAX))
		return SZL_ERR;

	mode = szl_obj_str(objv[2], &len);
	if (!mode || !len)
		return SZL_ERR;

	fp = fdopen((int)fd, mode);
	if (!fp)
		return SZL_ERR;

	if (!szl_new_file(interp, fp, mode)) {
		fclose(fp);
		return SZL_ERR;
	}

	return SZL_OK;
}

static
enum szl_res szl_io_proc_dup(struct szl_interp *interp,
                             const int objc,
                             struct szl_obj **objv)
{
	szl_int fd;

	if ((!szl_obj_int(objv[1], &fd)) || (fd < 0) || (fd > INT_MAX))
		return SZL_ERR;

	fd = (szl_int)dup((int)fd);
	if (fd < 0)
		return SZL_ERR;

	return szl_set_result_int(interp, fd);
}

static
enum szl_res szl_io_proc_size(struct szl_interp *interp,
                              const int objc,
                              struct szl_obj **objv)
{
	struct stat stbuf;
	const char *path;
	size_t len;

	path = szl_obj_str(objv[1], &len);
	if (!path || !len)
		return SZL_ERR;

	if ((stat(path, &stbuf) < 0) || (stbuf.st_size > SZL_INT_MAX))
		return SZL_ERR;

	return szl_set_result_int(interp, (szl_int)stbuf.st_size);
}

static
enum szl_res szl_io_proc_delete(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	const char *path;
	size_t len;

	path = szl_obj_str(objv[1], &len);
	if (!path || !len)
		return SZL_ERR;

	if (unlink(path) < 0)
		return SZL_ERR;

	return SZL_OK;
}

static
enum szl_res szl_io_proc_mkdir(struct szl_interp *interp,
                               const int objc,
                               struct szl_obj **objv)
{
	const char *path;
	size_t len;

	path = szl_obj_str(objv[1], &len);
	if (!path || !len)
		return SZL_ERR;

	if (mkdir(path, 0755) < 0)
		return SZL_ERR;

	return SZL_OK;
}

static
enum szl_res szl_io_proc_cd(struct szl_interp *interp,
                            const int objc,
                            struct szl_obj **objv)
{
	const char *path;
	size_t len;

	path = szl_obj_str(objv[1], &len);
	if (!path || !len)
		return SZL_ERR;

	if (chdir(path) < 0)
		return SZL_ERR;

	return SZL_OK;
}

static
void szl_flock_del(void *priv)
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
enum szl_res szl_flock_proc(struct szl_interp *interp,
                            const int objc,
                            struct szl_obj **objv)
{
	struct szl_flock *lock = (struct szl_flock *)objv[0]->priv;
	const char *op;
	size_t len;

	op = szl_obj_str(objv[1], &len);
	if (!op || !len)
		return SZL_ERR;

	if ((objc == 2) && (strcmp("unlock", op) == 0)) {
		if (lock->locked) {
			if (lockf(lock->fd, F_ULOCK, 0) < 0)
				return SZL_ERR;
			lock->locked = 0;
		}

		return SZL_OK;
	}

	return szl_usage(interp, objv[0]);
}

static
enum szl_res szl_io_proc_lock(struct szl_interp *interp,
                              const int objc,
                              struct szl_obj **objv)
{
	char name[sizeof("file.lock:"SZL_PASTE(SZL_INT_MIN))];
	struct szl_obj *proc;
	struct szl_flock *lock;
	const char *path;
	size_t len;

	path = szl_obj_str(objv[1], &len);
	if (!path || !len)
		return SZL_ERR;

	lock = (struct szl_flock *)malloc(sizeof(*lock));
	if (!lock)
		return SZL_ERR;

	lock->path = szl_obj_strdup(objv[1], NULL);
	if (!lock->path) {
		free(lock);
		return SZL_ERR;
	}

	lock->fd = open(path,
	                O_WRONLY | O_CREAT,
	                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (lock->fd < 0) {
		szl_set_result_fmt(interp, "failed to open %s", path);
		free(lock->path);
		free(lock);
		return SZL_ERR;
	}

	if (lockf(lock->fd, F_LOCK, 0) < 0) {
		szl_set_result_fmt(interp, "failed to lock %s", path);
		free(lock->path);
		free(lock);
		return SZL_ERR;
	}

	szl_new_obj_name(interp,
	                 "file.lock",
	                 name,
	                 sizeof(name),
	                 (szl_int)(intptr_t)lock);
	proc = szl_new_proc(interp,
	                    name,
	                    1,
	                    3,
	                    "lock unlock",
	                    szl_flock_proc,
	                    szl_flock_del,
	                    lock);
	if (!proc) {
		free(lock->path);
		free(lock);
		return SZL_ERR;
	}

	lock->locked = 1;
	return szl_set_result(interp, szl_obj_ref(proc));
}

static
enum szl_res szl_io_proc_locked(struct szl_interp *interp,
                                const int objc,
                                struct szl_obj **objv)
{
	const char *path;
	size_t len;
	int fd;

	path = szl_obj_str(objv[1], &len);
	if (!path || !len)
		return SZL_ERR;

	fd = open(path, O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd < 0) {
		if (errno != ENOENT) {
			szl_set_result_fmt(interp, "failed to open %s", path);
			return SZL_ERR;
		}

		return szl_set_result_bool(interp, 0);
	}

	if (lockf(fd, F_TEST, 0) == 0) {
		close(fd);
		return szl_set_result_bool(interp, 0);
	}

	if ((errno != EAGAIN) && (errno !=  EACCES)) {
		close(fd);
		szl_set_result_fmt(interp,
		                   "failed to check whether %s is locked",
		                   path);
		return SZL_ERR;
	}

	return szl_set_result_bool(interp, 1);
}

int szl_init_io(struct szl_interp *interp)
{
	return ((szl_new_proc(interp,
	                      "open",
	                      3,
	                      3,
	                      "open path mode",
	                      szl_io_proc_open,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "_io.fdopen",
	                      3,
	                      3,
	                      "_io.fdopen handle mode",
	                      szl_io_proc_fdopen,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "_io.dup",
	                      2,
	                      2,
	                      "_io.dup handle",
	                      szl_io_proc_dup,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "file.size",
	                      2,
	                      2,
	                      "file.size path",
	                      szl_io_proc_size,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "file.delete",
	                      2,
	                      2,
	                      "file.delete path",
	                      szl_io_proc_delete,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "mkdir",
	                      2,
	                      2,
	                      "mkdir path",
	                      szl_io_proc_mkdir,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "cd",
	                      2,
	                      2,
	                      "cd path",
	                      szl_io_proc_cd,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "file.lock",
	                      2,
	                      2,
	                      "file.lock path",
	                      szl_io_proc_lock,
	                      NULL,
	                      NULL)) &&
	        (szl_new_proc(interp,
	                      "file.locked",
	                      2,
	                      2,
	                      "file.locked path",
	                      szl_io_proc_locked,
	                      NULL,
	                      NULL)) &&
	        (szl_run_const(interp,
	                       szl_io_inc,
	                       sizeof(szl_io_inc) - 1) == SZL_OK));
}
