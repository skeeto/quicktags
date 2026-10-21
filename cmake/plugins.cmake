# Editor plugins' and build scripts' own ctags command lines, run on the
# sample tree test/plugins with Universal Ctags and with ctags, and
# compared: one test per tool, plugins/TOOL, label plugins. See
# runplugins.cmake. Differences must be explained in test/ledger.txt.

if(NOT ctags_units_ok)
  return()  # no Universal Ctags 6.2.1 (units.cmake)
endif()

set(ctags_plugins
  version tagbar vista ctrlp taglist fzf-btags fzf-tags fzf-lua-btags
  leaderf easytags gutentags gutentags-win kernel omnicppcomplete kate
  emacs)
foreach(tool IN LISTS ctags_plugins)
  add_test(NAME plugins/${tool} COMMAND ${CMAKE_COMMAND}
    -DTOOL=${tool}
    -DUCTAGS=${CTAGS_UCTAGS}
    -DOURS=$<TARGET_FILE:ctags>
    -DTAGDIFF=$<TARGET_FILE:tagdiff>
    -DTREE=${PROJECT_SOURCE_DIR}/test/plugins
    -DLEDGER=${PROJECT_SOURCE_DIR}/test/ledger.txt
    -DWORK=${PROJECT_BINARY_DIR}/work/plugins/${tool}
    -DWINDOWS=${WIN32}
    -P ${PROJECT_SOURCE_DIR}/cmake/runplugins.cmake)
  set_tests_properties(plugins/${tool} PROPERTIES
    LABELS plugins SKIP_RETURN_CODE 77 TIMEOUT 60)
endforeach()
