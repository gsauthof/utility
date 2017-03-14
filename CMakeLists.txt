cmake_minimum_required(VERSION 3.0.2)

project(utility C CXX)

include(CheckFunctionExists)

check_function_exists(renameat2 HAVE_RENAMEAT2)
configure_file(config.h.in config.h)

# guard from super-projects, i.e. when it is added as subdirectory
IF(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  # Remove existing -Wall in debug flags because we unconditionally supply
  # -Wall and we don't want a trailing -Wall re-enable the silenced
  # warnings
  string(REPLACE "-Wall" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

  add_subdirectory(libixxx)
endif()

add_executable(silence silence.c)
set_property(TARGET silence PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE)

add_executable(fail test/fail.c)


add_executable(silencce silence.cc)
set_property(TARGET silencce PROPERTY INCLUDE_DIRECTORIES
  ${CMAKE_SOURCE_DIR}/libixxx)
target_link_libraries(silencce
  ixxx_static)

add_executable(lockf lockf.c)

add_executable(swap swap.c)
set_property(TARGET swap PROPERTY INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR})

add_custom_target(check-old
  COMMAND env src_dir=${CMAKE_CURRENT_SOURCE_DIR}
                      ${CMAKE_CURRENT_SOURCE_DIR}/test/main.py
  DEPENDS silence fail lockf
  COMMENT "run unittests"
  )

add_custom_target(check-new
  COMMAND py.test-3 ${CMAKE_CURRENT_SOURCE_DIR}/user-installed.py
  COMMENT "run pytests"
  )

add_custom_target(check DEPENDS check-old check-new)
