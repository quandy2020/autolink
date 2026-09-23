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

# Optional Fast DDS 3.x for RTPS transport (AUTOLINK_ENABLE_FASTDDS).
#
# FetchContent pin: GIT_TAG v3.6.2 (latest stable 3.x as of 2026-09-07).
# Upstream tags use the "v" prefix. Prefer a system install via
# find_package(fastdds 3) when available; otherwise FetchContent builds
# with SECURITY=ON (requires OpenSSL). Major < 3 on the prefix is FATAL.

set(AUTOLINK_HAS_FASTDDS OFF)
set(AUTOLINK_FASTDDS_LINK_LIBS "")

# Latest stable 3.x tag used when FetchContent is required.
set(AUTOLINK_FASTDDS_GIT_TAG "v3.6.2")

# Probe installed fastdds/fastrtps ConfigVersion without importing targets.
# Returns PACKAGE_VERSION via PARENT_SCOPE when a version file is found.
function(autolink_probe_fastdds_version out_version out_verfile)
  set(_probe_roots
      ${CMAKE_PREFIX_PATH}
      "/opt/homebrew/lib/cmake"
      "/usr/local/lib/cmake"
      "/usr/lib/cmake"
      "/usr/lib64/cmake")
  foreach(_root IN LISTS _probe_roots)
    foreach(_pkg IN ITEMS fastdds fastrtps)
      set(_verfile "${_root}/${_pkg}/${_pkg}ConfigVersion.cmake")
      if(EXISTS "${_verfile}")
        set(PACKAGE_VERSION "")
        include("${_verfile}")
        set(${out_version} "${PACKAGE_VERSION}" PARENT_SCOPE)
        set(${out_verfile} "${_verfile}" PARENT_SCOPE)
        return()
      endif()
    endforeach()
  endforeach()
  set(${out_version} "" PARENT_SCOPE)
  set(${out_verfile} "" PARENT_SCOPE)
endfunction()

function(_autolink_use_system_fastdds version libs)
  if(version VERSION_LESS "3.0")
    message(FATAL_ERROR "autolink requires Fast DDS >= 3.0 (found ${version})")
  endif()
  set(AUTOLINK_HAS_FASTDDS ON PARENT_SCOPE)
  set(AUTOLINK_FASTDDS_LINK_LIBS ${libs} PARENT_SCOPE)
  message(STATUS "autolink: Fast DDS ${version} (${libs})")
  message(WARNING
    "autolink: system Fast DDS may have been built without SECURITY; "
    "DDS Security plugins will fail at runtime if missing.")
endfunction()

# FATAL if prefix has Fast DDS / fastrtps major < 3 (do not Fetch over 2.x).
function(autolink_fatal_if_fastdds_2x_installed)
  autolink_probe_fastdds_version(_probe_ver _probe_file)
  if(_probe_ver AND _probe_ver VERSION_LESS "3.0")
    message(FATAL_ERROR
      "autolink requires Fast DDS >= 3.0 (found ${_probe_ver} at "
      "${_probe_file}). Uninstall the 2.x package or clear it from "
      "CMAKE_PREFIX_PATH, then re-run cmake.")
  endif()
endfunction()

if(AUTOLINK_ENABLE_FASTDDS)
  find_package(fastdds 3 QUIET)
  if(fastdds_FOUND)
    _autolink_use_system_fastdds("${fastdds_VERSION}" "fastdds;fastcdr")
  else()
    autolink_fatal_if_fastdds_2x_installed()
    find_package(fastrtps QUIET)
    if(fastrtps_FOUND)
      _autolink_use_system_fastdds("${fastrtps_VERSION}" "fastrtps;fastcdr")
    else()
      message(STATUS "autolink: fastdds 3.x not found; "
                     "FetchContent Fast-DDS ${AUTOLINK_FASTDDS_GIT_TAG}")

      # DDS Security requires OpenSSL when building Fast DDS via FetchContent.
      find_package(OpenSSL QUIET)
      if(NOT OpenSSL_FOUND)
        message(FATAL_ERROR
          "autolink: AUTOLINK_ENABLE_FASTDDS=ON requires OpenSSL to build "
          "Fast DDS with SECURITY=ON.\n"
          "  macOS: brew install openssl && export OPENSSL_ROOT_DIR="
          "\"$(brew --prefix openssl)\"\n"
          "  Then re-run cmake, or pass -DOPENSSL_ROOT_DIR=...")
      endif()
      message(STATUS "autolink: OpenSSL ${OPENSSL_VERSION} "
                     "(${OPENSSL_INCLUDE_DIR})")

      include(FetchContent)
      # Pull Fast-CDR / foonathan_memory / Asio from Fast-DDS thirdparty/
      # when missing on the host.
      set(THIRDPARTY ON CACHE STRING "Activate use of internal submodules." FORCE)
      set(COMPILE_EXAMPLES OFF CACHE BOOL "" FORCE)
      set(COMPILE_TOOLS OFF CACHE BOOL "" FORCE)
      set(BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
      set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
      set(SECURITY ON CACHE BOOL "Enable Fast DDS Security" FORCE)
      FetchContent_Declare(
        fastdds
        GIT_REPOSITORY https://github.com/eProsima/Fast-DDS.git
        GIT_TAG ${AUTOLINK_FASTDDS_GIT_TAG}
        GIT_SHALLOW TRUE
      )
      FetchContent_MakeAvailable(fastdds)
      set(AUTOLINK_HAS_FASTDDS ON)
      if(TARGET fastdds)
        set(AUTOLINK_FASTDDS_LINK_LIBS fastdds fastcdr)
      elseif(TARGET fastrtps)
        set(AUTOLINK_FASTDDS_LINK_LIBS fastrtps fastcdr)
        message(WARNING
          "autolink: linking fastrtps alias; prefer fastdds target")
      else()
        message(FATAL_ERROR
          "autolink: Fast DDS FetchContent produced no fastdds/fastrtps "
          "target")
      endif()
      message(STATUS "autolink: Fast DDS ${AUTOLINK_FASTDDS_GIT_TAG} via "
                     "FetchContent (SECURITY=ON)")
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
