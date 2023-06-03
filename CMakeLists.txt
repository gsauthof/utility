cmake_minimum_required(VERSION 3.0.2)

project(utility C CXX)

include(CheckFunctionExists)
include(CheckLibraryExists)
include(CMakePushCheckState)

CMAKE_PUSH_CHECK_STATE(RESET)
set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_function_exists(renameat2 HAVE_RENAMEAT2)
cmake_pop_check_state()
configure_file(config.h.in config.h)

add_subdirectory(libixxx)
add_subdirectory(libixxxutil)

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
target_compile_definitions(silence PRIVATE _GNU_SOURCE)

add_executable(fail test/fail.c)


add_executable(silencce silence.cc)
target_link_libraries(silencce
    ixxxutil_static
    ixxx_static
)


add_executable(oldprocs oldprocs.cc)
target_link_libraries(oldprocs
    ixxxutil_static
    ixxx_static
)

add_executable(lockf lockf.c)


add_executable(dcat dcat.cc)
target_link_libraries(dcat
    ixxxutil_static
    ixxx_static
)

add_executable(swap swap.c)
target_include_directories(swap PRIVATE ${CMAKE_BINARY_DIR})

add_executable(pargs pargs.c)
target_compile_definitions(pargs PRIVATE _GNU_SOURCE)

add_executable            (pargs32 pargs.c)
target_compile_definitions(pargs32 PRIVATE _GNU_SOURCE)
target_compile_options    (pargs32 PRIVATE -m32 ${NO_SANITIZE})
target_link_options       (pargs32 PRIVATE -m32 ${NO_SANITIZE})

# NOTE: when compiling with sanitizers (address etc.) it's important
# that these test programs are compiled without those flags because
# we need to create some core files of them.
# Apparently, the sanitizing might blow up the core files, i.e. yielding:
# warning: Failed to write corefile contents (No space left on device).
add_executable        (snooze test/snooze.c)
target_compile_options(snooze PRIVATE ${NO_SANITIZE})
target_link_options   (snooze PRIVATE ${NO_SANITIZE})
add_executable        (snooze32 test/snooze.c)
target_compile_options(snooze32 PRIVATE -m32 ${NO_SANITIZE})
target_link_options   (snooze32 PRIVATE -m32 ${NO_SANITIZE})
add_executable        (busy_snooze test/busy_snooze.c)
target_compile_options(busy_snooze PRIVATE ${NO_SANITIZE})
target_link_options   (busy_snooze PRIVATE ${NO_SANITIZE})
target_link_libraries (busy_snooze ${RT_LIB})

add_executable(searchb searchb.c)

add_executable(searchbxx searchb.cc)
target_link_libraries(searchbxx
  ixxxutil_static
  ixxx_static
)

add_executable(exec exec.c)

add_executable(adjtimex adjtimex.c)

add_executable(pq pq.cc syscalls.cc)
target_link_libraries(pq PUBLIC
    ixxxutil_static
    ixxx_static
)


add_custom_target(check-old
  COMMAND env src_dir=${CMAKE_CURRENT_SOURCE_DIR}
                      ${CMAKE_CURRENT_SOURCE_DIR}/test/main.py
  DEPENDS silence silencce fail lockf
  COMMENT "run unittests"
  )

add_custom_target(check-new
  # work around py.test-3 pytest etc. system differences
  COMMAND python3 -m pytest -v
    ${CMAKE_CURRENT_SOURCE_DIR}/user-installed.py
    ${CMAKE_CURRENT_SOURCE_DIR}/ascii.py
    ${CMAKE_CURRENT_SOURCE_DIR}/test/pargs.py
    ${CMAKE_CURRENT_SOURCE_DIR}/test/dcat.py
  DEPENDS dcat pargs pargs32 snooze32 snooze busy_snooze swap
  COMMENT "run pytests"
  )

add_custom_target(check DEPENDS check-old check-new)


install(TARGETS adjtimex dcat exec lockf oldprocs pargs pq searchb silence swap
    RUNTIME DESTINATION bin)
set(scripts
    addrof.sh
    arsort.sh
    ascii.py
    check-cert.py
    check-dnsbl.py
    chromium-extensions.py
    cpufreq.py
    detect-size.py
    devof.sh
    disas.sh
    firefox-addons.py
    gs-ext.py
    inhibit.py
    isempty.py
    latest-kernel-running.sh
    lsata.sh
    macgen.py
    matrixto.py
    pdfmerge.py
    pldd.py
    pwhatch.sh
    remove.py
    reset-tmux.sh
    ripdvd
    unrpm.sh
    user-installed.py
    wipedev.py
    )
foreach(script ${scripts})
    STRING(REGEX REPLACE "\\.[^.]*$" "" name ${script})
    install(PROGRAMS ${script} DESTINATION bin RENAME ${name})
endforeach()
