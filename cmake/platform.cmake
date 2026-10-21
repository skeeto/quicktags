# Check the platform layer's reading of files through the real program,
# with -I lists: a device reads as empty, and a file that cannot be read
# gets the reason Universal Ctags' C runtime gives on Windows. Then its
# current directory, through --tag-relative.
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

# --tag-relative through the real current directory (OPT-18): the file
# field of u.c's tag, run in DIR with -f OUT
file(MAKE_DIRECTORY ${WORK}/sub)
file(WRITE ${WORK}/u.c "int u;\n")
function(checkrel dir out want)
  execute_process(COMMAND ${CTAGS} -f ${out} ${ARGN}
    WORKING_DIRECTORY ${dir} OUTPUT_VARIABLE tags ERROR_VARIABLE err)
  get_filename_component(path ${out} ABSOLUTE BASE_DIR ${dir})
  if(NOT out STREQUAL "-" AND EXISTS ${path})
    file(READ ${path} tags)
    file(REMOVE ${path})
  endif()
  set(got "")
  if(tags MATCHES "(^|\n)u\t([^\t]*)\t")
    set(got "${CMAKE_MATCH_2}")
  endif()
  string(STRIP "${got}${err}" got)
  if(NOT got STREQUAL want)
    string(JOIN " " args ${ARGN})
    message("in ${dir}: -f ${out} ${args}\n  got:  ${got}\n  want: ${want}")
    set(failed 1 PARENT_SCOPE)
  endif()
endfunction()

# WORK made absolute by the program: from the system, which on POSIX
# resolves links, and on Windows with its drive letter uppercased
set(abs ${WORK})
if(WINDOWS)
  string(SUBSTRING ${WORK} 0 1 drive)
  string(SUBSTRING ${WORK} 1 -1 tail)
  string(TOUPPER ${drive} drive)
  set(abs ${drive}${tail})
else()
  file(REAL_PATH ${WORK} abs)
endif()
checkrel(${WORK} sub/tags "../u.c" --tag-relative u.c)
checkrel(${WORK} - "u.c" --tag-relative=always ${WORK}/u.c)
checkrel(${WORK} - "${abs}/u.c" --tag-relative=never ./sub/../u.c)

# From the root of WORK's drive or file system, which Windows names with
# its separator, "C:\", unlike any other directory
string(REGEX MATCH "^([A-Za-z]:)?/" root ${WORK})
string(LENGTH "${root}" len)
string(SUBSTRING ${WORK} ${len} -1 rel)  # WORK from there
string(SUBSTRING ${abs} 0 ${len} absroot)
set(abs ${absroot}${rel})
checkrel(${root} - "${abs}/u.c" --tag-relative=never ${rel}/u.c)
checkrel(${root} - "${rel}/u.c" --tag-relative ./${rel}/u.c)
checkrel(${root} - "${rel}/u.c" --tag-relative=always ${WORK}/u.c)
checkrel(${root} ${rel}/sub/tags "../u.c" --tag-relative=always ${WORK}/u.c)

if(failed)
  message(FATAL_ERROR "the program read files or made paths unlike Universal Ctags")
endif()
