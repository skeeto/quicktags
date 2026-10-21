# Check the platform layer's current directory, which the test platform
# only imitates, as the !_TAG_PROC_CWD line has it: run from the root of
# the work directory's file system (drive, share, or /), then from the
# work directory itself.
#   $ cmake -DCTAGS=ctags -DWORK=dir -P cwd_test.cmake
cmake_minimum_required(VERSION 3.21...4.4)

file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})
file(WRITE ${WORK}/a.c "int x;\n")

# Run ctags from DIR: the header must give WANT as its directory.
function(check dir want)
  file(REMOVE ${WORK}/tags)
  execute_process(COMMAND ${CTAGS} -f ${WORK}/tags ${WORK}/a.c
    WORKING_DIRECTORY ${dir} RESULT_VARIABLE status ERROR_VARIABLE err)
  set(lines "")
  if(EXISTS ${WORK}/tags)
    file(STRINGS ${WORK}/tags lines REGEX "^!_TAG_PROC_CWD\t")
  endif()
  set(expect "!_TAG_PROC_CWD\t${want}\t//")
  if(NOT status EQUAL 0 OR NOT lines STREQUAL expect)
    message(SEND_ERROR "from ${dir} (status ${status})\n"
      "  got:  ${lines}\n  want: ${expect}\n${err}")
  endif()
endfunction()

if(WORK MATCHES "^([A-Za-z]:/|//[^/]+/[^/]+/)")
  check(${CMAKE_MATCH_1} ${CMAKE_MATCH_1})
  check(${WORK} ${WORK}/)
else()
  check(/ /)
  file(REAL_PATH ${WORK} real)  # as getcwd() spells it
  check(${WORK} ${real}/)
endif()
