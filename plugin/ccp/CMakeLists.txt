find_package (Compiz REQUIRED)

include (CompizPlugin)

set (COMPIZ_PLUGIN_INSTALL_TYPE
     "compiz")

compiz_plugin (ccp LIBRARIES compizconfig)

set_target_properties (
    ccp PROPERTIES
    INSTALL_RPATH "${COMPIZCONFIG_LIBDIR}"
)
