cmake_minimum_required(VERSION 3.0.2)

project(utility C CXX)

# guard from super-projects, i.e. when it is added as subdirectory
IF(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  # Remove existing -Wall in debug flags because we unconditionally supply
  # -Wall and we don't want a trailing -Wall re-enable the silenced
  # warnings
  string(REPLACE "-Wall" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

  add_subdirectory(libixxx)
endif()

add_executable(chronic chronic.c)
set_property(TARGET chronic PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE)

add_executable(fail test/fail.c)


add_executable(chronicc chronic.cc)
set_property(TARGET chronicc PROPERTY INCLUDE_DIRECTORIES
  ${CMAKE_SOURCE_DIR}/libixxx)
target_link_libraries(chronicc
  ixxx_static)

add_custom_target(check
  COMMAND env src_dir=${CMAKE_CURRENT_SOURCE_DIR}
                      ${CMAKE_CURRENT_SOURCE_DIR}/test/main.py
  DEPENDS chronic fail
  COMMENT "run unittests"
  )
