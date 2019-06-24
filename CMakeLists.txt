# Copyright (C) 2019 Sven Willner <sven.willner@pik-potsdam.de>
#
# This file is part of Acclimate Impact Preprocessing.
#
# Acclimate Impact Preprocessing is free software: you can
# redistribute it and/or modify it under the terms of the GNU Affero
# General Public License as published by the Free Software Foundation,
# either version 3 of the License, or (at your option) any later
# version.
#
# Acclimate Impact Preprocessing is distributed in the hope that it
# will be useful, but WITHOUT ANY WARRANTY; without even the implied
# warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
# the GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public
# License along with AcclimateImpact Preprocessing.  If not, see
# <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
include(cmake/helpers.cmake)
include(cmake/libraries.cmake)
add_system_paths()

set_default_build_type(Release)

project(impactgen)

file(GLOB IMPACTGEN_SOURCES src/*.cpp src/impacts/*.cpp)
add_executable(impactgen ${IMPACTGEN_SOURCES})

target_include_directories(impactgen PRIVATE include lib/cpp-library)
target_compile_options(impactgen PRIVATE -std=c++14)

set_ccache_use()
set_advanced_cpp_warnings(impactgen)
set_build_type_specifics(impactgen)
add_git_version(impactgen
  FALLBACK_VERSION 1.0.0
  WITH_DIFF)

option(IMPACTGEN_PARALLELIZATION "" ON)
if(IMPACTGEN_PARALLELIZATION)
  find_package(OpenMP REQUIRED)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

option(IMPACTGEN_PROGRESSBARS_SILENT "" OFF)
if(IMPACTGEN_PROGRESSBARS_SILENT)
  target_compile_definitions(impactgen PUBLIC PROGRESSBAR_SILENT)
endif()

include_netcdf_cxx4(impactgen ON v4.3.0)

include(lib/settingsnode/settingsnode.cmake)
include_settingsnode(impactgen)
include_yaml_cpp(impactgen ON "yaml-cpp-0.6.2")

add_cpp_tools(impactgen)
