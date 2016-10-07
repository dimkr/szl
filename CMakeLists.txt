cmake_minimum_required(VERSION 3.3)

add_subdirectory(src)
add_subdirectory(doc)

install(
	FILES README COPYING AUTHORS
	DESTINATION doc/szl
)
