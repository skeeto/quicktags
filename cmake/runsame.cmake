# Run ctags over a tree twice, with and without OPTS, and require the
# same tags file: the fast paths (--_nofast) and the thread count
# (--jobs=1) must not change the output.
#   $ cmake -DOURS=ctags -DROOT=dir -DOPTS=--_nofast -DWORK=dir -P runsame.cmake

file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})
execute_process(COMMAND ${OURS} -R -f ${WORK}/a.tags
  WORKING_DIRECTORY ${ROOT} RESULT_VARIABLE status)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "ctags failed (${status})")
endif()
execute_process(COMMAND ${OURS} ${OPTS} -R -f ${WORK}/b.tags
  WORKING_DIRECTORY ${ROOT} RESULT_VARIABLE status)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "ctags ${OPTS} failed (${status})")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files
  ${WORK}/a.tags ${WORK}/b.tags RESULT_VARIABLE status)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "the tags differ with ${OPTS}")
endif()
