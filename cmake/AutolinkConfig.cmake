# find_package(Autolink) shim. The package config lives next to the
# lowercase autocmake export.
get_filename_component(_Autolink_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/../autolink/autolink-config.cmake")
set(Autolink_FOUND TRUE)
set(Autolink_INCLUDE_DIRS "${_Autolink_prefix}/include")
set(Autolink_LIBRARIES autolink::autolink)
unset(_Autolink_prefix)
