# ctags: fast tags for C, C++, and Fortran
#   $ cmake -B build && cmake --build build && ctest --test-dir build
# Each executable is a unity build of one entry file, which a compiler
# may also be given directly (see the top of ctags.cpp). No C++ runtime
# library, exceptions, or RTTI, and on Windows no C runtime.
cmake_minimum_required(VERSION 3.21...4.4)

# Release by default at the top level, set before project()
get_property(ctags_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT ctags_multi_config AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

project(ctags LANGUAGES CXX)

include(CheckCXXCompilerFlag)
include(CheckLinkerFlag)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(
  -fno-exceptions -fno-rtti -fno-threadsafe-statics
  -fno-asynchronous-unwind-tables
  -Wall -Wextra -Wconversion -Wno-sign-conversion
  -Wno-unused-parameter -Wno-unused-function
)

# Debug builds trap on undefined behavior, which includes failed asserts.
set(CTAGS_TRAP "")
check_cxx_compiler_flag(-fsanitize-trap CTAGS_HAS_TRAP)
if(CTAGS_HAS_TRAP)
  set(CTAGS_TRAP -fsanitize=undefined -fsanitize-trap)
else()
  check_cxx_compiler_flag(-fsanitize-undefined-trap-on-error CTAGS_HAS_TRAP2)
  if(CTAGS_HAS_TRAP2)
    set(CTAGS_TRAP -fsanitize=undefined -fsanitize-undefined-trap-on-error)
  endif()
endif()
add_compile_options("$<$<CONFIG:Debug>:-g3;${CTAGS_TRAP}>")
add_link_options("$<$<CONFIG:Debug>:${CTAGS_TRAP}>")

check_linker_flag(CXX -nostdlib++ CTAGS_HAS_NOSTDLIBXX)
if(CTAGS_HAS_NOSTDLIBXX)
  add_link_options(-nostdlib++)
endif()

option(CTAGS_FORCE_XP "Take the Windows XP fallbacks, to test them" OFF)

if(WIN32)
  # CRT-free: the only import is kernel32.dll. Static libgcc keeps its
  # helpers (e.g. 64-bit division on i686) from adding a DLL dependency.
  add_executable(ctags main_win32.cpp)
  target_link_options(ctags PRIVATE -nostartfiles -static-libgcc
    -Wl,--stack,0x800000 -Wl,--gc-sections -Wl,--no-insert-timestamp
    "$<$<NOT:$<CONFIG:Debug>>:-s>")
  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_link_options(ctags PRIVATE -Wl,--large-address-aware)
  endif()
  target_link_libraries(ctags PRIVATE kernel32)
  if(CTAGS_FORCE_XP)
    target_compile_definitions(ctags PRIVATE CTAGS_FORCE_XP=1)
  endif()
else()
  add_executable(ctags main_posix.cpp)
  find_package(Threads REQUIRED)
  target_link_libraries(ctags PRIVATE Threads::Threads)
endif()

# Tests run where the programs can: natively, and for 32-bit Windows on
# 64-bit Windows.
if(NOT CMAKE_CROSSCOMPILING OR (CMAKE_HOST_WIN32 AND WIN32))
  enable_testing()

  add_executable(ctags_test main_test.cpp)
  set(CTAGS_TEST_GROUPS wide output cli walk c cxx fast fortran)
  foreach(group IN LISTS CTAGS_TEST_GROUPS)
    add_test(NAME unit/${group} COMMAND ctags_test ${group})
    set_tests_properties(unit/${group} PROPERTIES LABELS unit)
  endforeach()

  if(WIN32)
    # The program imports exactly test/imports.txt from KERNEL32.dll
    find_program(CTAGS_OBJDUMP NAMES objdump)
    if(CTAGS_OBJDUMP)
      add_test(NAME imports COMMAND ${CMAKE_COMMAND}
        -DOBJDUMP=${CTAGS_OBJDUMP} -DEXE=$<TARGET_FILE:ctags>
        -DWANT=${PROJECT_SOURCE_DIR}/test/imports.txt
        -P ${PROJECT_SOURCE_DIR}/cmake/imports.cmake)
      set_tests_properties(imports PROPERTIES LABELS unit)
    endif()
  endif()

  # Differential testing against Universal Ctags (a hosted tool, with a
  # C runtime, like a compiler's test harness)
  add_executable(tagdiff test/tagdiff.cpp)
  include(cmake/units.cmake)
  include(cmake/corpus.cmake)
  if(ctags_units_ok)
    add_test(NAME cases/uctags COMMAND ${CMAKE_COMMAND}
      -DUCTAGS=${CTAGS_UCTAGS} -DTEST=$<TARGET_FILE:ctags_test>
      -DTAGDIFF=$<TARGET_FILE:tagdiff> -DWORK=${PROJECT_BINARY_DIR}/work/cases
      -P ${PROJECT_SOURCE_DIR}/cmake/cases.cmake)
    set_tests_properties(cases/uctags PROPERTIES LABELS unit)
  endif()

  add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>
            -LE "corpus|perf|remote"
    DEPENDS ctags ctags_test tagdiff
    USES_TERMINAL)
endif()
