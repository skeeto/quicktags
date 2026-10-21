# Check the unit tests' tag cases against Universal Ctags: SHARED cases
# must be its exact output, IMPROVED cases must record its output.
#   $ cmake -DUCTAGS=... -DTEST=ctags_test -DTAGDIFF=... -DWORK=dir -P cases.cmake
cmake_minimum_required(VERSION 3.21...4.4)

file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})
execute_process(COMMAND ${TEST} --export ${WORK} RESULT_VARIABLE status)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "cannot export cases")
endif()

file(GLOB cases LIST_DIRECTORIES true ${WORK}/*)
set(failed 0)
foreach(case IN LISTS cases)
  if(NOT EXISTS ${case}/case)
    continue()
  endif()
  file(STRINGS ${case}/case info)
  list(GET info 0 kindfile)
  list(GET info 1 name)
  set(args "")
  list(LENGTH info n)
  if(n GREATER 2)
    list(GET info 2 argline)
    separate_arguments(args UNIX_COMMAND "${argline}")
  endif()
  string(REGEX REPLACE "^[a-z]+ " "" file "${kindfile}")

  execute_process(
    COMMAND ${UCTAGS} --options=NONE --languages=C,C++,Fortran --fields=+n
            ${args} -f ref.tags ${file}
    WORKING_DIRECTORY ${case}
    RESULT_VARIABLE status ERROR_QUIET)
  # Unsorted output is compared in order, and Emacs tags as such
  set(diff "")
  list(FIND args --sort=no sortno)
  list(FIND args -u unsorted)
  list(FIND args -e etags)
  list(FIND args --output-format=etags etagslong)
  if(sortno GREATER -1 OR unsorted GREATER -1)
    set(diff --ordered)
  endif()
  if(etags GREATER -1 OR etagslong GREATER -1)
    set(diff --ordered --etags)
  endif()
  execute_process(
    COMMAND ${TAGDIFF} ${diff} ${case}/ref.tags ${case}/want.tags
    RESULT_VARIABLE status OUTPUT_VARIABLE out)
  if(NOT status EQUAL 0)
    message("${name} (${kindfile}):\n${out}")
    math(EXPR failed "${failed} + 1")
  endif()
endforeach()

if(failed)
  message(FATAL_ERROR "${failed} cases disagree with Universal Ctags")
endif()
