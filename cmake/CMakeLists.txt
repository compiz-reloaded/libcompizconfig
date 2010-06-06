set (_files
    LibCompizConfigCommon.cmake
)

install (
    FILES ${_files}
    DESTINATION ${COMPIZ_DESTDIR}${CMAKE_INSTALL_PREFIX}/share/compiz/cmake
)
