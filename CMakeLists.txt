project (libcompizconfig)

find_package (Compiz REQUIRED)

include (CompizCommon)
include (CompizPackage)
include (CheckLibraryExists)
include (CheckIncludeFile)

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

# libcompizconfig package version number
# An odd micro number indicates in-progress development.
# An even micro number indicates a released version.
set (LIBCOMPIZCONFIG_VERSION_MAJOR 0)
set (LIBCOMPIZCONFIG_VERSION_MINOR 9)
set (LIBCOMPIZCONFIG_VERSION_MICRO 4)
set (LIBCOMPIZCONFIG_VERSION_MACRO 0)
set (VERSION ${LIBCOMPIZCONFIG_VERSION_MAJOR}.${LIBCOMPIZCONFIG_VERSION_MINOR}.${LIBCOMPIZCONFIG_VERSION_MICRO}.${LIBCOMPIZCONFIG_VERSION_MACRO})

set (_libcompizconfig_package_string "LibCompizConfig ${VERSION}")
set (_libcompizconfig_package "LibCompizCompiz")

set (ALL_LINGUAS af ar bg bn bn_IN bs ca cs cy da de el en_GB en_US es eu et fi fr gl gu he hi hr hu id it ja ka km ko lo lt mk mr nb nl or pa pl pt pt_BR ro ru sk sl sr sv ta tr uk vi xh zh_CN zh_TW zu)
set (GETTEXT_PACKAGE libcompizconfig)

configure_file (
    "${CMAKE_SOURCE_DIR}/config.h.in"
    "${CMAKE_BINARY_DIR}/config.h"
    )

include_directories (
    "include"
    )

if (COMPIZ_PACKAGING_ENABLED)
    set (libdir ${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}         CACHE PATH "libdir")
    set (includedir ${CMAKE_INSTALL_PREFIX}/include/compizconfig CACHE PATH "includedir")
    set (prefix ${CMAKE_INSTALL_PREFIX}                          CACHE PATH "prefix")
    set (exec_prefix ${CMAKE_INSTALL_PREFIX}/bin                 CACHE PATH "bindir")
else (COMPIZ_PACKAGING_ENABLED)
    set (libdir ${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX})
    set (includedir ${CMAKE_INSTALL_PREFIX}/include/compizconfig)
    set (prefix ${CMAKE_INSTALL_PREFIX})
    set (exec_prefix ${CMAKE_INSTALL_PREFIX}/bin)
endif (COMPIZ_PACKAGING_ENABLED)

set (COMPIZ_SYSCONFDIR "/etc" CACHE PATH "/etc")

option (USE_PROTOBUF
	"Use Google Protocol Buffers library for faster XML loading"
	ON
	)

set (PROTOC_ERROR_MESSAGE
     "\nWARNING: You need to install Protocol Buffers to get faster program startup.\n"
     "Sources are available at http://code.google.com/p/protobuf/downloads/list\n"
     "Package names for Ubuntu/Debian: libprotobuf0, libprotobuf-dev, protobuf-compiler\n"
     "Package names for other distributions: protobuf, protobuf-devel\n"
     "Disabling protobuf.\n"
     )

set (LIBCOMPIZCONFIG_PROTOBUF
     protobuf)

if (USE_PROTOBUF)
    find_program (HAVE_PROTOC protoc)
    if (HAVE_PROTOC)
	if (USE_PROTOBUF)
	    pkg_check_modules (PROTOBUF protobuf)
	    if (PROTOBUF_FOUND)
	    else ()
		message (${PROTOC_ERROR_MESSAGE})
		set (USE_PROTOBUF
		     false
		     )
	    endif (PROTOBUF_FOUND)
	endif (USE_PROTOBUF)
    else ()
	message (${PROTOC_ERROR_MESSAGE})
	set (USE_PROTOBUF
	     false
	     )
    endif (HAVE_PROTOC)
endif (USE_PROTOBUF)

if (USE_PROTOBUF)
    set (HAVE_PROTOBUF TRUE)
endif (USE_PROTOBUF)

check_include_file ("sys/inotify.h"
		    HAVE_INOTIFY)

set (INOTIFY_WARNING_MESSAGE
     "NOTE\n
      ======\n
      Inotify header files not found. While this library might work \n
      without them, user experience is degraded as settings won't be\n
      applied instantly. We strongly suggest installing the inotify\n
      header files.\n")

if (HAVE_INOTIFY)
else ()
    message (${INOTIFY_WARNING_MESSAGE})
endif (HAVE_INOTIFY)

set (COMPIZCONFIG_LIBDIR ${libdir})

set (LIBCOMPIZCONFIG_REQUIRES
    x11
    compiz
    libxml-2.0
    libxslt
    )

if (HAVE_PROTOBUF)
    set (LIBCOMPIZCONFIG_REQUIRES
	 ${LIBCOMPIZCONFIG_REQUIRES}
	 protobuf)
endif ()

compiz_pkg_check_modules (LIBCOMPIZCONFIG REQUIRED ${LIBCOMPIZCONFIG_REQUIRES})

compiz_configure_file (
    ${CMAKE_CURRENT_SOURCE_DIR}/libcompizconfig.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/libcompizconfig.pc
)

install (
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libcompizconfig.pc
    DESTINATION ${COMPIZ_DESTDIR}${libdir}/pkgconfig
)


add_subdirectory (config)
add_subdirectory (backend)
add_subdirectory (plugin)
add_subdirectory (src)
add_subdirectory (include)
add_subdirectory (cmake)

compiz_print_configure_header ("CompizConfig Library")
compiz_color_message ("\n${_escape}[4mOptional features:${_escape}[0m\n")

compiz_print_result_message ("protocol buffers" HAVE_PROTOBUF)
compiz_print_result_message ("file system change notifications" HAVE_INOTIFY)
compiz_print_plugin_stats ("${CMAKE_SOURCE_DIR}/plugin")
compiz_print_configure_footer ()

find_file (_find_compizconfig FindCompizConfig.cmake PATHS ${CMAKE_ROOT}/Modules ${ARGN})
if (NOT _find_compizconfig)
    compiz_color_message ("${_escape}[1;31mWARNING:${_escape}[0m")
    message ("\"FindCompiz.cmake\" file not found in cmake module directories.")
    message ("It should be installed to allow building of external compiz packages.")
    message ("Call \"sudo make findcompiz_install\" to install it.\n")
    compiz_print_configure_footer ()
endif (NOT _find_compizconfig)
    add_custom_target (
        findcompizconfig_install
	${CMAKE_COMMAND} -E make_directory ${COMPIZ_DESTDIR}${CMAKE_ROOT}/Modules &&
	${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/cmake/FindCompizConfig.cmake ${COMPIZ_DESTDIR}${CMAKE_ROOT}/Modules
    )

macro (add_uninstall)
	compiz_set(_compiz_uninstall_rule_created TRUE)

	set (_file "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake")

	file (WRITE  ${_file} "if (NOT EXISTS \"${CMAKE_BINARY_DIR}/install_manifest.txt\")\n")
	file (APPEND ${_file} "  message (FATAL_ERROR \"Cannot find install manifest: \\\"${CMAKE_BINARY_DIR}/install_manifest.txt\\\"\")\n")
	file (APPEND ${_file} "endif (NOT EXISTS \"${CMAKE_BINARY_DIR}/install_manifest.txt\")\n\n")
	file (APPEND ${_file} "file (READ \"${CMAKE_BINARY_DIR}/install_manifest.txt\" files)\n")
	file (APPEND ${_file} "string (REGEX REPLACE \"\\n\" \";\" files \"\${files}\")\n")
	file (APPEND ${_file} "foreach (file \${files})\n")
	file (APPEND ${_file} "  message (STATUS \"Uninstalling \\\"\${file}\\\"\")\n")
	file (APPEND ${_file} "  if (EXISTS \"\${file}\")\n")
	file (APPEND ${_file} "    exec_program(\n")
	file (APPEND ${_file} "      \"${CMAKE_COMMAND}\" ARGS \"-E remove \\\"\${file}\\\"\"\n")
	file (APPEND ${_file} "      OUTPUT_VARIABLE rm_out\n")
	file (APPEND ${_file} "      RETURN_VALUE rm_retval\n")
	file (APPEND ${_file} "      )\n")
	file (APPEND ${_file} "    if (\"\${rm_retval}\" STREQUAL 0)\n")
	file (APPEND ${_file} "    else (\"\${rm_retval}\" STREQUAL 0)\n")
	file (APPEND ${_file} "      message (FATAL_ERROR \"Problem when removing \\\"\${file}\\\"\")\n")
	file (APPEND ${_file} "    endif (\"\${rm_retval}\" STREQUAL 0)\n")
	file (APPEND ${_file} "  else (EXISTS \"\${file}\")\n")
	file (APPEND ${_file} "    message (STATUS \"File \\\"\${file}\\\" does not exist.\")\n")
	file (APPEND ${_file} "  endif (EXISTS \"\${file}\")\n")
	file (APPEND ${_file} "endforeach (file)\n")

	add_custom_target(uninstall "${CMAKE_COMMAND}" -P "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake")
endmacro ()

compiz_ensure_linkage ()
compiz_package_generation ("CompizConfig Library")
add_uninstall ()
compiz_add_git_dist ()
