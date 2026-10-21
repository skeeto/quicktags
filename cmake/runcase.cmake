# Run one Units case through Universal Ctags and ctags, and compare.
#   $ cmake -DUCTAGS=... -DOURS=... -DTAGDIFF=... -DCASE=dir -DMODE=default
#           -DSCOPE=name -DLEDGER=file -DWORK=dir [-DONLY=kinds] -P runcase.cmake
# Exits 77 (skipped) when the mode does not apply to the case.

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

# Options: the case's --kinds options are kept for the "args" mode, and
# the rest ignored, as features this program does not have
set(opts "")
if(MODE STREQUAL "allkinds")
  set(opts --kinds-C=* --kinds-C++=* --kinds-Fortran=*)
elseif(MODE STREQUAL "args")
  if(EXISTS ${CASE}/args.ctags)
    file(STRINGS ${CASE}/args.ctags lines)
    foreach(line IN LISTS lines)
      string(STRIP "${line}" line)
      string(TOLOWER "${line}" lower)
      if(lower MATCHES "^--(kinds-(c|c\\+\\+|fortran)|(c|c\\+\\+|fortran)-(kinds|types))=")
        list(APPEND opts "${line}")
      endif()
    endforeach()
  endif()
  if(NOT opts)
    message("no --kinds options")
    cmake_language(EXIT 77)
  endif()
endif()

file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})
execute_process(
  COMMAND ${UCTAGS} --options=NONE --languages=C,C++,Fortran --fields=+n
          -f ${WORK}/ref.tags ${opts} ${inputs}
  WORKING_DIRECTORY ${CASE}
  RESULT_VARIABLE status ERROR_VARIABLE err)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "Universal Ctags failed (${status}): ${err}")
endif()
execute_process(
  COMMAND ${OURS} ${opts} -f ${WORK}/ours.tags ${inputs}
  WORKING_DIRECTORY ${CASE}
  RESULT_VARIABLE status ERROR_VARIABLE err)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "ctags failed (${status}): ${err}")
endif()

set(only "")
if(ONLY)
  set(only --only ${ONLY})
endif()
execute_process(
  COMMAND ${TAGDIFF} --ledger ${LEDGER} --scope ${SCOPE} ${only}
          ${WORK}/ref.tags ${WORK}/ours.tags
  RESULT_VARIABLE status)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "tags differ")
endif()
