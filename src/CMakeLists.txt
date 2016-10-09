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

include(CMakeParseArguments)
include(FindPkgConfig)

set(extensions archive builtin curl dict dir ed25519 env exc exec ext ffi file http io linenoise list logic loop lzfse math null obj path poll proc server signal socket str szl test time timer tls zlib zstd)

# add common definitions
execute_process(
	COMMAND git log -1 --format=%h
	OUTPUT_VARIABLE last_commit
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
add_definitions(-DSZL_VERSION="0.1-${last_commit}" -D_GNU_SOURCE)
set(cflags -std=c99 -Wall -Wno-overlength-strings -pedantic)
add_compile_options(${cflags})

set(extension_directory lib/szl)

# add libszl, the library implementing the interpreter
add_library(libszl libszl.c szl_builtin.c)
set_target_properties(libszl PROPERTIES PREFIX "")
target_compile_definitions(
	libszl
	PRIVATE -DSZL_EXT_DIR="${CMAKE_INSTALL_PREFIX}/${extension_directory}"
)
target_link_libraries(libszl -ldl)
install(
	TARGETS libszl
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)

# add szl, a non-interactive interpreter
add_executable(szl szl.c)
target_link_libraries(szl libszl)
install(
	TARGETS szl
	RUNTIME DESTINATION bin
)

# add szlsh, an interactive interpreter
configure_file(szlsh.in szlsh)
install(
	PROGRAMS szlsh
	DESTINATION bin
)

option(BUILD_SHARED_LIBS "Determines whether shared libraries should be built" ON)
if (BUILD_SHARED_LIBS)
	option(BUILTIN_ALL "Determines whether all extensions will be built-in" OFF)
else()
	message("Shared libraries are not built; marking all extensions as built-in")
	set(BUILTIN_ALL ON)
endif()

set(builtin_extensions)

function(add_extension name)
	set(options BUILTIN)
	set(multi_value_arguments LIBRARIES INCLUDES SOURCES DOCS)
	cmake_parse_arguments(add_extension "${options}" "" "${multi_value_arguments}" ${ARGN})

	if(NOT ${add_extension_BUILTIN})
		string(TOUPPER ${name} upper_name)
		set(builtin_opt BUILTIN_${upper_name})

		option(
			${builtin_opt}
			"Determines whether ${name} will be a built-in extension"
			OFF
		)

		if (BUILTIN_ALL OR ${${builtin_opt}})
			set(add_extension_BUILTIN TRUE)
		endif()
	endif()

	if(${add_extension_BUILTIN})
		target_sources(libszl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/szl_${name}.c)
		set(extension_target libszl)
		set(builtin_extensions ${builtin_extensions} ${name} PARENT_SCOPE)
	else()
		message("Building ${name} as an extension")
		set(extension_target szl_${name})
		add_library(${extension_target} MODULE ${CMAKE_CURRENT_SOURCE_DIR}/szl_${name}.c)
		target_link_libraries(${extension_target} libszl)
		set_target_properties(${extension_target} PROPERTIES PREFIX "")
		install(
			TARGETS ${extension_target}
			LIBRARY DESTINATION ${extension_directory}
		)
	endif()

	if(add_extension_INCLUDES)
		target_include_directories(${extension_target} PRIVATE ${add_extension_INCLUDES})
	endif()

	if (add_extension_LIBRARIES)
		target_link_libraries(${extension_target} ${add_extension_LIBRARIES})
	endif()

	if (add_extension_SOURCES)
		target_sources(${extension_target} PRIVATE ${add_extension_SOURCES})
	endif()

	if(add_extension_DOCS)
		foreach(doc ${add_extension_DOCS})
			get_filename_component(basename ${doc} NAME)
			install(
				FILES ${doc}
				DESTINATION share/doc/szl
				RENAME ${basename}.${name}
			)
		endforeach()
	endif()

	set(extension_szl ${CMAKE_CURRENT_SOURCE_DIR}/szl_${name}.szl)
	if (EXISTS ${extension_szl})
		set(extension_inc szl_${name}.inc)
		add_custom_command(
			OUTPUT ${extension_inc}
			COMMENT "Minifying ${name}"
			COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/minify.sh < ${extension_szl} > ${CMAKE_CURRENT_SOURCE_DIR}/${extension_inc}
		)
		target_sources(${extension_target} PRIVATE ${extension_inc})
	endif()
endfunction()

# add built-in extensions
foreach(extension obj proc exec str exc socket null logic file dir io list)
	add_extension(
		${extension}
		BUILTIN
	)
endforeach()

add_extension(
	math
	BUILTIN
	LIBRARIES -lm
)

foreach(extension loop ext time env path signal poll timer dict szl)
	add_extension(
		${extension}
		BUILTIN
	)
endforeach()

# add extra extensions
find_package(OpenSSL)
add_extension(
	tls
	LIBRARIES ${OPENSSL_LIBRARIES}
	INCLUDES ${OPENSSL_INCLUDE_DIR}
)

find_package(ZLIB)
add_extension(
	zlib
	LIBRARIES ${ZLIB_LIBRARIES}
	INCLUDES ${ZLIB_INCLUDE_DIRS}
)

add_extension(test)

pkg_check_modules(FFI libffi)
add_extension(
	ffi
	INCLUDES ${FFI_INCLUDE_DIRS}
	LIBRARIES ${FFI_LIBRARIES}
)

find_package(CURL)
add_extension(
	curl
	LIBRARIES ${CURL_LIBRARY}
	INCLUDES ${CURL_INCLUDE_DIR}
)

find_package(LibArchive)
add_extension(
	archive
	LIBRARIES ${LibArchive_LIBRARIES}
	INCLUDES ${LibArchive_INCLUDE_DIRS}
)

add_extension(
	linenoise
	INCLUDES linenoise
	SOURCES linenoise/linenoise.c
	DOCS linenoise/LICENSE
)
add_extension(
	ed25519
	INCLUDES ed25519/src
	SOURCES ed25519/src/add_scalar.c ed25519/src/fe.c ed25519/src/ge.c ed25519/src/key_exchange.c ed25519/src/keypair.c ed25519/src/sc.c ed25519/src/seed.c ed25519/src/sha512.c ed25519/src/sign.c ed25519/src/verify.c
	DOCS ed25519/license.txt
)
add_extension(
	lzfse
	INCLUDES lzfse/src
	SOURCES lzfse/src/lzfse_decode_base.c lzfse/src/lzfse_decode.c lzfse/src/lzfse_encode_base.c lzfse/src/lzfse_encode.c lzfse/src/lzfse_fse.c lzfse/src/lzvn_decode_base.c lzfse/src/lzvn_encode_base.c
	DOCS lzfse/LICENSE
)
add_extension(
	zstd
	INCLUDES zstd/lib/common zstd/lib
	SOURCES zstd/lib/compress/huf_compress.c zstd/lib/compress/zstd_compress.c zstd/lib/compress/fse_compress.c zstd/lib/compress/zbuff_compress.c zstd/lib/common/zstd_common.c zstd/lib/common/fse_decompress.c zstd/lib/common/entropy_common.c zstd/lib/common/xxhash.c zstd/lib/decompress/zbuff_decompress.c zstd/lib/decompress/zstd_decompress.c zstd/lib/decompress/huf_decompress.c
	DOCS zstd/LICENSE zstd/PATENTS
)
add_extension(server)
add_extension(http)

# once all extensions are added, generate szl_builtin.c, which initializes them
# during interpreter creation
add_custom_command(
	OUTPUT szl_builtin.c
	COMMENT "Generating szl_builtin.c"
	COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/mkbuiltin.sh ${builtin_extensions} > szl_builtin.c
)
set_source_files_properties(szl_builtin.c PROPERTIES GENERATED TRUE)
target_sources(libszl PRIVATE szl_builtin.c)
