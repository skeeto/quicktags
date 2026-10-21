# Run one Units case through Universal Ctags and ctags, and compare.
#   $ cmake -DUCTAGS=... -DOURS=... -DTAGDIFF=... -DCASE=dir -DMODE=default
#           -DSCOPE=name -DLEDGER=file -DWORK=dir [-DONLY=kinds] -P runcase.cmake
# Exits 77 (skipped) when the mode does not apply to the case.
#
# Modes: "default" (no options), "allkinds" (every kind enabled), "args"
# (the case's options that change which tags exist), and "fmt" (those and
# its options that change only how tags are written, compared by tag
# against a run without the latter: tagdiff --base). caseopts.cmake
# chooses the options.

include(${CMAKE_CURRENT_LIST_DIR}/caseopts.cmake)

# Inputs: input.* first, then extra inputs (input-N..., input_N...), in
# the languages this program tags
file(GLOB names RELATIVE ${CASE} ${CASE}/input*)
list(SORT names)
set(first "")
set(rest "")
foreach(name IN LISTS names)
  string(TOLOWER "${name}" lower)
  if(NOT lower MATCHES "\\.(c|c\\+\\+|cc|cp|cpp|cxx|h|h\\+\\+|hh|hp|hpp|hxx|inl|f|for|ftn|f77|f90|f95|f03|f08|f15)$")
    continue()
  endif()
  if(name MATCHES "^input\\.")
    list(APPEND first ${name})
  elseif(name MATCHES "^input[-_][0-9]")
    list(APPEND rest ${name})
  endif()
endforeach()
set(inputs ${first} ${rest})
if(NOT inputs)
  message("no inputs in a tagged language")
  cmake_language(EXIT 77)
endif()

set(lines "")
if(EXISTS ${CASE}/args.ctags)
  file(STRINGS ${CASE}/args.ctags lines)
endif()
ctags_caseopts("${lines}")

set(base "")
set(opts "")
if(MODE STREQUAL "allkinds")
  set(opts --kinds-C=* --kinds-C++=* --kinds-Fortran=*)
elseif(MODE STREQUAL "args")
  set(opts ${ctags_args})
  if(NOT opts)
    message("no --kinds or --extras options")
    cmake_language(EXIT 77)
  endif()
elseif(MODE STREQUAL "fmt")
  if(NOT ctags_fmt)
    message("no format options")
    cmake_language(EXIT 77)
  endif()
  set(base ${ctags_args})
  ctags_keyedopts(opts ${ctags_args} ${ctags_fmt})
endif()

file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})

# Tag the inputs with both tools, into PREFIXref.tags and PREFIXours.tags
function(ctags_runboth prefix)
  execute_process(
    COMMAND ${UCTAGS} --options=NONE --languages=C,C++,Fortran --fields=+n
            -f ${WORK}/${prefix}ref.tags ${ARGN} ${inputs}
    WORKING_DIRECTORY ${CASE}
    RESULT_VARIABLE status ERROR_VARIABLE err)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "Universal Ctags failed (${status}): ${err}")
  endif()
  execute_process(
    COMMAND ${OURS} ${ARGN} -f ${WORK}/${prefix}ours.tags ${inputs}
    WORKING_DIRECTORY ${CASE}
    RESULT_VARIABLE status ERROR_VARIABLE err)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "ctags failed (${status}): ${err}")
  endif()
endfunction()

set(diff "")
if(MODE STREQUAL "fmt")
  ctags_runboth(base- ${base})
  list(APPEND diff --base ${WORK}/base-ref.tags ${WORK}/base-ours.tags)
endif()
ctags_runboth("" ${opts})
list(FIND opts --sort=no sortno)
list(FIND opts -u unsorted)
if(sortno GREATER -1 OR unsorted GREATER -1)
  list(APPEND diff --ordered)
endif()

if(ONLY)
  list(APPEND diff --only ${ONLY})
endif()
execute_process(
  COMMAND ${TAGDIFF} --ledger ${LEDGER} --scope ${SCOPE} ${diff}
          ${WORK}/ref.tags ${WORK}/ours.tags
  RESULT_VARIABLE status)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "tags differ")
endif()
