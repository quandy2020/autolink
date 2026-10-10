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

file(GLOB _cli CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cli/*.cpp")
file(GLOB_RECURSE _libs CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/recorder/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/monitor/*.cpp")
list(FILTER _libs EXCLUDE REGEX "/main\\.cpp$")

autocmake_binary(autolink_cli
  SOURCES main.cpp ${_cli} ${_libs}
  DEPENDENCIES autolink)
set_target_properties(autolink_cli PROPERTIES OUTPUT_NAME autolink)
target_include_directories(autolink_cli BEFORE PRIVATE
  "${AUTOLINK_ROOT_DIR}"
  "${AUTOLINK_ROOT_DIR}/thirdparty/CLI11/include")
target_link_libraries(autolink_cli PRIVATE pugixml::pugixml)

find_package(Curses REQUIRED)
if(TARGET Curses::Curses)
  target_link_libraries(autolink_cli PRIVATE Curses::Curses)
else()
  target_include_directories(autolink_cli PRIVATE ${CURSES_INCLUDE_DIR})
  target_link_libraries(autolink_cli PRIVATE ${CURSES_LIBRARIES})
endif()
