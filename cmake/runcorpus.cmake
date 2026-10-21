# Run Universal Ctags and ctags with -R on a directory of a source tree,
# from the tree's root, and compare.
#   $ cmake -DUCTAGS=... -DOURS=... -DTAGDIFF=... -DROOT=tree -DDIR=subdir
#           -DSCOPE=name -DLEDGER=file -DWORK=dir [-DONLY=kinds] [-DREUSE=ON]
#           [-DARGS=options] [-DDIFF=flags] [-DBASE=dir] -P runcorpus.cmake
# ARGS are options for both tools, and DIFF flags for tagdiff. With BASE,
# the work directory of a run without ARGS, its ref.tags and ours.tags
# are the base of a keyed comparison (tagdiff --base): only the tags on
# which they agree are compared. If they are missing, or ctags is newer,
# the base is made here instead. ARGS then name files as the base does
# (ctags_keyedopts).
cmake_minimum_required(VERSION 3.21...4.4)

include(${CMAKE_CURRENT_LIST_DIR}/caseopts.cmake)

# Tag the tree with both tools, into OUT/ref.tags and OUT/ours.tags,
# keeping an earlier ref.tags if asked to reuse it
function(ctags_runboth out reuse)
  file(MAKE_DIRECTORY ${out})
  if(NOT reuse OR NOT EXISTS ${out}/ref.tags)
    execute_process(
      COMMAND ${UCTAGS} --options=NONE --languages=C,C++,Fortran --fields=+n
              ${ARGN} -R -f ${out}/ref.tags ${DIR}
      WORKING_DIRECTORY ${ROOT}
      RESULT_VARIABLE status ERROR_QUIET)
    if(NOT status EQUAL 0)
      message(FATAL_ERROR "Universal Ctags failed (${status})")
    endif()
  endif()
  execute_process(
    COMMAND ${OURS} ${ARGN} -R -f ${out}/ours.tags ${DIR}
    WORKING_DIRECTORY ${ROOT}
    RESULT_VARIABLE status ERROR_VARIABLE err)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "ctags failed (${status}): ${err}")
  endif()
endfunction()

set(diff ${DIFF})
set(args ${ARGS})
if(BASE)
  set(base ${BASE})
  if(NOT EXISTS ${BASE}/ref.tags OR "${OURS}" IS_NEWER_THAN ${BASE}/ours.tags)
    set(base ${WORK}/base)
    ctags_runboth(${base} "${REUSE}")
  endif()
  list(APPEND diff --base ${base}/ref.tags ${base}/ours.tags)
  ctags_keyedopts(args ${ARGS})
endif()
ctags_runboth(${WORK} "${REUSE}" ${args})

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
