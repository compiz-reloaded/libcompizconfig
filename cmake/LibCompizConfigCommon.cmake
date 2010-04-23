function (compizconfig_backend bname)
    project (compizconfig_${bname})

    find_package (Compiz REQUIRED)

    include (CompizCommon)

    set (BACKEND_REQUIRES
        libcompizconfig
        )

    compiz_pkg_check_modules (BACKEND REQUIRED ${BACKEND_REQUIRES})

    include_directories (
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${BACKEND_INCLUDE_DIRS}
    )

    link_directories (
	${COMPIZCONFIG_LIBRARY_DIRS}
    )

    set (BACKEND_LIBS "")
    foreach (_val ${BACKEND_LDFLAGS})
	set (BACKEND_LIBS "${BACKEND_LIBS}${_val} ")
    endforeach (_val ${BACKEND_LDFLAGS})

    set (BACKEND_CFLAGS "")
	foreach (_val ${LIBBACKEND_CFLAGS})
    set (BACKEND_CFLAGS "${BACKEND_CFLAGS}${_val} ")
	endforeach (_val ${LIBBACKEND_CFLAGS})

    add_library (${bname} SHARED
		 ${bname}.c
    )

    target_link_libraries (
	${bname}
	compizconfig
    )

    set_target_properties (
	${bname} PROPERTIES
	INSTALL_RPATH "${COMPIZCONFIG_LIBDIR}"
    )

    install (
	TARGETS ${bname}
	DESTINATION ${COMPIZCONFIG_LIBDIR}/compizconfig/backends
    )
endfunction ()
