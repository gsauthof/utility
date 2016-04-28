cmake_minimum_required(VERSION 3.0.2)

project(utility C CXX)

add_executable(chronic chronic.c)
set_property(TARGET chronic PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE)

add_executable(fail test/fail.c)



add_custom_target(check
  COMMAND env src_dir=${CMAKE_CURRENT_SOURCE_DIR}
                      ${CMAKE_CURRENT_SOURCE_DIR}/test/main.py
  DEPENDS chronic fail
  COMMENT "run unittests"
  )
