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

configure_file(szl.1.in szl.1)
install(
	FILES szl.1 szlsh.1
	DESTINATION share/man/man1
)

add_custom_command(
	OUTPUT manual_gen
	COMMENT "Generating user manual"
	COMMAND asciidoc -a toc -a toclevels=4 -o szl.html szl.txt
)
add_custom_target(manual ALL DEPENDS manual_gen)
install(
	FILES szl.html
	DESTINATION share/doc/szl
)

add_custom_target(
	api_doc_gen
	COMMENT "Generating API documentation"
	COMMAND doxygen doxygen.conf
)
add_custom_target(api_doc ALL DEPENDS api_doc_gen)
install(
	DIRECTORY api
	DESTINATION share/doc/szl
)
