cmake_minimum_required(VERSION 3.28)
project(bazaartalks-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(BAZAARTALKS_BUILD_TESTS "Build unit/golden/parity tests" ON)

find_package(Eigen3 CONFIG REQUIRED)
find_package(date CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

if(BAZAARTALKS_BUILD_TESTS)
  find_package(Catch2 3 CONFIG REQUIRED)
  include(CTest)
  include(Catch)
endif()

# Phase 1: shared utility libraries (no deps on storage/network -- built first,
# used by every later phase).
add_subdirectory(libs/calendar)
add_subdirectory(libs/stats)
add_subdirectory(libs/linalg)

# Phase 2+ targets are added incrementally as each phase lands:
# add_subdirectory(libs/storage)
# add_subdirectory(libs/streaming)
# add_subdirectory(libs/http)
# add_subdirectory(libs/quant_core)
# add_subdirectory(services/stream_consumers)
# add_subdirectory(services/serve_api)
# add_subdirectory(services/pipeline_cli)
