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
#include <string.h>

#include <archive.h>
#include <archive_entry.h>

#include "szl.h"

struct szl_archive {
	struct archive *in;
	struct archive *out;
};

static
int szl_archive_iter(struct szl_interp *interp,
                     struct szl_archive *ar,
                     int (*cb)(struct szl_interp *,
                               struct szl_archive *,
                               const char *,
                               void *),
                     void *arg)
{
	struct archive_entry *entry;
	const char *path;

	do {
		switch (archive_read_next_header(ar->in, &entry)) {
			case ARCHIVE_OK:
				break;

			case ARCHIVE_EOF:
				return 1;

			default:
				szl_set_result_str(interp, "failed to read a file header", -1);
				return 0;
		}

		path = archive_entry_pathname(entry);
		if (!path)
			break;

		if (archive_write_header(ar->out, entry) != ARCHIVE_OK) {
			szl_set_result_str(interp, "failed to write a file header", -1);
			break;
		}

		if (!cb(interp, ar, path, arg))
			break;
	} while (1);

	return 0;
}

static
int szl_archive_append_path(struct szl_interp *interp,
                            struct szl_archive *ar,
                            const char *path,
                            void *arg)
{
	return szl_lappend_str(interp, (struct szl_obj *)arg, path);
}

static
enum szl_res szl_archive_list(struct szl_interp *interp,
                              struct szl_archive *ar)
{
	struct szl_obj *paths;

	paths = szl_new_empty();
	if (!paths)
		return SZL_ERR;

	if (!szl_archive_iter(interp, ar, szl_archive_append_path, paths)) {
		szl_obj_unref(paths);
		return SZL_ERR;
	}

	return szl_set_result(interp, paths);
}

static
int szl_archive_extract_file(struct szl_interp *interp,
                             struct szl_archive *ar,
                             const char *path,
                             void *arg)
{
	const void *blk;
	size_t size;
	__LA_INT64_T off;

	do {
		switch (archive_read_data_block(ar->in, &blk, &size, &off)) {
			case ARCHIVE_EOF:
				return 1;

			case ARCHIVE_OK:
				if (archive_write_data_block(ar->out,
				                             blk,
				                             size,
				                             off) == ARCHIVE_OK)
					break;
				/* fall through */

			default:
				szl_set_result_fmt(interp, "failed to extract %s", path);
				return 0;
		}
	} while (1);

	return 0;
}

static
enum szl_res szl_archive_extract(struct szl_interp *interp,
                                 struct szl_archive *ar)
{
	return szl_archive_iter(interp,
	                        ar,
	                        szl_archive_extract_file,
	                        NULL) ? SZL_OK : SZL_ERR;
}

static
enum szl_res szl_archive_proc(struct szl_interp *interp,
                              const int objc,
                              struct szl_obj **objv)
{
	const char *op;
	size_t len;

	op = szl_obj_str(interp, objv[1], &len);
	if (!op || !len)
		return SZL_ERR;

	if (strcmp("list", op) == 0)
		return szl_archive_list(interp, (struct szl_archive *)objv[0]->priv);
	else if (strcmp("extract", op) == 0) {
		return szl_archive_extract(interp, (struct szl_archive *)objv[0]->priv);
	}
	else
		return szl_usage(interp, objv[0]);

	return SZL_OK;
}

static
void szl_archive_del(void *priv)
{
	struct szl_archive *ar = (struct szl_archive *)priv;

	archive_write_close(ar->out);
	archive_write_free(ar->out);
	archive_read_free(ar->in);

	free(priv);
}

static
enum szl_res szl_archive_proc_open(struct szl_interp *interp,
                                   const int objc,
                                   struct szl_obj **objv)
{
	char name[sizeof("archive:"SZL_PASTE(SZL_INT_MIN))];
	struct szl_obj *proc;
	void *data;
	struct szl_archive *ar;
	size_t len;

	data = (void *)szl_obj_str(interp, objv[1], &len);
	if (!data || !len)
		return SZL_ERR;

	ar = (struct szl_archive *)malloc(sizeof(*ar));
	if (!ar)
		return SZL_ERR;

	ar->in = archive_read_new();
	if (!ar->in) {
		free(ar);
		return SZL_ERR;
	}

	ar->out = archive_write_disk_new();
	if (!ar->out) {
		free(ar);
		return SZL_ERR;
	}

	archive_read_support_filter_all(ar->in);
	archive_read_support_format_all(ar->in);

	archive_write_disk_set_options(ar->out,
	                               ARCHIVE_EXTRACT_OWNER |
	                               ARCHIVE_EXTRACT_PERM |
	                               ARCHIVE_EXTRACT_TIME |
	                               ARCHIVE_EXTRACT_UNLINK |
	                               ARCHIVE_EXTRACT_ACL |
	                               ARCHIVE_EXTRACT_FFLAGS |
	                               ARCHIVE_EXTRACT_XATTR);
	archive_write_disk_set_standard_lookup(ar->out);

	if (archive_read_open_memory(ar->in, data, len) != 0) {
		szl_set_result_str(interp, archive_error_string(ar->in), -1);
		szl_archive_del(ar);
		return SZL_ERR;
	}

	szl_new_obj_name(interp, "archive", name, sizeof(name), ar);
	proc = szl_new_proc(interp,
	                    name,
	                    1,
	                    3,
	                    "archive list|extract",
	                    szl_archive_proc,
	                    szl_archive_del,
	                    ar);
	if (!proc) {
		szl_archive_del(ar);
		return SZL_ERR;
	}

	return szl_set_result(interp, szl_obj_ref(proc));
}

int szl_init_archive(struct szl_interp *interp)
{
	return szl_new_proc(interp,
	                    "archive.open",
	                    2,
	                    3,
	                    "archive.open data",
	                    szl_archive_proc_open,
	                    NULL,
	                    NULL) ? 1 : 0;
}
