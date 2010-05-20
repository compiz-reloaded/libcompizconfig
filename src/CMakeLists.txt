include_directories (
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBCOMPIZCONFIG_INCLUDE_DIRS}
)

set (LIBCOMPIZCONFIG_FILES
    bindings.c
    ccs-private.h
    compiz.cpp
    config.c
    filewatch.c
    ini.c
    iniparser.c
    iniparser.h
    lists.c
    main.c
)

add_definitions (
    -DHAVE_CONFIG_H
    -DPLUGINDIR=\\\"${COMPIZ_PLUGINDIR}\\\"
    -DIMAGEDIR=\\\"${COMPIZ_IMAGEDIR}\\\"
    -DMETADATADIR=\\\"${COMPIZ_METADATADIR}\\\"
    -DSYSCONFDIR=\\\"${COMPIZ_SYSCONFDIR}\\\"
    -DLIBDIR=\\\"${COMPIZ_LIBDIR}\\\"
)

if (HAVE_PROTOBUF)
    exec_program (${HAVE_PROTOC} ${CMAKE_SOURCE_DIR}/src
		  ARGS "--cpp_out='.' compizconfig.proto")
    set (LIBCOMPIZCONFIG_FILES
	 ${LIBCOMPIZCONFIG_FILES}
	 compizconfig.pb.cc
	)
    add_definitions (
	    -DUSE_PROTOBUF
    )
endif (HAVE_PROTOBUF)

if (HAVE_INOTIFY)
    add_definitions (
	    -DHAVE_SYS_INOTIFY_H
    )
endif (HAVE_INOTIFY)

add_library (
    compizconfig SHARED
    ${LIBCOMPIZCONFIG_FILES}
)

link_directories (
    ${LIBCOMPIZCONFIG_LINK_DIRS}
)

target_link_libraries (
    compizconfig ${LIBCOMPIZCONFIG_LIBRARIES} m
)

set (COMPIZCONFIG_LIBS "")
foreach (_val ${LIBCOMPIZCONFIG_LDFLAGS})
    set (COMPIZCONFIG_LIBS "${COMPIZCONFIG_LIBS}${_val} ")
endforeach (_val ${LIBCOMPIZCONFIG_LDFLAGS})

set (COMPIZCONFIG_CFLAGS "")
foreach (_val ${LIBCOMPIZCONFIG_CFLAGS})
    set (COMPIZCONFIG_CFLAGS "${COMPIZCONFIG_CFLAGS}${_val} ")
endforeach (_val ${LIBCOMPIZCONFIG_CFLAGS})

install (
    TARGETS compizconfig
    DESTINATION ${COMPIZ_DESTDIR}${libdir}
)
