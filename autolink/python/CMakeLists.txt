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

cmake_minimum_required(VERSION 3.15)
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

set(PYBIND11_FINDPYTHON ON)
add_subdirectory(
  "${AUTOLINK_ROOT_DIR}/thirdparty/pybind11"
  "${CMAKE_BINARY_DIR}/thirdparty/pybind11"
  EXCLUDE_FROM_ALL)

set(AUTOLINK_PY_PKG_DIR "${CMAKE_BINARY_DIR}/python/autolink")
file(MAKE_DIRECTORY "${AUTOLINK_PY_PKG_DIR}")

set(AUTOLINK_PY_BIND_SOURCES
  bindings/bind_lifecycle.cpp
  bindings/bind_node.cpp
  bindings/bind_action.cpp
  bindings/bind_parameter.cpp
  bindings/bind_record.cpp
  bindings/bind_time.cpp
  bindings/bind_timer.cpp
  bindings/bind_utils.cpp
  bindings/module.cpp
)

pybind11_add_module(_core ${AUTOLINK_PY_BIND_SOURCES})
target_include_directories(_core PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/bindings"
  "${AUTOLINK_ROOT_DIR}")
target_link_libraries(_core PRIVATE autolink)
set_target_properties(_core PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${AUTOLINK_PY_PKG_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${AUTOLINK_PY_PKG_DIR}")

# Generate core Python protobuf stubs into the build PYTHONPATH tree.
# File layout follows package import paths (not protoc's default relative-to-proto_path).
set(AUTOLINK_PY_OUT "${CMAKE_BINARY_DIR}/python")
set(AUTOLINK_PY_PB2_UNIT "${AUTOLINK_PY_OUT}/autolink/proto/unit_test_pb2.py")
set(AUTOLINK_PY_PB2_ACTION "${AUTOLINK_PY_OUT}/autolink/proto/action_pb2.py")
set(AUTOLINK_PY_PB2_PARAM "${AUTOLINK_PY_OUT}/autolink/proto/parameter_pb2.py")
set(AUTOLINK_PY_PB2_EXAMPLES "${AUTOLINK_PY_OUT}/autolink/examples/examples_pb2.py")
add_custom_command(
  OUTPUT
    "${AUTOLINK_PY_PB2_UNIT}"
    "${AUTOLINK_PY_PB2_ACTION}"
    "${AUTOLINK_PY_PB2_PARAM}"
    "${AUTOLINK_PY_PB2_EXAMPLES}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${AUTOLINK_PY_OUT}/autolink/proto"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${AUTOLINK_PY_OUT}/autolink/examples"
  COMMAND ${Protobuf_PROTOC_EXECUTABLE}
          --proto_path=${AUTOLINK_ROOT_DIR}/autolink/proto
          --python_out=${AUTOLINK_PY_OUT}/autolink/proto
          ${AUTOLINK_ROOT_DIR}/autolink/proto/unit_test.proto
          ${AUTOLINK_ROOT_DIR}/autolink/proto/action.proto
          ${AUTOLINK_ROOT_DIR}/autolink/proto/parameter.proto
  COMMAND ${Protobuf_PROTOC_EXECUTABLE}
          --proto_path=${AUTOLINK_ROOT_DIR}/examples/cpp/proto
          --python_out=${AUTOLINK_PY_OUT}/autolink/examples
          ${AUTOLINK_ROOT_DIR}/examples/cpp/proto/examples.proto
  COMMAND ${CMAKE_COMMAND} -E touch
          "${AUTOLINK_PY_OUT}/autolink/proto/__init__.py"
          "${AUTOLINK_PY_OUT}/autolink/examples/__init__.py"
  DEPENDS
    "${AUTOLINK_ROOT_DIR}/autolink/proto/unit_test.proto"
    "${AUTOLINK_ROOT_DIR}/autolink/proto/action.proto"
    "${AUTOLINK_ROOT_DIR}/autolink/proto/parameter.proto"
    "${AUTOLINK_ROOT_DIR}/examples/cpp/proto/examples.proto"
  COMMENT "Generating Python protobuf (*_pb2.py)"
  VERBATIM)
add_custom_target(autolink_python_pb2 ALL DEPENDS
  "${AUTOLINK_PY_PB2_UNIT}"
  "${AUTOLINK_PY_PB2_ACTION}"
  "${AUTOLINK_PY_PB2_PARAM}"
  "${AUTOLINK_PY_PB2_EXAMPLES}")
add_dependencies(_core autolink_python_pb2)
install(DIRECTORY "${AUTOLINK_PY_OUT}/autolink/proto"
  DESTINATION "python/autolink"
  FILES_MATCHING PATTERN "*.py")
install(DIRECTORY "${AUTOLINK_PY_OUT}/autolink/examples"
  DESTINATION "python/autolink"
  FILES_MATCHING PATTERN "*.py")

file(GLOB AUTOLINK_PY_FILES "${CMAKE_CURRENT_SOURCE_DIR}/autolink/*.py")
foreach(PY ${AUTOLINK_PY_FILES})
  get_filename_component(NAME "${PY}" NAME)
  configure_file("${PY}" "${AUTOLINK_PY_PKG_DIR}/${NAME}" COPYONLY)
endforeach()

install(TARGETS _core
  LIBRARY DESTINATION "python/autolink"
  RUNTIME DESTINATION "python/autolink")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/autolink/"
  DESTINATION "python/autolink"
  FILES_MATCHING PATTERN "*.py")

if(AUTOLINK_BUILD_TEST)
  # Prefer pytest when available; fall back to running the script directly.
  execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import pytest"
    RESULT_VARIABLE AUTOLINK_PYTEST_OK
    OUTPUT_QUIET ERROR_QUIET)
  if(AUTOLINK_PYTEST_OK EQUAL 0)
    add_test(NAME autolink_python_smoke_import
      COMMAND ${Python3_EXECUTABLE} -m pytest
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_smoke_import.py" -q)
    add_test(NAME autolink_python_time
      COMMAND ${Python3_EXECUTABLE} -m pytest
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_time.py" -q)
    add_test(NAME autolink_python_pubsub
      COMMAND ${Python3_EXECUTABLE} -m pytest
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pubsub.py" -q)
    add_test(NAME autolink_python_service
      COMMAND ${Python3_EXECUTABLE} -m pytest
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_service.py" -q)
    add_test(NAME autolink_python_parameter
      COMMAND ${Python3_EXECUTABLE} -m pytest
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_parameter.py" -q)
    add_test(NAME autolink_python_record
      COMMAND ${Python3_EXECUTABLE} -m pytest
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_record.py" -q)
    add_test(NAME autolink_python_protobuf_pubsub
      COMMAND ${Python3_EXECUTABLE} -m pytest
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_protobuf_pubsub.py" -q)
    add_test(NAME autolink_python_protobuf_service
      COMMAND ${Python3_EXECUTABLE} -m pytest
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_protobuf_service.py" -q)
    add_test(NAME autolink_python_action
      COMMAND ${Python3_EXECUTABLE} -m pytest
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_action.py" -q)
  else()
    add_test(NAME autolink_python_smoke_import
      COMMAND ${Python3_EXECUTABLE}
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_smoke_import.py")
    add_test(NAME autolink_python_time
      COMMAND ${Python3_EXECUTABLE}
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_time.py")
    add_test(NAME autolink_python_pubsub
      COMMAND ${Python3_EXECUTABLE}
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pubsub.py")
    add_test(NAME autolink_python_service
      COMMAND ${Python3_EXECUTABLE}
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_service.py")
    add_test(NAME autolink_python_parameter
      COMMAND ${Python3_EXECUTABLE}
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_parameter.py")
    add_test(NAME autolink_python_record
      COMMAND ${Python3_EXECUTABLE}
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_record.py")
    add_test(NAME autolink_python_protobuf_pubsub
      COMMAND ${Python3_EXECUTABLE}
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_protobuf_pubsub.py")
    add_test(NAME autolink_python_protobuf_service
      COMMAND ${Python3_EXECUTABLE}
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_protobuf_service.py")
    add_test(NAME autolink_python_action
      COMMAND ${Python3_EXECUTABLE}
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_action.py")
  endif()
  # Prefer CycloneDDS/iceoryx from the same prefix as libddsc (often ROS Humble),
  # otherwise system iceoryx 1.x may be loaded first and break libddsc symbols.
  set(_autolink_py_ld_path "${CMAKE_BINARY_DIR}/lib")
  if(DEFINED ENV{AUTOLINK_PYTHON_LD_LIBRARY_PATH})
    set(_autolink_py_ld_path "$ENV{AUTOLINK_PYTHON_LD_LIBRARY_PATH}:${_autolink_py_ld_path}")
  elseif(EXISTS "/opt/ros/humble/lib/x86_64-linux-gnu")
    set(_autolink_py_ld_path "/opt/ros/humble/lib/x86_64-linux-gnu:${_autolink_py_ld_path}")
  endif()
  set_tests_properties(
    autolink_python_smoke_import autolink_python_time
    autolink_python_pubsub autolink_python_service
    autolink_python_parameter autolink_python_record
    autolink_python_protobuf_pubsub autolink_python_protobuf_service
    autolink_python_action
    PROPERTIES
    ENVIRONMENT "PYTHONPATH=${CMAKE_BINARY_DIR}/python;LD_LIBRARY_PATH=${_autolink_py_ld_path}:$ENV{LD_LIBRARY_PATH}")
endif()

# Avoid flaky LTO + parallel link failures in some Docker environments.
if(TARGET _core)
  set_property(TARGET _core PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
endif()
