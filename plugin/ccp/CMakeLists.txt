find_package (Compiz REQUIRED)

include (CompizPlugin)
include (CompizCommon)

set (COMPIZ_PLUGIN_INSTALL_TYPE
     "compiz")

compiz_plugin (ccp LIBRARIES compizconfig)

if (COMPIZ_BUILD_WITH_RPATH)
    set_target_properties (
	ccp PROPERTIES
	INSTALL_RPATH "${COMPIZCONFIG_LIBDIR}"
    )
endif (COMPIZ_BUILD_WITH_RPATH)
