# Which options of a Units case (its args.ctags) the differential tests
# give both tools. Options for what this program does not do are first
# stripped. Of the rest, the "args" mode keeps those that change which
# tags exist, and the "fmt" mode those that change only how tags are
# written. The lists of regular expressions below grow as features land.

# A regular expression matching regardless of case, from one written in
# lowercase without character classes or escaped letters.
function(ctags_nocase var regex)
  set(r "")
  string(LENGTH "${regex}" len)
  foreach(i RANGE 1 ${len})
    math(EXPR at "${i} - 1")
    string(SUBSTRING "${regex}" ${at} 1 c)
    if(c MATCHES "[a-z]")
      string(TOUPPER "${c}" upper)
      string(APPEND r "[${c}${upper}]")
    else()
      string(APPEND r "${c}")
    endif()
  endforeach()
  set(${var} "${r}" PARENT_SCOPE)
endfunction()

# Stripped: comments, and options for features not ported
set(ctags_strip
  "^#" "^--fields-" "^--extras-" "^--param-" "^-D" "^--if0"
  "^--line-directives" "^--roles-" "^--langdef" "^--kinddef-"
  "^--regex-" "^--mline-regex-" "^--_")
# Also stripped: --map-L, --kinds-L and --language-force=L, where L is a
# language other than these
set(ctags_langs "c|c\\+\\+|fortran")

# Kept by "args": --kinds-L, --L-kinds and --L-types (any case), and
# --extras without the reference (r) and guest (g) extras
set(ctags_kinds "kinds-(${ctags_langs})|(${ctags_langs})-(kinds|types)")
ctags_nocase(ctags_kinds "^--(${ctags_kinds})=")
set(ctags_args_keep "${ctags_kinds}" "^--extras?=")

# Kept by "fmt"
set(ctags_fmt_keep "")

# Letters and {names} filtered out of --fields: e and o never print in
# tags output here, and the rest until their fields are ported
set(ctags_fields_strip e o {end} {nth}
  m {implementation} S a i {signature} {access} {inherits})

# Is the option stripped from both command lines?
function(ctags_stripped var opt)
  set(r OFF)
  foreach(re IN LISTS ctags_strip)
    if(opt MATCHES "${re}")
      set(r ON)
    endif()
  endforeach()
  string(TOLOWER "${opt}" lower)
  set(lang "")
  if(lower MATCHES "^--(map|kinds)-([^=]*)=")
    set(lang "${CMAKE_MATCH_2}")
  elseif(lower MATCHES "^--language-force=(.*)$")
    set(lang "${CMAKE_MATCH_1}")
  endif()
  if(NOT lang STREQUAL "" AND NOT lang MATCHES "^(${ctags_langs})$")
    set(r ON)
  endif()
  set(${var} ${r} PARENT_SCOPE)
endfunction()

# A --fields spec without the letters in ctags_fields_strip, signs
# merged: "" for a relative spec left empty, which is dropped, and an
# absolute one left empty stays "--fields=".
function(ctags_filterfields var spec)
  string(REGEX MATCHALL "\\{[^}]*\\}|." tokens "${spec}")
  set(out "")
  set(mode "")
  set(written "")
  set(any OFF)
  foreach(tok IN LISTS tokens)
    list(FIND ctags_fields_strip "${tok}" at)
    if(tok STREQUAL "+" OR tok STREQUAL "-")
      set(mode "${tok}")
    elseif(at EQUAL -1)
      if(NOT mode STREQUAL written)
        string(APPEND out "${mode}")
        set(written "${mode}")
      endif()
      string(APPEND out "${tok}")
      set(any ON)
    endif()
  endforeach()
  if(any OR NOT spec MATCHES "^[-+]")
    set(${var} "--fields=${out}" PARENT_SCOPE)
  else()
    set(${var} "" PARENT_SCOPE)
  endif()
endfunction()

# An option as kept, filtered, or "" if dropped.
function(ctags_keepopt var opt)
  if(opt MATCHES "^(--extras?=)(.*)$")
    set(name "${CMAKE_MATCH_1}")
    set(spec "${CMAKE_MATCH_2}")
    if(NOT spec MATCHES "{")
      string(REGEX REPLACE "[rg]" "" spec "${spec}")
    endif()
    set(opt "${name}${spec}")
    if(spec STREQUAL "+")
      set(opt "")
    endif()
  elseif(opt MATCHES "^--fields=(.*)$")
    ctags_filterfields(opt "${CMAKE_MATCH_1}")
  endif()
  set(${var} "${opt}" PARENT_SCOPE)
endfunction()

# Sort a case's option lines into ctags_args and ctags_fmt, the options
# each mode keeps, in order.
function(ctags_caseopts lines)
  set(args "")
  set(fmt "")
  foreach(line IN LISTS lines)
    string(STRIP "${line}" line)
    ctags_stripped(stripped "${line}")
    if(stripped OR line STREQUAL "")
      continue()
    endif()
    foreach(mode IN ITEMS args fmt)
      set(kept OFF)
      foreach(re IN LISTS ctags_${mode}_keep)
        if(line MATCHES "${re}")
          set(kept ON)
        endif()
      endforeach()
      if(kept)
        ctags_keepopt(opt "${line}")
        if(NOT opt STREQUAL "")
          list(APPEND ${mode} "${opt}")
        endif()
      endif()
    endforeach()
  endforeach()
  set(ctags_args "${args}" PARENT_SCOPE)
  set(ctags_fmt "${fmt}" PARENT_SCOPE)
endfunction()
