# Run ctags over a tree twice, with and without OPTS, and require the
# same tags file: the fast paths (--_nofast) and the thread count
# (--jobs=1) must not change the output. BASEOPTS are given both runs.
#   $ cmake -DOURS=ctags -DROOT=dir -DOPTS=--_nofast [-DBASEOPTS=options]
#           -DWORK=dir -P runsame.cmake
cmake_minimum_required(VERSION 3.21...4.4)

file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})
execute_process(COMMAND ${OURS} ${BASEOPTS} -R -f ${WORK}/a.tags
  WORKING_DIRECTORY ${ROOT} RESULT_VARIABLE status)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "ctags ${BASEOPTS} failed (${status})")
endif()
execute_process(COMMAND ${OURS} ${BASEOPTS} ${OPTS} -R -f ${WORK}/b.tags
  WORKING_DIRECTORY ${ROOT} RESULT_VARIABLE status)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "ctags ${BASEOPTS} ${OPTS} failed (${status})")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files
  ${WORK}/a.tags ${WORK}/b.tags RESULT_VARIABLE status)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "the tags differ with ${OPTS}")
endif()
