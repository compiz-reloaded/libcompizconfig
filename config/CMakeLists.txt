set (_config_file
     config)

compiz_opt_install_file (${_config_file} ${COMPIZ_SYSCONFDIR}/compizconfig/${_config_file})

add_custom_target (
    config_install
    ${CMAKE_COMMAND} -E make_directory ${COMPIZ_DESTDIR}${CMAKE_ROOT}/Modules &&
    ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/config/${_config_file} ${COMPIZ_DESTDIR}${COMPIZ_SYSCONFDIR}/compizconfig/${_config_file}
    )
