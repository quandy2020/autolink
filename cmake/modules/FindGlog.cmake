# Copyright 2024 The OpenRobotic Beginner Authors (duyongquan)
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Prefer a matching include/lib pair. Mixing /usr/local headers (glog 0.6)
# with apt libglog.so.0 (0.4) causes undefined references at link time.

# If install_glog.sh placed a Config package under /usr/local, force it —
# a cached glog_DIR pointing at apt otherwise wins and breaks the ABI.
foreach(_glog_cfg_prefix IN ITEMS
    "/usr/local"
    "/opt/homebrew"
    "$ENV{HOME}/.local")
  if(EXISTS "${_glog_cfg_prefix}/lib/cmake/glog/glog-config.cmake")
    set(glog_DIR "${_glog_cfg_prefix}/lib/cmake/glog" CACHE PATH
      "Directory containing glog-config.cmake" FORCE)
    break()
  endif()
endforeach()

find_package(glog CONFIG QUIET)
if(glog_FOUND)
  if(TARGET glog::glog)
    get_target_property(_glog_inc glog::glog INTERFACE_INCLUDE_DIRECTORIES)
    if(_glog_inc)
      set(GLOG_INCLUDE_DIR "${_glog_inc}")
      set(GLOG_INCLUDE_DIRS "${_glog_inc}")
    endif()
    # Also expose the concrete .so path for consumers that use GLOG_LIBRARY.
    get_target_property(_glog_loc glog::glog IMPORTED_LOCATION_RELEASE)
    if(NOT _glog_loc)
      get_target_property(_glog_loc glog::glog IMPORTED_LOCATION)
    endif()
    if(_glog_loc)
      set(GLOG_LIBRARY "${_glog_loc}" CACHE FILEPATH "glog library" FORCE)
    endif()
    set(GLOG_LIBRARIES glog::glog)
  endif()
  set(GLOG_FOUND TRUE)
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Glog DEFAULT_MSG GLOG_LIBRARIES)
  mark_as_advanced(GLOG_INCLUDE_DIR GLOG_LIBRARY)
  return()
endif()

# Module fallback: search /usr/local before system multiarch.
set(_glog_search_paths
  "$ENV{HOME}/.local"
  /opt/homebrew
  /usr/local
)

find_path(GLOG_INCLUDE_DIR
  NAMES glog/logging.h
  HINTS ${CMAKE_PREFIX_PATH} $ENV{CMAKE_PREFIX_PATH}
  PATHS ${_glog_search_paths}
  PATH_SUFFIXES include
)

find_library(GLOG_LIBRARY
  NAMES glog
  HINTS ${CMAKE_PREFIX_PATH} $ENV{CMAKE_PREFIX_PATH}
  PATHS ${_glog_search_paths}
  PATH_SUFFIXES lib lib64
)

# If headers came from /usr/local, force the matching library (avoid apt).
if(GLOG_INCLUDE_DIR MATCHES "/usr/local")
  find_library(GLOG_LIBRARY
    NAMES glog
    PATHS /usr/local
    PATH_SUFFIXES lib lib64
    NO_DEFAULT_PATH
    NO_CMAKE_SYSTEM_PATH)
endif()

if(NOT GLOG_INCLUDE_DIR OR NOT GLOG_LIBRARY)
  find_path(GLOG_INCLUDE_DIR NAMES glog/logging.h)
  find_library(GLOG_LIBRARY NAMES glog)
endif()

if(GLOG_INCLUDE_DIR AND GLOG_LIBRARY)
  set(GLOG_FOUND TRUE)
  set(GLOG_INCLUDE_DIRS ${GLOG_INCLUDE_DIR})
  set(GLOG_LIBRARIES ${GLOG_LIBRARY})
else()
  set(GLOG_FOUND FALSE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Glog DEFAULT_MSG GLOG_INCLUDE_DIR GLOG_LIBRARY)

mark_as_advanced(GLOG_INCLUDE_DIR GLOG_LIBRARY)
