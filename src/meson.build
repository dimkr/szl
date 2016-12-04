# this file is part of szl.
#
# Copyright (c) 2016 Dima Krasner
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

ext_dir = join_paths(get_option('prefix'), get_option('libdir'), 'szl')
add_project_arguments(['-std=gnu99', '-D_GNU_SOURCE', '-DSZL_VERSION="@0@"'.format(meson.project_version()), '-DSZL_EXT_DIR="@0@"'.format(ext_dir), '-Wall', '-pedantic', '-Wno-overlength-strings'],
                      language: 'c')

doc_dir = join_paths(get_option('datadir'), 'doc', 'szl')

builtin_exts = ['obj', 'proc', 'exec', 'str', 'exc', 'socket', 'null', 'logic', 'file', 'dir', 'io', 'list', 'math', 'loop', 'ext', 'time', 'env', 'path', 'signal', 'poll', 'timer', 'dict', 'szl']
exts = []

libszl_srcs = ['libszl.c']

cc = meson.get_compiler('c')
dl_dep = cc.find_library('dl', required: false)
libszl_deps = []

builtin_all = get_option('builtin_all')

if builtin_all
	builtin_tls = true
else
	builtin_tls = get_option('builtin_tls')
endif
openssl_dep = dependency('openssl')
if builtin_tls
	builtin_exts += 'tls'
	libszl_deps += openssl_dep
endif

if builtin_all
	builtin_zlib = true
else
	builtin_zlib = get_option('builtin_zlib')
endif
zlib_dep = dependency('zlib')
if builtin_zlib
	builtin_exts += 'zlib'
	libszl_deps += zlib_dep
endif

if builtin_all
	builtin_ffi = true
else
	builtin_ffi = get_option('builtin_ffi')
endif
libffi_dep = dependency('libffi')
if builtin_ffi
	builtin_exts += 'ffi'
	libszl_deps += libffi_dep
endif

if builtin_all
	builtin_curl = true
else
	builtin_curl = get_option('builtin_curl')
endif
libcurl_dep = dependency('libcurl')
if builtin_curl
	builtin_exts += 'curl'
	libszl_deps += libcurl_dep
endif

if builtin_all
	builtin_archive = true
else
	builtin_archive = get_option('builtin_archive')
endif
libarchive_dep = dependency('libarchive')
if builtin_archive
	builtin_exts += 'archive'
	libszl_deps += libarchive_dep
endif

if builtin_all
	builtin_linenoise = true
else
	builtin_linenoise = get_option('builtin_linenoise')
endif
linenoise_dep = declare_dependency(sources: join_paths('linenoise', 'linenoise.c'))
if builtin_linenoise
	builtin_exts += 'linenoise'
	libszl_deps += linenoise_dep
endif
install_data(join_paths('linenoise', 'LICENSE'),
             install_dir: join_paths(doc_dir, 'linenoise'))

ed25519_dep = declare_dependency(sources: ['ed25519/src/add_scalar.c', 'ed25519/src/fe.c', 'ed25519/src/ge.c', 'ed25519/src/key_exchange.c', 'ed25519/src/keypair.c', 'ed25519/src/sc.c', 'ed25519/src/seed.c', 'ed25519/src/sha512.c', 'ed25519/src/sign.c', 'ed25519/src/verify.c'])
if builtin_all
	builtin_ed25519 = true
else
	builtin_ed25519 = get_option('builtin_ed25519')
endif
if builtin_ed25519
	builtin_exts += 'ed25519'
	libszl_deps += ed25519_dep
endif
install_data(join_paths('ed25519', 'license.txt'),
             install_dir: join_paths(doc_dir, 'ed25519'))

lzfse_dep = declare_dependency(sources: ['lzfse/src/lzfse_decode_base.c', 'lzfse/src/lzfse_decode.c', 'lzfse/src/lzfse_encode_base.c', 'lzfse/src/lzfse_encode.c', 'lzfse/src/lzfse_fse.c', 'lzfse/src/lzvn_decode_base.c', 'lzfse/src/lzvn_encode_base.c'],
                               compile_args: ['-Wno-unknown-pragmas', '-Wno-unused-variable', '-Wno-pedantic'])
if builtin_all
	builtin_lzfse = true
else
	builtin_lzfse = get_option('builtin_lzfse')
endif
if builtin_lzfse
	builtin_exts += 'lzfse'
	libszl_deps += lzfse_dep
endif
install_data(join_paths('lzfse', 'LICENSE'),
             install_dir: join_paths(doc_dir, 'lzfse'))

zstd_dep = declare_dependency(include_directories: include_directories('zstd/lib/common', 'zstd/lib'),
                              sources: ['zstd/lib/common/error_private.c', 'zstd/lib/compress/huf_compress.c', 'zstd/lib/compress/zstd_compress.c', 'zstd/lib/compress/fse_compress.c', 'zstd/lib/compress/zbuff_compress.c', 'zstd/lib/common/zstd_common.c', 'zstd/lib/common/fse_decompress.c', 'zstd/lib/common/entropy_common.c', 'zstd/lib/common/xxhash.c', 'zstd/lib/decompress/zbuff_decompress.c', 'zstd/lib/decompress/zstd_decompress.c', 'zstd/lib/decompress/huf_decompress.c'])
if builtin_all
	builtin_zstd = true
else
	builtin_zstd = get_option('builtin_zstd')
endif
if builtin_zstd
	builtin_exts += 'zstd'
	libszl_deps += zstd_dep
endif
install_data(join_paths('zstd', 'LICENSE'), join_paths('zstd', 'PATENTS'),
             install_dir: join_paths(doc_dir, 'zstd'))

builtin_exts += ['test', 'oop', 'server', 'resp', 'http', 'https']

builtin_c = custom_target('builtin',
                          output: 'szl_builtin.c',
                          command: ['sh', join_paths(meson.current_source_dir(), 'mkbuiltin.sh')] + builtin_exts,
                          capture: true)

foreach builtin: builtin_exts
	libszl_srcs += 'szl_@0@.c'.format(builtin)
	builtin_szl = 'szl_@0@.szl'.format(builtin)
	if run_command('[', '-f', builtin_szl, ']').returncode() == 0
		target = '@0@_h'.format(builtin)
		set_variable(target,
		             custom_target(target,
		                           output: 'szl_@0@.h'.format(builtin),
		                           command: ['sh', join_paths(meson.current_source_dir(), 'minify.sh'), join_paths(meson.current_source_dir(), builtin_szl)],
		                           input: builtin_szl,
		                           capture: true))
		libszl_deps += declare_dependency(sources: get_variable(target))
	endif
endforeach

if builtin_all
	libszl = static_library('szl',
	                        libszl_srcs + [builtin_c],
	                        dependencies: libszl_deps + [dl_dep, cc.find_library('m', required: false)],
	                        install: true)
else
	libszl = shared_library('szl',
	                        libszl_srcs + [builtin_c],
	                        dependencies: libszl_deps + [dl_dep, cc.find_library('m', required: false)],
	                        install: true)
endif
install_headers('szl.h')

if not (builtin_tls or builtin_all)
	shared_library('szl_tls',
	               ['szl_tls.c'],
	               dependencies: [openssl_dep],
	               link_with: libszl,
	               name_prefix: '',
	               install: true)
endif

if not (builtin_zlib or builtin_all)
	shared_library('szl_zlib',
	               ['szl_zlib.c'],
	               dependencies: [zlib_dep],
	               link_with: libszl,
	               name_prefix: '',
	               install: true)
endif

if not (builtin_ffi or builtin_all)
	shared_library('szl_ffi',
	               ['szl_ffi.c'],
	               dependencies: [libffi_dep, dl_dep],
	               link_with: libszl,
	               name_prefix: '',
	               install: true)
endif

if not (builtin_curl or builtin_all)
	shared_library('szl_curl',
	               ['szl_curl.c'],
	               dependencies: [libcurl_dep],
	               link_with: libszl,
	               name_prefix: '',
	               install: true)
endif

if not (builtin_archive or builtin_all)
	shared_library('szl_archive',
	               ['szl_archive.c'],
	               dependencies: [libarchive_dep],
	               link_with: libszl,
	               name_prefix: '',
	               install: true)
endif

if not (builtin_linenoise or builtin_all)
	shared_library('szl_linenoise',
	               ['szl_linenoise.c'],
	               link_with: libszl,
	               dependencies: linenoise_dep,
	               name_prefix: '',
	               install: true)
endif

if not (builtin_ed25519 or builtin_all)
	shared_library('szl_ed25519',
	               ['szl_ed25519.c'],
	               link_with: libszl,
	               dependencies: ed25519_dep,
	               name_prefix: '',
	               install: true)
endif

if not (builtin_lzfse or builtin_all)
	shared_library('szl_lzfse',
	               ['szl_lzfse.c'],
	               link_with: libszl,
	               dependencies: lzfse_dep,
	               name_prefix: '',
	               install: true)
endif

if not (builtin_zstd or builtin_all)
	shared_library('szl_zstd',
	               ['szl_zstd.c'],
	               link_with: libszl,
	               dependencies: zstd_dep,
	               name_prefix: '',
	               install: true)
endif

foreach ext: ['test', 'oop', 'server', 'resp', 'http', 'https']
	builtin_ext = get_option('builtin_@0@'.format(ext))
	if not (builtin_ext or builtin_all)
		shared_library('szl_@0@'.format(ext),
		               ['szl_@0@.c'.format(ext)],
		               link_with: libszl,
		               dependencies: declare_dependency(sources: [get_variable('@0@_h'.format(ext))]),
		               name_prefix: '',
		               install: true)
	endif
endforeach

executable('szl', 'szl.c', link_with: libszl, install: true)

szlsh_cfg = configuration_data()
szlsh_cfg.set('prefix', get_option('prefix'))
bindir = get_option('bindir')
szlsh_cfg.set('bindir', bindir)
configure_file(input: 'szlsh.in',
               output: 'szlsh',
               install_dir: bindir,
               configuration: szlsh_cfg)
