# RTYPE_SANITIZERS: the sanitizers the whole build runs under, empty for none. "address,undefined" finds memory
# errors, leaks and undefined behaviour; "thread" finds data races. The flags are added to every target, ours and
# the dependencies built by CPM: a sanitizer must see all the code, or it reports what it does not understand.
#
# Included before any target is created. See docs/contributing/testing.md#sanitizers.

set(RTYPE_SANITIZERS "" CACHE STRING "Sanitizers for the whole build: empty, address,undefined or thread")
set_property(CACHE RTYPE_SANITIZERS PROPERTY STRINGS "" "address,undefined" "thread")

if(RTYPE_SANITIZERS STREQUAL "")
  return()
endif()

if(NOT RTYPE_SANITIZERS STREQUAL "address,undefined" AND NOT RTYPE_SANITIZERS STREQUAL "thread")
  message(FATAL_ERROR "RTYPE_SANITIZERS is \"${RTYPE_SANITIZERS}\": use \"address,undefined\" or \"thread\"")
endif()
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
  message(FATAL_ERROR "RTYPE_SANITIZERS needs GCC or Clang, not ${CMAKE_CXX_COMPILER_ID}")
endif()

message(STATUS "Sanitizers: ${RTYPE_SANITIZERS}")

# -fno-sanitize-recover: undefined behaviour stops the program like the other errors, so that a test fails on it
add_compile_options(-fsanitize=${RTYPE_SANITIZERS} -fno-sanitize-recover=all -fno-omit-frame-pointer)
add_link_options(-fsanitize=${RTYPE_SANITIZERS})

# The options and the suppressions, in every program created from here
add_library(rtype_sanitizer_defaults OBJECT ${CMAKE_CURRENT_LIST_DIR}/Sanitizers.cpp)
link_libraries($<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:rtype_sanitizer_defaults>)
