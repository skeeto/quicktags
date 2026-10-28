# Differential tests on real source trees, opt-in:
#   $ cmake -B build -DCTAGS_CORPUS_LLAMA=~/src/llama.cpp -DCTAGS_CORPUS_LLVM=~/src/llvm-project
# llama.cpp is one test, corpus/llama; llvm-project one per top-level
# directory, corpus/llvm/NAME. Each runs Universal Ctags and ctags with
# -R from the tree's root and compares, against test/ledger.txt. With
# format options, each is also corpus-fmt/NAME/SET (label corpus-fmt).

set(CTAGS_CORPUS_LLAMA "" CACHE PATH "llama.cpp source tree, for corpus tests")
set(CTAGS_CORPUS_LLVM  "" CACHE PATH "llvm-project source tree, for corpus tests")
set(CTAGS_CORPUS_ONLY  "" CACHE STRING
  "Compare only these kinds in corpus tests, e.g. C:d,C++:d (empty for all)")
option(CTAGS_CORPUS_REUSE_REF
  "Reuse Universal Ctags' output from an earlier run (faster; stale if the tree changes)" OFF)

# Format option sets, run with the label corpus-fmt, each compared tag
# by tag (tagdiff --base) against the default run of the same tree. The
# set NAME gives both tools the options ctags_fmt_NAME, and tagdiff the
# flags ctags_fmt_NAME_diff. Sets listed in ctags_fmt_sets run on
# llama.cpp, those in ctags_fmt_llvm on llvm-project. A set with -e also
# gets --tag-relative=no, so that TAGS paths start at the tree's root as
# the base's do (ctags_keyedopts in caseopts.cmake).
set(ctags_fmt_fields --fields=+iaSmlKzZEr)  # 1
set(ctags_fmt_combine --excmd=combine)   # 2
set(ctags_fmt_sortno --sort=no)          # 3, in the walk's order
set(ctags_fmt_sortno_diff --ordered)
set(ctags_fmt_etags -e)                  # 4, Emacs tags in the walk's order
set(ctags_fmt_etags_diff --etags --ordered)
set(ctags_fmt_foldcase --sort=foldcase)  # 5, ours in folded order
set(ctags_fmt_foldcase_diff --folded)
set(ctags_fmt_sets fields combine sortno etags foldcase)
set(ctags_fmt_llvm fields combine sortno)

# A list as one argument of add_test
function(ctags_listarg var)
  string(REPLACE ";" "$<SEMICOLON>" r "${ARGN}")
  set(${var} "${r}" PARENT_SCOPE)
endfunction()

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

# The tree as corpus/NAME, with the option set OPTSET, based on the run
# of corpus/NAME, which goes first when both run
function(ctags_corpus_fmt_test name root dir optset)
  set(test corpus-fmt/${name}/${optset})
  ctags_listarg(args ${ctags_fmt_${optset}})
  ctags_listarg(diff ${ctags_fmt_${optset}_diff})
  add_test(NAME ${test} COMMAND ${CMAKE_COMMAND}
    -DUCTAGS=${CTAGS_UCTAGS}
    -DOURS=$<TARGET_FILE:ctags>
    -DTAGDIFF=$<TARGET_FILE:tagdiff>
    -DROOT=${root}
    -DDIR=${dir}
    "-DARGS=${args}"
    "-DDIFF=${diff}"
    -DBASE=${PROJECT_BINARY_DIR}/work/corpus/${name}
    -DSCOPE=${test}
    -DONLY=${CTAGS_CORPUS_ONLY}
    -DREUSE=${CTAGS_CORPUS_REUSE_REF}
    -DLEDGER=${PROJECT_SOURCE_DIR}/test/ledger.txt
    -DWORK=${PROJECT_BINARY_DIR}/work/${test}
    -P ${PROJECT_SOURCE_DIR}/cmake/runcorpus.cmake)
  set_tests_properties(${test} PROPERTIES
    LABELS "corpus;corpus-fmt" TIMEOUT 1800 PROCESSORS 4
    DEPENDS corpus/${name})
endfunction()

if(CTAGS_CORPUS_LLAMA AND ctags_units_ok)
  ctags_corpus_test(llama ${CTAGS_CORPUS_LLAMA} .)
  foreach(optset IN LISTS ctags_fmt_sets)
    ctags_corpus_fmt_test(llama ${CTAGS_CORPUS_LLAMA} . ${optset})
  endforeach()
endif()

if(CTAGS_CORPUS_LLVM AND ctags_units_ok)
  file(GLOB tops LIST_DIRECTORIES true RELATIVE ${CTAGS_CORPUS_LLVM}
    ${CTAGS_CORPUS_LLVM}/*)
  foreach(top IN LISTS tops)
    if(IS_DIRECTORY ${CTAGS_CORPUS_LLVM}/${top} AND NOT top STREQUAL ".git")
      ctags_corpus_test(llvm/${top} ${CTAGS_CORPUS_LLVM} ${top})
      foreach(optset IN LISTS ctags_fmt_llvm)
        ctags_corpus_fmt_test(llvm/${top} ${CTAGS_CORPUS_LLVM} ${top} ${optset})
      endforeach()
    endif()
  endforeach()
endif()

# The fast paths and threads must not change the output (label "same"),
# also with the options after OPTS given to both runs
function(ctags_same_test name root opts)
  ctags_listarg(base ${ARGN})
  add_test(NAME same/${name} COMMAND ${CMAKE_COMMAND}
    -DOURS=$<TARGET_FILE:ctags> -DROOT=${root} -DOPTS=${opts}
    "-DBASEOPTS=${base}"
    -DWORK=${PROJECT_BINARY_DIR}/work/same/${name}
    -P ${PROJECT_SOURCE_DIR}/cmake/runsame.cmake)
  set_tests_properties(same/${name} PROPERTIES
    LABELS "corpus;same" TIMEOUT 1800 PROCESSORS 8)
endfunction()

# Fields built from token runs (signatures, base classes) too, the
# unsorted and folded orders, with any number of threads, and each level
# of vector scanning below the processor's
set(ctags_same_fields --fields=+iaSm)
if(CTAGS_CORPUS_LLAMA)
  ctags_same_test(llama/nofast ${CTAGS_CORPUS_LLAMA} --_nofast)
  ctags_same_test(llama/jobs1  ${CTAGS_CORPUS_LLAMA} --jobs=1)
  ctags_same_test(llama/nofast-fields ${CTAGS_CORPUS_LLAMA} --_nofast
    ${ctags_same_fields})
  ctags_same_test(llama/sortno ${CTAGS_CORPUS_LLAMA} --jobs=1 --sort=no)
  ctags_same_test(llama/foldcase ${CTAGS_CORPUS_LLAMA} --jobs=1
    --sort=foldcase)
  ctags_same_test(llama/etags ${CTAGS_CORPUS_LLAMA} --jobs=1 -e)
  ctags_same_test(llama/simd0 ${CTAGS_CORPUS_LLAMA} --_simd=0)
  ctags_same_test(llama/simd1 ${CTAGS_CORPUS_LLAMA} --_simd=1)
endif()
if(CTAGS_CORPUS_LLVM)
  ctags_same_test(llvm/nofast ${CTAGS_CORPUS_LLVM} --_nofast)
  ctags_same_test(llvm/jobs1  ${CTAGS_CORPUS_LLVM} --jobs=1)
  ctags_same_test(llvm/nofast-fields ${CTAGS_CORPUS_LLVM} --_nofast
    ${ctags_same_fields})
  ctags_same_test(llvm/sortno ${CTAGS_CORPUS_LLVM} --jobs=1 --sort=no)
  ctags_same_test(llvm/etags ${CTAGS_CORPUS_LLVM} --jobs=1 -e)
  ctags_same_test(llvm/simd0 ${CTAGS_CORPUS_LLVM} --_simd=0)
  ctags_same_test(llvm/simd1 ${CTAGS_CORPUS_LLVM} --_simd=1)
endif()
