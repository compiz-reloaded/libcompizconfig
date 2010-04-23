project (libcompizconfig)

find_package (Compiz REQUIRED)

include (CompizCommon)
include (CheckLibraryExists)
include (CheckIncludeFile)

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

# libcompizconfig package version number
# An odd micro number indicates in-progress development.
# An even micro number indicates a released version.
set (LIBCOMPIZCONFIG_VERSION_MAJOR 0)
set (LIBCOMPIZCONFIG_VERSION_MINOR 9)
set (LIBCOMPIZCONFIG_VERSION_MICRO 0)
set (VERSION ${LIBCOMPIZCONFIG_VERSION_MAJOR}.${LIBCOMPIZCONFIG_VERSION_MINOR}.${LIBCOMPIZCONFIG_VERSION_MICRO})

set (_libcompizconfig_package_string "LibCompizConfig ${VERSION}")
set (_libcompizconfig_package "LibCompizCompiz")

configure_file (
    "${CMAKE_SOURCE_DIR}/config.h.in"
    "${CMAKE_BINARY_DIR}/config.h"
    )

include_directories (
    "include"
    )


set (libdir ${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX})
set (includedir ${CMAKE_INSTALL_PREFIX}/include/compizconfig)
set (prefix ${CMAKE_INSTALL_PREFIX})
set (exec_prefix ${CMAKE_INSTALL_PREFIX})

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
    DESTINATION ${libdir}/pkgconfig
)

find_file (_find_compiz FindCompizConfig.cmake PATHS ${CMAKE_ROOT}/Modules ${ARGN})
if (NOT _find_compiz)
    message ("\"FindCompizConfig.cmake\" file not found in cmake module directories.")
    message ("It should be installed to allow building of external compiz packages.")
    message ("Call \"sudo make findcompizconfig_install\" to install it.\n")
    endif ()
    add_custom_target (
        findcompizconfig_install
	${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/cmake/FindCompizConfig.cmake ${CMAKE_ROOT}/Modules
    )

add_subdirectory (backend)
add_subdirectory (plugin)
add_subdirectory (src)
add_subdirectory (include)
add_subdirectory (cmake)
#add_subdirectory (metadata)
#add_subdirectory (po)
