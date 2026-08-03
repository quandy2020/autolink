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

# --- Unified CLI (CLI11) ---
set(AUTOLINK_CLI_SOURCES
  main.cpp
  cli/proto_json.cpp
  cli/cmd_channel.cpp
  cli/cmd_node.cpp
  cli/cmd_service.cpp
  cli/cmd_action.cpp
  cli/cmd_param.cpp
  cli/cmd_recorder.cpp
  cli/cmd_launch.cpp
  cli/cmd_monitor.cpp
  cli/cmd_doctor.cpp
  cli/cmd_completion.cpp
)

file(GLOB_RECURSE AUTOLINK_RECORDER_LIB_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/recorder/*.cpp")
list(FILTER AUTOLINK_RECORDER_LIB_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

file(GLOB AUTOLINK_MONITOR_LIB_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/monitor/*.cpp")
list(FILTER AUTOLINK_MONITOR_LIB_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

add_executable(autolink_cli
  ${AUTOLINK_CLI_SOURCES}
  ${AUTOLINK_RECORDER_LIB_SOURCES}
  ${AUTOLINK_MONITOR_LIB_SOURCES}
)
set_target_properties(autolink_cli PROPERTIES OUTPUT_NAME autolink)

target_include_directories(autolink_cli BEFORE PRIVATE
  ${AUTOLINK_ROOT_DIR}
  ${AUTOLINK_ROOT_DIR}/thirdparty/CLI11/include
)

target_link_libraries(autolink_cli PRIVATE autolink)

find_package(Curses REQUIRED)
if(TARGET Curses::Curses)
  target_link_libraries(autolink_cli PRIVATE Curses::Curses)
else()
  target_include_directories(autolink_cli PRIVATE ${CURSES_INCLUDE_DIR})
  target_link_libraries(autolink_cli PRIVATE ${CURSES_LIBRARIES})
endif()

install(TARGETS autolink_cli
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
