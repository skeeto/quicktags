# Check that a Windows program imports exactly the listed functions, all
# from KERNEL32.dll: no C runtime, no toolchain DLLs.
#   $ cmake -DOBJDUMP=objdump -DEXE=ctags.exe -DWANT=test/imports.txt -P imports.cmake
execute_process(COMMAND ${OBJDUMP} -p ${EXE}
  OUTPUT_VARIABLE dump RESULT_VARIABLE status)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "objdump failed on ${EXE}")
endif()

string(REPLACE "\n" ";" lines "${dump}")
set(dlls "")
set(funcs "")
foreach(line IN LISTS lines)
  if(line MATCHES "DLL Name: (.+)$")
    list(APPEND dlls "${CMAKE_MATCH_1}")
  elseif(line MATCHES "^\t[0-9a-f]+ +<none> +[0-9a-f]+ +([A-Za-z0-9_]+)$")
    list(APPEND funcs "${CMAKE_MATCH_1}")
  endif()
endforeach()

if(NOT dlls STREQUAL "KERNEL32.dll")
  message(FATAL_ERROR "imports from ${dlls}, not just KERNEL32.dll")
endif()

file(STRINGS ${WANT} want)
list(SORT funcs)
list(SORT want)
if(NOT funcs STREQUAL want)
  list(JOIN funcs " " got)
  list(JOIN want " " expected)
  message(FATAL_ERROR "imports differ\n  got:  ${got}\n  want: ${expected}")
endif()
