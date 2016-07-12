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
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <curl/curl.h>

#include "szl.h"

#define CONNECT_TIMEOUT 30
#define TIMEOUT 180

static
enum szl_res szl_curl_proc_encode(struct szl_interp *interp,
                                  const int objc,
                                  struct szl_obj **objv)
{
	const char *s;
	char *out;
	size_t len;
	enum szl_res res;

	s = szl_obj_str(interp, objv[1], &len);
	if (!s)
		return SZL_ERR;

	out = curl_easy_escape((CURL *)objv[0]->priv, s, (int)len);
	if (!out)
		return SZL_ERR;

	res = szl_set_result_str(interp, out, -1);
	curl_free(out);
	return res;
}

static
void szl_curl_encode_del(void *priv)
{
	curl_easy_cleanup((CURL *)priv);
}

static
int szl_curl_net_reachable(void)
{
	struct ifaddrs *ifap, *ifa;

	if (getifaddrs(&ifap) == -1)
		return 0;

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if ((!ifa->ifa_addr) || (strcmp("lo", ifa->ifa_name) == 0))
			continue;

		if ((ifa->ifa_addr->sa_family == AF_INET) ||
		    (ifa->ifa_addr->sa_family == AF_INET6)) {
			freeifaddrs(ifap);
			return 1;
		}
	}

	freeifaddrs(ifap);
	return 0;
}


static
enum szl_res szl_curl_proc_get(struct szl_interp *interp,
                               const int objc,
                               struct szl_obj **objv)
{
	sigset_t set, oldset, pend;
	CURLM *cm;
	CURL **cs;
	FILE **fhs;
	const char **urls, **paths;
	const struct CURLMsg *info;
	size_t len;
	int n, i, j, act, nfds, q, err;
	CURLMcode m;
	enum szl_res res = SZL_ERR;

	if (objc % 2 == 0)
		return szl_usage(interp, objv[0]);

	if (!szl_curl_net_reachable()) {
		szl_set_result_str(interp, "network is unreachable", -1);
		return SZL_ERR;
	}

	n = (objc - 1) / 2;

	cs = (CURL **)malloc(sizeof(CURL *) * n);
	if (!cs)
		return SZL_ERR;

	fhs = (FILE **)malloc(sizeof(FILE *) * n);
	if (!fhs) {
		free(cs);
		return SZL_ERR;
	}

	urls = (const char **)malloc(sizeof(char *) * n);
	if (!urls) {
		free(fhs);
		free(cs);
		return SZL_ERR;
	}

	paths = (const char **)malloc(sizeof(char *) * n);
	if (!paths) {
		free(urls);
		free(fhs);
		free(cs);
		return SZL_ERR;
	}

	cm = curl_multi_init();
	if (!cm)
		goto free_arrs;

	i = 1;
	j = 0;
	while (i < objc) {
		urls[j] = szl_obj_str(interp, objv[i], &len);
		if (!urls[j] || !len)
			goto cleanup_cm;

		paths[j] = szl_obj_str(interp, objv[i + 1], &len);
		if (!paths[j] || !len)
			goto cleanup_cm;

		i += 2;
		++j;
	}

	if ((sigemptyset(&set) == -1) ||
	    (sigaddset(&set, SIGTERM) == -1) ||
	    (sigaddset(&set, SIGINT) == -1) ||
	    (sigprocmask(SIG_BLOCK, &set, &oldset) == -1))
		goto cleanup_cm;

	for (i = 0; i < n; ++i) {
		fhs[i] = fopen(paths[i], "w");
		if (!fhs[i]) {
			err = errno;

			for (j = 0; j < i; ++j) {
				fclose(fhs[j]);
				unlink(paths[j]);
			}

			szl_set_result_fmt(interp,
			                   "failed to open %s: %s",
			                   paths[i],
			                   strerror(err));
			goto restore_sigmask;
		}
	}

	for (i = 0; i < n; ++i) {
		cs[i] = curl_easy_init();
		if (!cs[i]) {
			for (j = 0; j < i; ++j)
				curl_easy_cleanup(cs[j]);

			goto close_fhs;
		}
	}

	for (i = 0; i < n; ++i) {
		if ((curl_easy_setopt(cs[i], CURLOPT_FAILONERROR, 1) != CURLE_OK) ||
		    (curl_easy_setopt(cs[i], CURLOPT_TCP_NODELAY, 1) != CURLE_OK) ||
		    (curl_easy_setopt(cs[i], CURLOPT_USE_SSL, CURLUSESSL_TRY) != CURLE_OK) ||
		    (curl_easy_setopt(cs[i], CURLOPT_WRITEFUNCTION, fwrite) != CURLE_OK) ||
		    (curl_easy_setopt(cs[i], CURLOPT_WRITEDATA, fhs[i]) != CURLE_OK) ||
		    (curl_easy_setopt(cs[i], CURLOPT_URL, urls[i]) != CURLE_OK) ||
		    (curl_easy_setopt(cs[i], CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT) != CURLE_OK) ||
		    (curl_easy_setopt(cs[i], CURLOPT_TIMEOUT, TIMEOUT)) != CURLE_OK)
			goto cleanup_cs;
	}

	for (i = 0; i < n; ++i) {
		if (curl_multi_add_handle(cm, cs[i]) != CURLM_OK) {
			for (j = 0; j < i; ++j)
				curl_multi_remove_handle(cm, cs[j]);

			goto cleanup_cs;
		}
	}

	do {
		if ((sigpending(&pend) == -1) ||
		    (sigismember(&pend, SIGTERM)) ||
		    (sigismember(&pend, SIGINT)))
			break;

		m = curl_multi_perform(cm, &act);
		if (m != CURLM_OK) {
			szl_set_result_str(interp, curl_multi_strerror(m), -1);
			break;
		}

		if (act == 0) {
			info = curl_multi_info_read(cm, &q);
			if (info && (info->msg == CURLMSG_DONE)) {
				if (info->data.result == CURLE_OK)
					res = SZL_OK;
				else
					szl_set_result_str(interp,
					                   curl_easy_strerror(info->data.result),
					                   -1);
			}
			break;
		}

		m = curl_multi_wait(cm, NULL, 0, 1000, &nfds);
		if (m != CURLM_OK) {
			szl_set_result_str(interp, curl_multi_strerror(m), -1);
			break;
		}

		if (!nfds)
			usleep(100000);
	} while (1);

	for (i = 0; i < n; ++i)
		curl_multi_remove_handle(cm, cs[i]);

cleanup_cs:
	for (i = 0; i < n; ++i)
		curl_easy_cleanup(cs[i]);

close_fhs:
	for (i = 0; i < n; ++i)
		fclose(fhs[i]);

	if (res != SZL_OK) {
		for (i = 0; i < n; ++i)
			unlink(paths[i]);
	}

restore_sigmask:
	sigprocmask(SIG_SETMASK, &oldset, NULL);

cleanup_cm:
	curl_multi_cleanup(cm);

free_arrs:
	free(paths);
	free(urls);
	free(fhs);
	free(cs);

	return res;
}

int szl_init_curl(struct szl_interp *interp)
{
	CURL *curl;

	curl = curl_easy_init();
	if (!curl)
		return 0;

	if (!szl_new_proc(interp,
	                  "curl.encode",
	                  2,
	                  2,
	                  "curl.encode str",
	                  szl_curl_proc_encode,
	                  szl_curl_encode_del,
	                  curl)) {
		curl_easy_cleanup(curl);
		return 0;
	}

	if (!szl_new_proc(interp,
	                  "curl.get",
	                  3,
	                  -1,
	                  "curl.get url path...",
	                  szl_curl_proc_get,
	                  NULL,
	                  NULL))
		return 0;

	return 1;
}
