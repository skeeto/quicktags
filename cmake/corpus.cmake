# Differential tests on real source trees, opt-in:
#   $ cmake -B build -DCTAGS_CORPUS_LLAMA=~/src/llama.cpp -DCTAGS_CORPUS_LLVM=~/src/llvm-project
# llama.cpp is one test, corpus/llama; llvm-project one per top-level
# directory, corpus/llvm/NAME. Each runs Universal Ctags and ctags with
# -R from the tree's root and compares, against test/ledger.txt.

set(CTAGS_CORPUS_LLAMA "" CACHE PATH "llama.cpp source tree, for corpus tests")
set(CTAGS_CORPUS_LLVM  "" CACHE PATH "llvm-project source tree, for corpus tests")
set(CTAGS_CORPUS_ONLY  "" CACHE STRING
  "Compare only these kinds in corpus tests, e.g. C:d,C++:d (empty for all)")
option(CTAGS_CORPUS_REUSE_REF
  "Reuse Universal Ctags' output from an earlier run (faster; stale if the tree changes)" OFF)

function(ctags_corpus_test name root dir)
  add_test(NAME corpus/${name} COMMAND ${CMAKE_COMMAND}
    -DUCTAGS=${CTAGS_UCTAGS}
    -DOURS=$<TARGET_FILE:ctags>
    -DTAGDIFF=$<TARGET_FILE:tagdiff>
    -DROOT=${root}
    -DDIR=${dir}
    -DSCOPE=corpus/${name}
    -DONLY=${CTAGS_CORPUS_ONLY}
    -DREUSE=${CTAGS_CORPUS_REUSE_REF}
    -DLEDGER=${PROJECT_SOURCE_DIR}/test/ledger.txt
    -DWORK=${PROJECT_BINARY_DIR}/work/corpus/${name}
    -P ${PROJECT_SOURCE_DIR}/cmake/runcorpus.cmake)
  set_tests_properties(corpus/${name} PROPERTIES
    LABELS corpus TIMEOUT 1800 PROCESSORS 4)
endfunction()

if(CTAGS_CORPUS_LLAMA AND ctags_units_ok)
  ctags_corpus_test(llama ${CTAGS_CORPUS_LLAMA} .)
endif()

if(CTAGS_CORPUS_LLVM AND ctags_units_ok)
  file(GLOB tops LIST_DIRECTORIES true RELATIVE ${CTAGS_CORPUS_LLVM}
    ${CTAGS_CORPUS_LLVM}/*)
  foreach(top IN LISTS tops)
    if(IS_DIRECTORY ${CTAGS_CORPUS_LLVM}/${top} AND NOT top STREQUAL ".git")
      ctags_corpus_test(llvm/${top} ${CTAGS_CORPUS_LLVM} ${top})
    endif()
  endforeach()
endif()
