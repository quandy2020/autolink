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

find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

set(PYBIND11_FINDPYTHON ON)
add_subdirectory(
  "${AUTOLINK_ROOT_DIR}/thirdparty/pybind11"
  "${CMAKE_BINARY_DIR}/thirdparty/pybind11"
  EXCLUDE_FROM_ALL)

set(_pkg "${CMAKE_BINARY_DIR}/python/autolink")
file(MAKE_DIRECTORY "${_pkg}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/autolink/" DESTINATION "${_pkg}"
  FILES_MATCHING PATTERN "*.py")

file(GLOB _bindings CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/bindings/*.cpp")
pybind11_add_module(_core ${_bindings})
target_include_directories(_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/bindings" "${AUTOLINK_ROOT_DIR}")
target_link_libraries(_core PRIVATE autolink)
set_target_properties(_core PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${_pkg}"
  RUNTIME_OUTPUT_DIRECTORY "${_pkg}"
  INTERPROCEDURAL_OPTIMIZATION FALSE)
add_dependencies(_core autolink_python_pb2)

install(TARGETS _core LIBRARY DESTINATION "python/autolink" RUNTIME DESTINATION "python/autolink")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/autolink/"
  DESTINATION "python/autolink" FILES_MATCHING PATTERN "*.py")
install(DIRECTORY
  "${CMAKE_BINARY_DIR}/python/autolink/proto"
  "${CMAKE_BINARY_DIR}/python/autolink/examples"
  DESTINATION "python/autolink" FILES_MATCHING PATTERN "*.py")

if(AUTOLINK_BUILD_TEST)
  execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import pytest"
    RESULT_VARIABLE _pytest_ok
    OUTPUT_QUIET ERROR_QUIET)
  # Same prefix as libddsc. A newer system iceoryx breaks its symbols.
  set(_ld "${CMAKE_BINARY_DIR}/lib")
  if(DEFINED ENV{AUTOLINK_PYTHON_LD_LIBRARY_PATH})
    set(_ld "$ENV{AUTOLINK_PYTHON_LD_LIBRARY_PATH}:${_ld}")
  elseif(EXISTS "/opt/ros/humble/lib/x86_64-linux-gnu")
    set(_ld "/opt/ros/humble/lib/x86_64-linux-gnu:${_ld}")
  endif()

  file(GLOB _tests CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.py")
  foreach(_test ${_tests})
    cmake_path(GET _test STEM _stem)
    string(REGEX REPLACE "^test_" "" _stem "${_stem}")
    if(_pytest_ok EQUAL 0)
      add_test(NAME "autolink_python_${_stem}"
        COMMAND ${Python3_EXECUTABLE} -m pytest "${_test}" -q)
    else()
      add_test(NAME "autolink_python_${_stem}"
        COMMAND ${Python3_EXECUTABLE} "${_test}")
    endif()
    set_tests_properties("autolink_python_${_stem}" PROPERTIES
      ENVIRONMENT "PYTHONPATH=${CMAKE_BINARY_DIR}/python;LD_LIBRARY_PATH=${_ld}:$ENV{LD_LIBRARY_PATH}")
  endforeach()
endif()
