# Copyright 2025 The Openbot Authors (duyongquan)
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

# Package entry. autolink/ builds the library. Optional directories install themselves.

cmake_minimum_required(VERSION 3.20)

project(autolink VERSION 0.1.0 LANGUAGES CXX)

# Installed autocmake, or the sibling source tree.
find_package(autocmake CONFIG QUIET)
if(NOT autocmake_FOUND)
  set(AUTOCMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../autocmake")
  include("${AUTOCMAKE_DIR}/Autocmake.cmake")
endif()

option(AUTOLINK_BUILD_TOOLS "Build autolink tools" ON)
option(AUTOLINK_BUILD_TEST "Build unit tests" ON)
option(AUTOLINK_BUILD_EXAMPLES "Build examples" ON)
option(AUTOLINK_BUILD_PYTHON "Build Python bindings" ON)
option(AUTOLINK_BUILD_DOCS "Build documentation" ON)
option(AUTOLINK_ENABLE_FASTDDS "Build RTPS transport with Fast DDS 3.x" OFF)

# autocmake_test follows BUILD_TESTING. The C++ tests here use GMock
# directly; this switch still has to be off before autocmake_build().
if(DEFINED AUTOLINK_BUILD_TEST AND NOT AUTOLINK_BUILD_TEST)
  set(BUILD_TESTING OFF CACHE BOOL "Build tests" FORCE)
endif()

# FindGlog and FindTinyXML2 must be visible before package.xml is resolved.
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

autocmake_project(EXTRAS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AutolinkCompat.cmake")
autocmake_build(QUIT)

# Keep historical layout: executables in bin/, libraries in lib/.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(AUTOLINK_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(AUTOLINK_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

# Sets AUTOLINK_HAS_FASTDDS. Linking is applied on the library target.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FastDDS.cmake")

add_subdirectory(autolink)
set(_optional "")
if(AUTOLINK_BUILD_PYTHON)
  list(APPEND _optional autolink/python)
endif()
if(AUTOLINK_BUILD_EXAMPLES)
  list(APPEND _optional examples)
endif()
if(AUTOLINK_BUILD_DOCS)
  list(APPEND _optional docs)
endif()
foreach(_dir ${_optional})
  add_subdirectory(${_dir})
endforeach()

# In-tree alias. The installed config creates the same name from
# autolink::autolink via cmake/AutolinkCompat.cmake.
if(NOT TARGET Autolink::Autolink)
  add_library(Autolink::Autolink ALIAS autolink)
endif()

# The autonomy superproject exports this library on AutonomyExport.
# A target can belong to that set in addition to the autocmake export.
if(CMAKE_PROJECT_NAME STREQUAL "autonomy")
  install(TARGETS autolink
    EXPORT AutonomyExport
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# find_package(Autolink) looks beside lib/cmake/Autolink, not lib/cmake/autolink.
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AutolinkConfig.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Autolink")

autocmake_package()
