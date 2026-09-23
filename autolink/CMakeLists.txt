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

set(AUTOLINK_DEFAULT_CONF_DIR "${AUTOLINK_ROOT_DIR}/autolink/conf")
set(AUTOLINK_DEFAULT_PLUGIN_INDEX_DIR
  "${CMAKE_INSTALL_PREFIX}/share/autolink_plugin_index")
file(MAKE_DIRECTORY "${AUTOLINK_BINARY_DIR}/autolink/conf")
configure_file(conf/conf.hpp.in "${AUTOLINK_BINARY_DIR}/autolink/conf/conf.hpp" @ONLY)

# Tests, tools, mainboard, examples, and Fast DDS sources stay out of the core list.
file(GLOB_RECURSE AUTOLINK_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(FILTER AUTOLINK_CORE_SOURCES EXCLUDE REGEX
  "(_test\\.cpp$|/(tools|mainboard|example|integration_test|python)/|/transport/rtps/|rtps_dispatcher\\.cpp$|rtps_topology_backend\\.cpp$)")

autocmake_library(autolink SHARED SOURCES ${AUTOLINK_CORE_SOURCES})

# Layout root keeps includes as "autolink/proto/*.pb.h".
# The proto directory is the search path for import "qos_profile.proto".
file(GLOB AUTOLINK_PROTO_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto")
autocmake_protobuf(autolink
  SOURCES ${AUTOLINK_PROTO_FILES}
  IMPORTS "${AUTOLINK_ROOT_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/proto")

# Staged next to the protoc rule. autolink/python/ cannot depend on that output.
if(AUTOLINK_BUILD_PYTHON)
  set(_py "${CMAKE_BINARY_DIR}/python/autolink")
  set(_pb2 "${CMAKE_CURRENT_BINARY_DIR}/autolink/proto")
  set(_staged
    "${_py}/proto/unit_test_pb2.py"
    "${_py}/proto/action_pb2.py"
    "${_py}/proto/parameter_pb2.py"
    "${_py}/examples/examples_pb2.py")
  add_custom_command(
    OUTPUT ${_staged}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_py}/proto" "${_py}/examples"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${_pb2}/unit_test_pb2.py" "${_pb2}/action_pb2.py" "${_pb2}/parameter_pb2.py"
      "${_py}/proto"
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
      --proto_path=${AUTOLINK_ROOT_DIR}/examples/cpp/proto
      --python_out=${_py}/examples
      ${AUTOLINK_ROOT_DIR}/examples/cpp/proto/examples.proto
    COMMAND ${CMAKE_COMMAND} -E touch "${_py}/proto/__init__.py" "${_py}/examples/__init__.py"
    DEPENDS
      "${_pb2}/unit_test_pb2.py" "${_pb2}/action_pb2.py" "${_pb2}/parameter_pb2.py"
      "${AUTOLINK_ROOT_DIR}/examples/cpp/proto/examples.proto"
    COMMENT "Staging Python protobuf (*_pb2.py)"
    VERBATIM)
  add_custom_target(autolink_python_pb2 ALL DEPENDS ${_staged})

  # Writer/Reader templates belong in libautolink. Putting them in the
  # pybind module duplicated template statics and crashed Init.
  file(GLOB _bridges CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/python/bridge/*.cpp")
  target_sources(autolink PRIVATE ${_bridges})
endif()

# security_config has no Fast DDS dependency.
if(AUTOLINK_HAS_FASTDDS)
  file(GLOB_RECURSE _rtps CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/transport/rtps/*.cpp")
  list(FILTER _rtps EXCLUDE REGEX "_test\\.cpp$")
  target_sources(autolink PRIVATE
    ${_rtps}
    "${CMAKE_CURRENT_SOURCE_DIR}/transport/dispatcher/rtps_dispatcher.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/service_discovery/rtps_topology_backend.cpp")
else()
  target_sources(autolink PRIVATE transport/rtps/security_config.cpp)
endif()

enable_language(ASM)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
  set(_swap swap_aarch64.S)
else()
  set(_swap swap_x86_64.S)
endif()
target_sources(autolink PRIVATE "croutine/detail/${_swap}")

target_include_directories(autolink PUBLIC
  $<BUILD_INTERFACE:${AUTOLINK_ROOT_DIR}>
  $<BUILD_INTERFACE:${AUTOLINK_BINARY_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

autocmake_link(autolink Threads TinyXML2)
if(TARGET glog::glog)
  target_link_libraries(autolink PUBLIC glog::glog)
else()
  target_link_libraries(autolink PUBLIC ${GLOG_LIBRARIES})
  target_include_directories(autolink PUBLIC ${GLOG_INCLUDE_DIRS})
endif()
target_link_libraries(autolink PUBLIC atomic dl)
autolink_apply_fastdds(autolink)

autocmake_binary(autolink_mainboard
  SOURCES mainboard/mainboard.cpp mainboard/module_argument.cpp mainboard/module_controller.cpp
  DEPENDENCIES autolink)
set_target_properties(autolink_mainboard PROPERTIES OUTPUT_NAME mainboard)

if(AUTOLINK_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

# Headers sit beside sources. autocmake_protobuf installs the generated ones.
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/autolink"
  FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
install(FILES "${AUTOLINK_BINARY_DIR}/autolink/conf/conf.hpp"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/autolink/conf")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/conf/"
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/autolink/conf"
  FILES_MATCHING PATTERN "*.conf")

# Each test brings its own main. file_test.cpp also links gmock.
if(AUTOLINK_BUILD_TEST)
  find_package(GMock QUIET)
  set(_deps autolink)
  if(TARGET GTest::gmock)
    list(APPEND _deps GTest::gmock)
  elseif(GMOCK_LIBRARIES)
    list(APPEND _deps ${GMOCK_LIBRARIES})
  endif()

  file(GLOB_RECURSE _tests CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp")
  list(FILTER _tests EXCLUDE REGEX "/integration_test/")
  list(APPEND _tests
    "${CMAKE_CURRENT_SOURCE_DIR}/transport/integration_test/rtps_transceiver_test.cpp")
  foreach(_src ${_tests})
    file(RELATIVE_PATH _rel "${CMAKE_CURRENT_SOURCE_DIR}" "${_src}")
    cmake_path(REMOVE_EXTENSION _rel OUTPUT_VARIABLE _name)
    string(REPLACE "/" "." _name "autolink.${_name}")
    autocmake_test(${_name} SOURCES ${_src} DEPENDENCIES ${_deps} NO_MAIN)
    if(NOT TARGET ${_name})
      continue()
    endif()
    if(UNIX AND NOT APPLE)
      target_link_libraries(${_name} PRIVATE atomic)
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options(${_name} PRIVATE -Wno-error=maybe-uninitialized)
    endif()
  endforeach()
endif()
