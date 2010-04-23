################################################################################
#
# FindCompizConfig
#
# This module finds LibCompizConfig (www.compiz.org). It uses FindPkgConfig to
# locate CompizConfig and adds the CompizConfig CMake module path to the CMake
# module path.  It also loads the LibCompizConfigDefaults that sets all variables
# to compile Compiz modules. 
#
# This module sets the following variables:
#   COMPIZCONFIG_FOUND          ... set to true if compiz and its CompizDefaults macro
#                             is found
#
# Variables set by the FindPkgConfig macro for compiz
#   COMPIZCONFIG_LIBRARY_DIRS   ... the paths of the libraries (w/o the '-L')
#   COMPIZCONFIG_LDFLAGS        ... all required linker flags
#   COMPIZCONFIG_LDFLAGS_OTHER  ... all other linker flags
#   COMPIZCONFIG_INCLUDE_DIRS   ... the '-I' preprocessor flags (w/o the '-I')
#   COMPIZCONFIG_CFLAGS         ... all required cflags
#   COMPIZCONFIG_CFLAGS_OTHER   ... the other compiler flags
#   COMPIZCONFIG_VERSION        ... version of the module
#   COMPIZCONFIG_PREFIX         ... prefix-directory of the module
#   COMPIZCONFIG_INCLUDEDIR     ... include-dir of the module
#   COMPIZCONFIG_LIBDIR         ... lib-dir of the module
#
# If the _COMPIZCONFIG_INTERNAL variable is set to true, then this module will do
# nothing. This is required for the Compiz core package build system.
#
#  Author: Dennis Kasprzyk <onestone@compiz.org>
#
################################################################################

if (NOT _COMPIZCONFIG_INTERNAL)

    if (CompizConfig_FIND_REQUIRED)
	set (_req REQUIRED)
    endif ()

    # look for pkg-config
    find_package (PkgConfig ${_req})

    if (PKG_CONFIG_FOUND)

	# do we need to look for a specified version?
	set (_comp_ver)
	if (CompizConfig_FIND_VERSION)
	    if (CompizConfig_FIND_VERSION_EXACT)
		set (_comp_ver "=${CompizConfig_FIND_VERSION}")
	    else ()
		set (_comp_ver ">=${CompizConfig_FIND_VERSION}")
	    endif ()
	endif ()

	# add install prefix to pkgconfig search path if needed
        string (REGEX REPLACE "([\\+\\(\\)\\^\\\$\\.\\-\\*\\?\\|])" "\\\\\\1" PKGCONFIG_REGEX ${CMAKE_INSTALL_PREFIX})
	set (PKGCONFIG_REGEX ".*${PKGCONFIG_REGEX}/lib/pkgconfig:${PKGCONFIG_REGEX}/share/pkgconfig.*")

	if (NOT "$ENV{PKG_CONFIG_PATH}" MATCHES "${PKGCONFIG_REGEX}")
	    if ("" STREQUAL "$ENV{PKG_CONFIG_PATH}")
		set (ENV{PKG_CONFIG_PATH} "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig:${CMAKE_INSTALL_PREFIX}/share/pkgconfig")
	    else ()
		set (ENV{PKG_CONFIG_PATH}
		    "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig:${CMAKE_INSTALL_PREFIX}/share/pkgconfig:$ENV{PKG_CONFIG_PATH}")
	    endif ()
	endif ()

	# look for compiz
	pkg_check_modules (COMPIZCONFIG ${_req} "libcompizconfig")

	# is the LibCompizConfigCommon module installed?
	find_path(_compiz_def_macro LibCompizConfigCommon.cmake ${COMPIZCONFIG_PREFIX}/share/compiz/cmake)

	if (COMPIZCONFIG_FOUND AND _compiz_def_macro)
            # everything found. Set module path and include defaults module
	    set (CMAKE_MODULE_PATH  ${CMAKE_MODULE_PATH} ${COMPIZCONFIG_PREFIX}/share/compiz/cmake)
	    include (LibCompizConfigCommon)
	else ()
	    set (COMPIZCONFIG_FOUND 0)
	endif ()

        if (Compiz_FIND_REQUIRED AND NOT COMPIZCONFIG_FOUND)
	    message (FATAL_ERROR "Unable to find CompizConfig ${_comp_ver}")
	endif ()
    endif ()
endif ()

