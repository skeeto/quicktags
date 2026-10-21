# Differential tests against Universal Ctags 6.2.1 on its own test inputs
# (Units/), which are GPL-2, so they are extracted from its source
# tarball into the build tree and never copied into this repository.
# Each case runs in modes: "default" (no options), "allkinds" (every kind
# enabled), and "args" (the case's own --kinds options, when it has any).
# Differences must be explained in test/ledger.txt.

set(CTAGS_UCTAGS_TARBALL "$ENV{USERPROFILE}/src/w64devkit/source/ctags-6.2.1.tar.gz"
  CACHE FILEPATH "Universal Ctags 6.2.1 source tarball, for its test inputs")
find_program(CTAGS_UCTAGS NAMES ctags DOC "Universal Ctags 6.2.1")
set(CTAGS_UNITS_ONLY "" CACHE STRING
  "Compare only these kinds, e.g. C:d,C++:d (empty for all)")

set(ctags_units_ok OFF)
if(CTAGS_UCTAGS AND EXISTS "${CTAGS_UCTAGS_TARBALL}")
  execute_process(COMMAND ${CTAGS_UCTAGS} --version
    OUTPUT_VARIABLE ctags_uctags_version ERROR_QUIET)
  if(ctags_uctags_version MATCHES "^Universal Ctags 6\\.2\\.1")
    set(ctags_units_ok ON)
  else()
    message(STATUS "ctags: ${CTAGS_UCTAGS} is not Universal Ctags 6.2.1")
  endif()
endif()

if(NOT ctags_units_ok)
  message(STATUS "ctags: no Units tests (need Universal Ctags 6.2.1 and its tarball)")
  add_test(NAME units/missing COMMAND ${CMAKE_COMMAND} -E false)
  set_tests_properties(units/missing PROPERTIES LABELS units DISABLED ON)
  return()
endif()

set(ctags_units_dir ${PROJECT_BINARY_DIR}/uctags)
set(ctags_units_cats parser-c.r parser-cxx.r parser-fortran.r parser-cpreprocessor.r)
set(ctags_units_stamp ${ctags_units_dir}/stamp)
if(NOT EXISTS ${ctags_units_stamp})
  set(patterns "")
  foreach(cat IN LISTS ctags_units_cats)
    list(APPEND patterns ctags-6.2.1/Units/${cat})
  endforeach()
  file(ARCHIVE_EXTRACT INPUT ${CTAGS_UCTAGS_TARBALL}
    DESTINATION ${ctags_units_dir} PATTERNS ${patterns})
  file(WRITE ${ctags_units_stamp} "")
endif()

foreach(cat IN LISTS ctags_units_cats)
  file(GLOB cases LIST_DIRECTORIES true
    ${ctags_units_dir}/ctags-6.2.1/Units/${cat}/*.d
    ${ctags_units_dir}/ctags-6.2.1/Units/${cat}/*.b)
  foreach(case IN LISTS cases)
    get_filename_component(name ${case} NAME)
    foreach(mode IN ITEMS default allkinds args)
      set(test units/${cat}/${name}/${mode})
      add_test(NAME ${test} COMMAND ${CMAKE_COMMAND}
        -DUCTAGS=${CTAGS_UCTAGS}
        -DOURS=$<TARGET_FILE:ctags>
        -DTAGDIFF=$<TARGET_FILE:tagdiff>
        -DCASE=${case}
        -DMODE=${mode}
        -DSCOPE=${test}
        -DONLY=${CTAGS_UNITS_ONLY}
        -DLEDGER=${PROJECT_SOURCE_DIR}/test/ledger.txt
        -DWORK=${PROJECT_BINARY_DIR}/work/${test}
        -P ${PROJECT_SOURCE_DIR}/cmake/runcase.cmake)
      set_tests_properties(${test} PROPERTIES
        LABELS "units;${cat}" SKIP_RETURN_CODE 77 TIMEOUT 60)
    endforeach()
  endforeach()
endforeach()
