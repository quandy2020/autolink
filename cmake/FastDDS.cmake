# Copyright 2026 The Openbot Authors (duyongquan)
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

# Optional Fast DDS 2.14 for RTPS transport (AUTOLINK_ENABLE_FASTDDS).
#
# FetchContent pin: GIT_TAG v2.14.6 (latest 2.14.x).
# Upstream tags use the "v" prefix; unprefixed "2.14.4" from the plan does not
# exist on eProsima/Fast-DDS. Prefer a system install via find_package when
# available.
#
# Package / target note: Fast DDS 2.14 still exports CMake package and library
# target name `fastrtps` (renamed to `fastdds` in 3.x). We try `fastdds` first
# (plan / 3.x naming), then `fastrtps` for installed 2.14 packages, then
# FetchContent which builds the `fastrtps` target.

set(AUTOLINK_HAS_FASTDDS OFF)
set(AUTOLINK_FASTDDS_LINK_LIBS "")

# Latest 2.14.x tag used when FetchContent is required.
set(AUTOLINK_FASTDDS_GIT_TAG "v2.14.6")

if(AUTOLINK_ENABLE_FASTDDS)
  find_package(fastdds 2.14 QUIET)
  if(fastdds_FOUND)
    set(AUTOLINK_HAS_FASTDDS ON)
    set(AUTOLINK_FASTDDS_LINK_LIBS fastdds fastcdr)
    message(STATUS "autolink: using Fast DDS via find_package(fastdds) "
                   "(${fastdds_VERSION})")
  else()
    find_package(fastrtps 2.14 QUIET)
    if(fastrtps_FOUND)
      set(AUTOLINK_HAS_FASTDDS ON)
      set(AUTOLINK_FASTDDS_LINK_LIBS fastrtps fastcdr)
      message(STATUS "autolink: using Fast DDS 2.14 via find_package(fastrtps) "
                     "(${fastrtps_VERSION}; 2.x CMake package name)")
    else()
      message(STATUS "autolink: fastdds/fastrtps 2.14 not found; "
                     "FetchContent Fast-DDS ${AUTOLINK_FASTDDS_GIT_TAG}")
      include(FetchContent)
      # Pull Fast-CDR / foonathan_memory / Asio from Fast-DDS thirdparty/
      # when missing on the host.
      set(THIRDPARTY ON CACHE STRING "Activate use of internal submodules." FORCE)
      set(COMPILE_EXAMPLES OFF CACHE BOOL "" FORCE)
      set(COMPILE_TOOLS OFF CACHE BOOL "" FORCE)
      set(BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
      set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
      FetchContent_Declare(
        fastdds
        GIT_REPOSITORY https://github.com/eProsima/Fast-DDS.git
        GIT_TAG ${AUTOLINK_FASTDDS_GIT_TAG}
        GIT_SHALLOW TRUE
      )
      FetchContent_MakeAvailable(fastdds)
      set(AUTOLINK_HAS_FASTDDS ON)
      set(AUTOLINK_FASTDDS_LINK_LIBS fastrtps fastcdr)
      message(STATUS "autolink: Fast DDS ${AUTOLINK_FASTDDS_GIT_TAG} via FetchContent")
    endif()
  endif()
endif()

# Apply AUTOLINK_ENABLE_FASTDDS=0/1 and optional link deps to a target.
function(autolink_apply_fastdds TARGET)
  if(NOT TARGET ${TARGET})
    message(FATAL_ERROR "autolink_apply_fastdds: target '${TARGET}' does not exist")
  endif()
  if(AUTOLINK_HAS_FASTDDS)
    target_compile_definitions(${TARGET} PUBLIC AUTOLINK_ENABLE_FASTDDS=1)
    target_link_libraries(${TARGET} PUBLIC ${AUTOLINK_FASTDDS_LINK_LIBS})
  else()
    target_compile_definitions(${TARGET} PUBLIC AUTOLINK_ENABLE_FASTDDS=0)
  endif()
endfunction()
