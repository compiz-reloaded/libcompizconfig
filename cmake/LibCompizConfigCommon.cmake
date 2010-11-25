find_package (Compiz REQUIRED)

include (CompizCommon)
include (CompizPackage)

macro (_get_backend_parameters _prefix)
    set (_current_var _foo)
    set (_supported_var PKGDEPS LDFLAGSADD CFLAGSADD LIBRARIES LIBDIRS INCDIRS)
    foreach (_val ${_supported_var})
	set (${_prefix}_${_val})
    endforeach (_val)
    foreach (_val ${ARGN})
	set (_found FALSE)
	foreach (_find ${_supported_var})
	    if ("${_find}" STREQUAL "${_val}")
		set (_found TRUE)
	    endif ("${_find}" STREQUAL "${_val}")
	endforeach (_find)

	if (_found)
	    set (_current_var ${_prefix}_${_val})
	else (_found)
	    list (APPEND ${_current_var} ${_val})
	endif (_found)
    endforeach (_val)
endmacro (_get_backend_parameters)

# check pkgconfig dependencies
macro (_check_backend_pkg_deps _prefix)
    set (${_prefix}_HAS_PKG_DEPS TRUE)
    foreach (_val ${ARGN})
        string (REGEX REPLACE "[<>=\\.]" "_" _name ${_val})
	string (TOUPPER ${_name} _name)

	compiz_pkg_check_modules (_${_name} ${_val})

	if (_${_name}_FOUND)
	    list (APPEND ${_prefix}_PKG_LIBDIRS "${_${_name}_LIBRARY_DIRS}")
	    list (APPEND ${_prefix}_PKG_LIBRARIES "${_${_name}_LIBRARIES}")
	    list (APPEND ${_prefix}_PKG_INCDIRS "${_${_name}_INCLUDE_DIRS}")
	else ()
	    set (${_prefix}_HAS_PKG_DEPS FALSE)
	    compiz_set (COMPIZCONFIG_${_prefix}_MISSING_DEPS "${COMPIZCONFIG_${_prefix}_MISSING_DEPS} ${_val}")
	    set(__pkg_config_checked__${_name} 0 CACHE INTERNAL "" FORCE)
	endif ()
    endforeach ()
endmacro ()

function (compizconfig_backend bname)
    string (TOUPPER ${bname} _BACKEND)

    project (compizconfig_${_BACKEND})

    _get_backend_parameters (compizconfig_${_BACKEND} ${ARGN})
    _check_backend_pkg_deps (compizconfig_${_BACKEND} ${compizconfig_${_BACKEND}_PKGDEPS})

    include_directories (
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${BACKEND_INCLUDE_DIRS}
        ${compizconfig_${_BACKEND}_LOCAL_INCDIRS}
        ${compizconfig_${_BACKEND}_PKG_INCDIRS}
        ${compizconfig_${_BACKEND}_INCDIRS}
        ${COMPIZCONFIG_INCLUDE_DIRS}
    )

    link_directories (
	${compizconfig_${_BACKEND}_PKG_LIBDIRS}
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

    file (GLOB _h_files "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
    file (GLOB _cpp_files "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
    file (GLOB _c_files "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

    add_library (
	${bname} SHARED ${_cpp_files}
		        ${_h_files}
		        ${_c_files})

    target_link_libraries (
	${bname}
	compizconfig
        ${compizconfig_${_BACKEND}_LOCAL_LIBRARIES}
        ${compizconfig_${_BACKEND}_PKG_LIBRARIES}
        ${compizconfig_${_BACKEND}_LIBRARIES}
    )

    if (COMPIZ_BUILD_WITH_RPATH)

	set_target_properties (
	    ${bname} PROPERTIES
	    INSTALL_RPATH "${COMPIZCONFIG_LIBDIR}"
	    COMPILE_FLAGS "${${_BACKEND}_CFLAGSADD}"
	    LINK_FLAGS "${${_BACKEND}_LDFLAGSADD}"
	)

    else (COMPIZ_BUILD_WITH_RPATH)

	set_target_properties (
	    ${bname} PROPERTIES
	    COMPILE_FLAGS "${${_BACKEND}_CFLAGSADD}"
	    LINK_FLAGS "${${_BACKEND}_LDFLAGSADD}"
	)
    endif ()

    install (
	TARGETS ${bname}
	DESTINATION ${COMPIZ_DESTDIR}${COMPIZCONFIG_LIBDIR}/compizconfig/backends
    )
endfunction (compizconfig_backend bname)
