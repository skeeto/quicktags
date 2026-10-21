# Run Universal Ctags and ctags with -R on a directory of a source tree,
# from the tree's root, and compare.
#   $ cmake -DUCTAGS=... -DOURS=... -DTAGDIFF=... -DROOT=tree -DDIR=subdir
#           -DSCOPE=name -DLEDGER=file -DWORK=dir [-DONLY=kinds] [-DREUSE=ON]
#           -P runcorpus.cmake

file(MAKE_DIRECTORY ${WORK})
if(NOT REUSE OR NOT EXISTS ${WORK}/ref.tags)
  execute_process(
    COMMAND ${UCTAGS} --options=NONE --languages=C,C++,Fortran --fields=+n
            -R -f ${WORK}/ref.tags ${DIR}
    WORKING_DIRECTORY ${ROOT}
    RESULT_VARIABLE status ERROR_QUIET)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "Universal Ctags failed (${status})")
  endif()
endif()

execute_process(
  COMMAND ${OURS} -R -f ${WORK}/ours.tags ${DIR}
  WORKING_DIRECTORY ${ROOT}
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
