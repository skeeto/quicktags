# Which options of a Units case (its args.ctags) the differential tests
# give both tools. Options for what this program does not do are first
# stripped. Of the rest, the "args" mode keeps those that change which
# tags exist, and the "fmt" mode those that change only how tags are
# written. The lists of regular expressions below grow as features land.
# Also the options of any run compared by key with a base run.

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

# Stripped: comments, and options for features not ported, but for the
# C preprocessor's ignore parameter, which is -I's
set(ctags_strip
  "^#" "^--fields-" "^--extras-" "^--param-" "^-D" "^--if0"
  "^--line-directives" "^--roles-" "^--langdef" "^--kinddef-"
  "^--regex-" "^--mline-regex-" "^--_")
ctags_nocase(ctags_macroparam "^--param-cpreprocessor")
string(APPEND ctags_macroparam "[.:]ignore=")
# Also stripped: --map-L, --kinds-L and --language-force=L, where L is a
# language other than these
set(ctags_langs "c|c\\+\\+|fortran")

# Kept by "args": --kinds-L, --L-kinds and --L-types (any case), --extras
# without the reference (r) and guest (g) extras, the options that
# choose languages: --langmap, --map-L, --languages, --language-force=L
# (others' already stripped) and -h, and -I
set(ctags_kinds "kinds-(${ctags_langs})|(${ctags_langs})-(kinds|types)")
ctags_nocase(ctags_kinds "^--(${ctags_kinds})=")
ctags_nocase(ctags_maps "^--map-(${ctags_langs})=")
set(ctags_args_keep "${ctags_kinds}" "^--extras?=" "${ctags_maps}"
  "^--langmap=" "^--languages=" "^--language-force=" "^-h"
  "^-I" "${ctags_macroparam}")

# Kept by "fmt": --fields (filtered below), --excmd, -n, -N, --format,
# --pattern-length-limit, --output-format=e-ctags, --sort and -u
set(ctags_fmt_keep "^--fields=" "^--excmd=" "^-[nN]$" "^--format="
  "^--pattern-length-limit=" "^--output-format=e-ctags$" "^--sort="
  "^-u$")

# Letters and {names} filtered out of --fields: e and o never print in
# tags output here
set(ctags_fields_strip e o {end} {nth})
# Every field letter, which a leading * enables (u-ctags --list-fields)
set(ctags_fields_all N F P C E K R S T Z a e f i k l m n o p r s t x z)

# Is the option stripped from both command lines?
function(ctags_stripped var opt)
  set(r OFF)
  foreach(re IN LISTS ctags_strip)
    if(opt MATCHES "${re}")
      set(r ON)
    endif()
  endforeach()
  if(opt MATCHES "${ctags_macroparam}")
    set(r OFF)
  endif()
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

# A --fields spec that enables the same fields, but none of those in
# ctags_fields_strip: "" for a relative spec left empty, which is
# dropped. An absolute spec (one without a leading sign) stays absolute,
# with its leading removals dropped, since they remove from nothing, and
# "--fields=" when left empty. A leading * becomes the letters it keeps.
function(ctags_filterfields var spec)
  set(out "")
  set(mode "+")
  set(written "")
  set(absolute OFF)
  if(NOT spec MATCHES "^[-+]")
    set(absolute ON)
    set(written "+")
  endif()
  if(spec MATCHES "^\\*(.*)$")
    set(spec "${CMAKE_MATCH_1}")
    foreach(tok IN LISTS ctags_fields_all)
      list(FIND ctags_fields_strip "${tok}" at)
      if(at EQUAL -1)
        string(APPEND out "${tok}")
      endif()
    endforeach()
  endif()
  string(REGEX MATCHALL "\\{[^}]*\\}|." tokens "${spec}")
  foreach(tok IN LISTS tokens)
    list(FIND ctags_fields_strip "${tok}" at)
    if(tok STREQUAL "+" OR tok STREQUAL "-")
      set(mode "${tok}")
    elseif(at EQUAL -1 AND NOT (absolute AND out STREQUAL "" AND mode STREQUAL "-"))
      if(NOT mode STREQUAL written)
        string(APPEND out "${mode}")
        set(written "${mode}")
      endif()
      string(APPEND out "${tok}")
    endif()
  endforeach()
  if(absolute OR NOT out STREQUAL "")
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

# The options after VAR, for a run compared by key (tagdiff --base) with
# a base run, whose keys name files as given, from the directory both
# run in. Etags output (-e) names them from the TAGS file's directory
# instead, which is elsewhere, so --tag-relative=no follows it.
function(ctags_keyedopts var)
  set(r ${ARGN})
  list(FIND r -e short)
  list(FIND r --output-format=etags long)
  if(short GREATER -1 OR long GREATER -1)
    list(APPEND r --tag-relative=no)
  endif()
  set(${var} "${r}" PARENT_SCOPE)
endfunction()

# Sort a case's option lines into ctags_args and ctags_fmt, the options
# each mode keeps, in order.
function(ctags_caseopts lines)
  set(args "")
  set(fmt "")
  set(param OFF)  # the next line is the parameter of a kept -h
  foreach(line IN LISTS lines)
    string(STRIP "${line}" line)
    if(param)
      list(APPEND args "${line}")
      set(param OFF)
      continue()
    endif()
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
        if(mode STREQUAL "args" AND opt STREQUAL "-h")
          set(param ON)
        endif()
      endif()
    endforeach()
  endforeach()
  set(ctags_args "${args}" PARENT_SCOPE)
  set(ctags_fmt "${fmt}" PARENT_SCOPE)
endfunction()
