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

if(NOT TARGET autolink)
  return()
endif()

# Header is "examples.pb.h", so the proto directory is the layout root.
autocmake_protobuf(examples_proto
  SOURCES proto/examples.proto
  IMPORTS "${CMAKE_CURRENT_SOURCE_DIR}/proto")

function(autolink_example name)
  autocmake_binary(autolink_example_${name}
    SOURCES ${name}.cpp
    DEPENDENCIES autolink ${ARGN})
  set_target_properties(autolink_example_${name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/examples")
endfunction()

function(autolink_component_example name)
  autocmake_library(${name} SHARED SOURCES "${name}/${name}.cpp")
  target_link_libraries(${name} PRIVATE autolink examples_proto)
  set_target_properties(${name} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/examples/${name}")
endfunction()

foreach(_name IN ITEMS
    talker listener talker_listener service paramserver record
    action_listener action_talker action_talker2)
  autolink_example(${_name} examples_proto)
endforeach()
foreach(_name IN ITEMS pod_talker_listener pod_talker pod_listener)
  autolink_example(${_name})
endforeach()

autolink_component_example(timer_component_example)
autolink_component_example(common_component_example)
