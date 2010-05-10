find_package (Compiz REQUIRED)

include (CompizCommon)
include (CompizPackage)

compiz_add_plugins_in_folder (${CMAKE_CURRENT_SOURCE_DIR})
