cmake_minimum_required(VERSION 3.25)

project(RType
    VERSION 0.1.0
    DESCRIPTION "Networked R-Type clone built on a custom game engine"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(RTYPE_BUILD_TESTS "Build the unit and integration tests" ON)

include(cmake/CPM.cmake)

add_library(rtype_options INTERFACE)
if(MSVC)
  target_compile_options(rtype_options INTERFACE /W4 /permissive- /utf-8)
else()
  target_compile_options(rtype_options INTERFACE -Wall -Wextra -Wpedantic)
endif()

add_subdirectory(libs)
add_subdirectory(apps)

if(RTYPE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

set(CPACK_PACKAGE_NAME "r-type")
set(CPACK_PACKAGE_VENDOR "R-Type team")
set(CPACK_PACKAGE_FILE_NAME "r-type-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")
set(CPACK_COMPONENTS_ALL client server)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY ON)
if(WIN32)
  set(CPACK_GENERATOR ZIP)
else()
  set(CPACK_GENERATOR TGZ)
endif()
include(CPack)
