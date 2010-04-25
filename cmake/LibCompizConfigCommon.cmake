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
	
    find_file (BACKEND_C_FILE ${bname}.c ${CMAKE_CURRENT_SOURCE_DIR})
    
    if (NOT BACKEND_C_FILE)
	find_file (BACKEND_CPP_FILE ${bname}.cpp ${CMAKE_CURRENT_SOURCE_DIR})
    endif (NOT BACKEND_C_FILE)
    
    if (BACKEND_C_FILE)
	add_library (${bname} SHARED
		     ${bname}.c
        )
    endif (BACKEND_C_FILE)
    
    if (BACKEND_CPP_FILE)
	add_library (${bname} SHARED
		     ${bname}.cpp
	)
    endif (BACKEND_CPP_FILE)

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
