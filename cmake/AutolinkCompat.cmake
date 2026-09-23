# Build-tree and installed consumers of find_package(Autolink) expect
# Autolink::Autolink. The autocmake export name is autolink::autolink.
if(TARGET autolink::autolink AND NOT TARGET Autolink::Autolink)
  add_library(Autolink::Autolink ALIAS autolink::autolink)
endif()

if(DEFINED PACKAGE_PREFIX_DIR)
  set(Autolink_INCLUDE_DIRS "${PACKAGE_PREFIX_DIR}/include")
endif()
set(Autolink_LIBRARIES autolink::autolink)
set(Autolink_FOUND TRUE)
