cmake_minimum_required(VERSION 3.0.2)

project(utility C CXX)

include(CheckFunctionExists)
include(CheckLibraryExists)

check_function_exists(renameat2 HAVE_RENAMEAT2)
configure_file(config.h.in config.h)

# guard from super-projects, i.e. when it is added as subdirectory
IF(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  # Remove existing -Wall in debug flags because we unconditionally supply
  # -Wall and we don't want a trailing -Wall re-enable the silenced
  # warnings
  string(REPLACE "-Wall" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-missing-braces")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-missing-braces")

  add_subdirectory(libixxx)
endif()

if((CMAKE_C_COMPILER_ID STREQUAL "GNU"
        AND CMAKE_C_COMPILER_VERSION VERSION_LESS "5")
   OR
   (CMAKE_C_COMPILER_ID STREQUAL "Clang"
        AND CMAKE_C_COMPILER_VERSION VERSION_LESS "4")
  )
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(NO_SANITIZE "-fno-sanitize=address")
  endif()
else()
  set(NO_SANITIZE "-fno-sanitize=all")
endif()

check_library_exists(c clock_gettime "" CLOCK_GETTIME_IN_C)
if (CLOCK_GETTIME_IN_C)
  set(RT_LIB "")
else()
  set(RT_LIB "-lrt")
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

add_executable(pargs pargs.c)
set_property(TARGET pargs PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE)

# NOTE: when compiling with sanitizers (address etc.) it's important
# that these test programs are compiled without those flags because
# we need to create some core files of them.
# Apparently, the sanitizing might blow up the core files, i.e. yielding:
# warning: Failed to write corefile contents (No space left on device).
add_executable(snooze test/snooze.c)
set_target_properties(snooze
  PROPERTIES COMPILE_FLAGS "${NO_SANITIZE}" LINK_FLAGS "${NO_SANITIZE}")
add_executable(snooze32 test/snooze.c)
set_target_properties(snooze32
  PROPERTIES COMPILE_FLAGS "-m32 ${NO_SANITIZE}"
  LINK_FLAGS "-m32 ${NO_SANITIZE}")
add_executable(busy_snooze test/busy_snooze.c)
set_target_properties(busy_snooze
  PROPERTIES COMPILE_FLAGS "${NO_SANITIZE}" LINK_FLAGS "${NO_SANITIZE}")
target_link_libraries(busy_snooze ${RT_LIB})

add_custom_target(check-old
  COMMAND env src_dir=${CMAKE_CURRENT_SOURCE_DIR}
                      ${CMAKE_CURRENT_SOURCE_DIR}/test/main.py
  DEPENDS silence fail lockf
  COMMENT "run unittests"
  )

add_custom_target(check-new
  # work around py.test-3 pytest etc. system differences
  COMMAND python3 -m pytest -v
    ${CMAKE_CURRENT_SOURCE_DIR}/user-installed.py
    ${CMAKE_CURRENT_SOURCE_DIR}/ascii.py
    ${CMAKE_CURRENT_SOURCE_DIR}/test/pargs.py
  DEPENDS pargs snooze32 snooze busy_snooze
  COMMENT "run pytests"
  )

add_custom_target(check DEPENDS check-old check-new)
