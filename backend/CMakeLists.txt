project (compizconfig_ini)

set (LIBCOMPIZCONFIG_INI_REQUIRES
    libcompizconfig
    )

compiz_pkg_check_modules (LIBCOMPIZCONFIG_INI REQUIRED ${LIBCOMPIZCONFIG_INI_REQUIRES})

include_directories (
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBCOMPIZCONFIG_INI_INCLUDE_DIRS}
)

link_directories (
    ${LIBCOMPIZCONFIG_INI_LINK_DIRS}
)

set (COMPIZCONFIG_INI_LIBS "")
foreach (_val ${LIBCOMPIZCONFIG_INI_LDFLAGS})
    set (COMPIZCONFIG_INI_LIBS "${COMPIZCONFIG_INI_LIBS}${_val} ")
endforeach (_val ${LIBCOMPIZCONFIG_INI_LDFLAGS})

set (COMPIZCONFIG_INI_CFLAGS "")
foreach (_val ${LIBCOMPIZCONFIG_INI_CFLAGS})
    set (COMPIZCONFIG_INI_CFLAGS "${COMPIZCONFIG_INI_CFLAGS}${_val} ")
endforeach (_val ${LIBCOMPIZCONFIG_INI_CFLAGS})

add_library (ini SHARED
	     ini.c
)

target_link_libraries (
    ini
    compizconfig
)

set_target_properties (
    ini PROPERTIES
    INSTALL_RPATH "${COMPIZCONFIG_LIBDIR}"
)

install (
    TARGETS ini
    DESTINATION ${libdir}/compizconfig/backends
    )
