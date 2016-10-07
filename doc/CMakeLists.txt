configure_file(szl.1.in szl.1)
install(
	FILES szl.1 szlsh.1
	DESTINATION man/man1
)

add_custom_command(
	OUTPUT szl.html
	COMMENT "Generating user manual"
	COMMAND asciidoc -a toc -a toclevels=4 -o szl.html szl.txt
)
install(
	FILES szl.html
	DESTINATION doc/szl
)

add_custom_target(
	api_doc_gen
	COMMENT "Generating API documentation"
	COMMAND doxygen doxygen.conf
)
add_custom_target(api_doc ALL DEPENDS api_doc_gen)
install(
	DIRECTORY api
	DESTINATION doc/szl
)
