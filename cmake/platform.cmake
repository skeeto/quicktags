# Check the platform layer's reading of files through the real program,
# with -I lists: a device reads as empty, and a file that cannot be read
# gets the reason Universal Ctags' C runtime gives on Windows.
#   $ cmake -DCTAGS=ctags.exe -DWORK=dir [-DWINDOWS=1] -P platform.cmake

file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK}/dir)
file(WRITE ${WORK}/list "X\n")
file(WRITE ${WORK}/t.c "X int a;\n")

set(failed 0)
function(check arg want)
  execute_process(COMMAND ${CTAGS} -f - -I ${arg} t.c
    WORKING_DIRECTORY ${WORK} OUTPUT_VARIABLE out ERROR_VARIABLE err)
  string(STRIP "${out}${err}" got)
  if(NOT got STREQUAL want)
    message("-I ${arg}\n  got:  ${got}\n  want: ${want}")
    set(failed 1 PARENT_SCOPE)
  endif()
endfunction()

set(tag "a\tt.c\t/^X int a;$/;\"\tv\tline:1\ttyperef:typename:")
check(@list "${tag}int")
if(WINDOWS)
  check(@NUL "${tag}X int")
  check(./nul "${tag}X int")
  check(@list. "${tag}int")  # opened as "list", as the C runtime does
  check(@dir "ctags: cannot open \"dir\" : Permission denied")
  check(@dir/ "ctags: cannot open \"dir/\" : Permission denied")
  check(@list/ "ctags: cannot open \"list/\" : Invalid argument")
  check(@a*b "ctags: cannot open \"a*b\" : Invalid argument")
  check(@nodir/x "ctags: cannot open \"nodir/x\" : No such file or directory")
else()
  check(/dev/null "${tag}X int")
endif()

if(failed)
  message(FATAL_ERROR "the program read files unlike Universal Ctags")
endif()
