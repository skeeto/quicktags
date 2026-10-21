# Tests of tagdiff on small fixtures: each comparison's exit status, and
# what its output must say.
#   $ cmake -DTAGDIFF=tagdiff -DWORK=dir -P tagdiff_test.cmake
#
# In fixture lines, "|" stands for a tab, "~" for ;" and <DEL>, <SOH>
# and <FF> for those bytes.

file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})
string(ASCII 127 ctags_del)
string(ASCII 1 ctags_soh)
string(ASCII 12 ctags_ff)
set(failed 0)

function(ctags_unescape var text)
  string(REPLACE "|" "\t" text "${text}")
  string(REPLACE "~" ";\"" text "${text}")
  string(REPLACE "<DEL>" "${ctags_del}" text "${text}")
  string(REPLACE "<SOH>" "${ctags_soh}" text "${text}")
  string(REPLACE "<FF>" "${ctags_ff}" text "${text}")
  set(${var} "${text}" PARENT_SCOPE)
endfunction()

# Write a file of the lines given, with LF newlines (file(WRITE) makes
# them CRLF on Windows).
function(fixture name)
  set(text "")
  foreach(line IN LISTS ARGN)
    string(APPEND text "${line}\n")
  endforeach()
  ctags_unescape(text "${text}")
  file(CONFIGURE OUTPUT ${WORK}/${name} CONTENT "${text}"
    @ONLY NEWLINE_STYLE LF)
endfunction()

# Append to a TAGS file's text, in VAR, a section of the entries given.
function(etagsection var path)
  set(section "")
  foreach(line IN LISTS ARGN)
    string(APPEND section "${line}\n")
  endforeach()
  ctags_unescape(section "${section}")
  string(LENGTH "${section}" size)
  set(${var} "${${var}}${ctags_ff}\n${path},${size}\n${section}" PARENT_SCOPE)
endfunction()

function(etags name text)
  file(CONFIGURE OUTPUT ${WORK}/${name} CONTENT "${text}"
    @ONLY NEWLINE_STYLE LF)
endfunction()

# Run tagdiff with the arguments before MATCH: it must exit with STATUS
# and print something matching each regular expression after MATCH.
function(check name status)
  cmake_parse_arguments(PARSE_ARGV 2 c "" "" "MATCH")
  execute_process(COMMAND ${TAGDIFF} ${c_UNPARSED_ARGUMENTS}
    WORKING_DIRECTORY ${WORK}
    RESULT_VARIABLE got OUTPUT_VARIABLE out ERROR_VARIABLE err)
  set(ok ON)
  if(NOT got EQUAL status)
    set(ok OFF)
  endif()
  foreach(re IN LISTS c_MATCH)
    if(NOT out MATCHES "${re}")
      set(ok OFF)
    endif()
  endforeach()
  if(NOT ok)
    message("FAIL: ${name}: status ${got}, want ${status}\n${out}${err}")
    math(EXPR n "${failed} + 1")
    set(failed ${n} PARENT_SCOPE)
  endif()
endfunction()


# Kinds by letter, by long name, and after kind:, for --only
fixture(kinds-ref.tags
  "f|x.c|/^f$/~|kind:function|line:1"
  "k|y.f90|/^k$/~|component|line:1"
  "v|x.c|/^v$/~|kind:v|line:2"
  "w|x.c|/^w$/~|v|line:4")
fixture(kinds-ours.tags
  "k|y.f90|/^k$/~|component|line:1"
  "v|x.c|/^v$/~|kind:v|line:3")
check(only-long 0 --only Fortran:k kinds-ref.tags kinds-ours.tags
  MATCH "1 ref, 1 ours, 0 missing")
check(only-kind-long 1 --only C:f kinds-ref.tags kinds-ours.tags
  MATCH "1 ref, 0 ours, 1 missing")
check(only-kind-letter 1 --only C:v kinds-ref.tags kinds-ours.tags
  MATCH "2 ref, 1 ours, 2 missing, 1 extra")

# A stale ledger entry fails
fixture(stale.ledger
  "@ s: a line that does not occur"
  "- q|x.c|/^q$/~|v|line:9")
check(stale 1 --ledger stale.ledger --scope s kinds-ours.tags kinds-ours.tags
  MATCH "stale ledger entry")


# anon-namespace-file: file: in its place, and fileScope in the extras
fixture(anon.ledger
  "@ *: P10"
  "! anon-namespace-file")
fixture(anon-ref.tags
  "!_TAG_FIELD_DESCRIPTION|extras|/Extra tag type information/"
  "!_TAG_FIELD_DESCRIPTION|file|/File-restricted scoping/"
  "g|x.cpp|/^int g()$/~|kind:function|line:4|scope:namespace:__anon0123456789ab11|typeref:typename:int|signature:()|roles:def|end:4"
  "s|x.cpp|/^int s$/~|kind:variable|line:5|namespace:__anon0123456789ab11|roles:def|extras:anonymous|end:5"
  "t|x.cpp|/^int t$/~|kind:variable|line:6|namespace:__anon0123456789ab11|file:|roles:def|extras:fileScope|end:6"
  "u|x.cpp|/^int u$/~|kind:variable|line:7|typeref:struct:__anon0123456789ab11::S|roles:def|end:7"
  "h|x.h|/^int h()$/~|kind:function|line:1|namespace:__anon0123456789ab11|roles:def|end:1")
fixture(anon-ours.tags
  "g|x.cpp|/^int g()$/~|kind:function|line:4|scope:namespace:__anon0123456789ab11|typeref:typename:int|file:|signature:()|roles:def|extras:fileScope|end:4"
  "s|x.cpp|/^int s$/~|kind:variable|line:5|namespace:__anon0123456789ab11|file:|roles:def|extras:fileScope,anonymous|end:5"
  "t|x.cpp|/^int t$/~|kind:variable|line:6|namespace:__anon0123456789ab11|file:|roles:def|extras:fileScope|end:6"
  "u|x.cpp|/^int u$/~|kind:variable|line:7|typeref:struct:__anon0123456789ab11::S|roles:def|end:7"
  "h|x.h|/^int h()$/~|kind:function|line:1|namespace:__anon0123456789ab11|roles:def|end:1")
check(anon-fields 0 --ledger anon.ledger --scope a anon-ref.tags anon-ours.tags)
check(anon-unledgered 1 anon-ref.tags anon-ours.tags MATCH "2 missing, 2 extra")

# By default (no field pseudo-tags), file: alone, at the end
fixture(anon-default-ref.tags
  "v|x.cpp|/^int v$/~|v|line:3|namespace:__anon0123456789ab11|typeref:typename:int")
fixture(anon-default-ours.tags
  "v|x.cpp|/^int v$/~|v|line:3|namespace:__anon0123456789ab11|typeref:typename:int|file:")
check(anon-default 0 --ledger anon.ledger --scope a
  anon-default-ref.tags anon-default-ours.tags)

# Without the file field, only the extras
fixture(anon-nofile-ref.tags
  "!_TAG_FIELD_DESCRIPTION|extras|/Extra tag type information/"
  "v|x.cpp|/^int v$/~|variable|line:3|namespace:__anon0123456789ab11|end:3")
fixture(anon-nofile-ours.tags
  "v|x.cpp|/^int v$/~|variable|line:3|namespace:__anon0123456789ab11|extras:fileScope|end:3")
check(anon-nofile 0 --ledger anon.ledger --scope a
  anon-nofile-ref.tags anon-nofile-ours.tags)

# --nofile: output without pseudo-tags (-f -) and without the file field
fixture(anon-nofile-stdout-ref.tags
  "v|x.cpp|/^int v$/~|variable|line:3|namespace:__anon0123456789ab11")
check(anon-nofile-stdout 0 --ledger anon.ledger --scope a --nofile
  anon-nofile-stdout-ref.tags anon-nofile-stdout-ref.tags)
check(anon-nofile-stdout-default 1 --ledger anon.ledger --scope a
  anon-nofile-stdout-ref.tags anon-nofile-stdout-ref.tags
  MATCH "1 missing, 1 extra")

# no-end-field: end: is dropped from the reference, wherever it is
fixture(end.ledger
  "@ *: FLD-5"
  "! no-end-field")
fixture(end-ref.tags
  "f|x.c|/^f$/~|f|line:1|end:3|signature:()"
  "g|x.c|/^g$/~|f|line:4|end:4"
  "h|x.c|/^h$/~|end:5")
fixture(end-ours.tags
  "f|x.c|/^f$/~|f|line:1|signature:()"
  "g|x.c|/^g$/~|f|line:4"
  "h|x.c|/^h$/~")
check(end-field 0 --ledger end.ledger --scope e end-ref.tags end-ours.tags)
check(end-field-unledgered 1 end-ref.tags end-ours.tags
  MATCH "3 missing, 3 extra")


# --ordered: order within each file, order of files, and duplicates
fixture(order-ref.tags
  "a|x.c|/^a$/~|v|line:1"
  "b|x.c|/^b$/~|v|line:2"
  "c|y.c|/^c$/~|v|line:1")
fixture(order-swap.tags
  "b|x.c|/^b$/~|v|line:2"
  "a|x.c|/^a$/~|v|line:1"
  "c|y.c|/^c$/~|v|line:1")
fixture(order-files.tags
  "c|y.c|/^c$/~|v|line:1"
  "a|x.c|/^a$/~|v|line:1"
  "b|x.c|/^b$/~|v|line:2")
fixture(order-dup.tags
  "a|x.c|/^a$/~|v|line:1"
  "a|x.c|/^a$/~|v|line:1"
  "b|x.c|/^b$/~|v|line:2"
  "c|y.c|/^c$/~|v|line:1")
fixture(order-split.tags
  "a|x.c|/^a$/~|v|line:1"
  "c|y.c|/^c$/~|v|line:1"
  "b|x.c|/^b$/~|v|line:2")
fixture(order-gap.tags
  "a|x.c|/^a$/~|v|line:1"
  "c|y.c|/^c$/~|v|line:1")
fixture(order.ledger
  "@ o: one difference in x.c"
  "= 1 x.c"
  "@ g: b is missing"
  "- b|x.c|/^b$/~|v|line:2")
check(order-sets 0 order-ref.tags order-swap.tags)
check(order-lines 1 --ordered order-ref.tags order-swap.tags
  MATCH "x\\.c: lines differ in order" "1 reordered")
check(order-lines-ledger 0 --ordered --ledger order.ledger --scope o
  order-ref.tags order-swap.tags)
check(order-files 1 --ordered order-ref.tags order-files.tags
  MATCH "x\\.c: files differ in order")
check(order-files-ledger 0 --ordered --ledger order.ledger --scope o
  order-ref.tags order-files.tags)
check(order-split 1 --ordered order-ref.tags order-split.tags
  MATCH "x\\.c: files differ in order" "1 reordered")
check(order-split-ledger 0 --ordered --ledger order.ledger --scope o
  order-ref.tags order-split.tags)
check(order-dup-sets 0 order-ref.tags order-dup.tags)
check(order-dup 1 --ordered order-ref.tags order-dup.tags
  MATCH "\\+ a.x\\.c" "1 extra")
check(order-gap 0 --ordered --ledger order.ledger --scope g
  order-ref.tags order-gap.tags MATCH "0 reordered")


# --folded: ours in foldcase order, and the same lines as the reference
fixture(fold-ref.tags
  "a|x.c|/^a$/~|v|line:1"
  "B|x.c|/^B$/~|v|line:2"
  "_c|x.c|/^_c$/~|v|line:3")
fixture(fold-ours.tags
  "a|x.c|/^a$/~|v|line:1"
  "B|x.c|/^B$/~|v|line:2"
  "_c|x.c|/^_c$/~|v|line:3")
fixture(fold-bytes.tags
  "B|x.c|/^B$/~|v|line:2"
  "_c|x.c|/^_c$/~|v|line:3"
  "a|x.c|/^a$/~|v|line:1")
check(fold-same 0 --folded fold-ref.tags fold-ours.tags MATCH "0 reordered")
check(fold-bytes 1 --folded fold-ref.tags fold-bytes.tags
  MATCH "not in foldcase order" "1 reordered")
check(fold-ref 0 --folded fold-bytes.tags fold-ours.tags)


# --base: only tags whose base lines agree are compared
fixture(base-ref0.tags
  "f|x.c|/^f$/~|f|line:1"
  "v|x.c|/^v$/~|v|line:2|typeref:typename:A")
fixture(base-ours0.tags
  "f|x.c|/^f$/~|f|line:1"
  "v|x.c|/^v$/~|v|line:2|typeref:typename:B")
fixture(base-ref.tags
  "f|x.c|/^f$/~|function|line:1"
  "v|x.c|/^v$/~|variable|line:2|typeref:typename:A")
fixture(base-ours.tags
  "f|x.c|/^f$/~|function|line:1"
  "v|x.c|/^v$/~|variable|line:2|typeref:typename:B")
fixture(base-bad.tags
  "f|x.c|/^f$/~|function|line:1|typeref:typename:C"
  "v|x.c|/^v$/~|variable|line:2|typeref:typename:B")
check(base 0 --base base-ref0.tags base-ours0.tags base-ref.tags base-ours.tags
  MATCH "1 keys compared, 1 skipped")
check(base-without 1 base-ref.tags base-ours.tags)
check(base-bad 1 --base base-ref0.tags base-ours0.tags base-ref.tags base-bad.tags
  MATCH "\\+ f")

# Keys from numeric addresses, without line:
fixture(base-num-ref.tags
  "f|x.c|1~|f"
  "v|x.c|2~|v|typeref:typename:A")
fixture(base-num-ours.tags
  "f|x.c|1~|f"
  "v|x.c|2~|v|typeref:typename:B")
check(base-num 0 --base base-ref0.tags base-ours0.tags
  base-num-ref.tags base-num-ours.tags MATCH "1 keys compared, 1 skipped")

# Keys in neither base are compared
fixture(base-new.tags
  "f|x.c|/^f$/~|function|line:1"
  "q|x.c|/^q$/~|variable|line:9")
check(base-new 1 --base base-ref0.tags base-ours0.tags base-new.tags base-ours.tags
  MATCH "- q" "2 keys compared")

# Ordered: only clean keys' lines count toward the order
fixture(base-order-ref.tags
  "v|x.c|/^v$/~|variable|line:2|typeref:typename:A"
  "f|x.c|/^f$/~|function|line:1")
check(base-order 0 --ordered --base base-ref0.tags base-ours0.tags
  base-order-ref.tags base-ours.tags)


# --etags: sections and entries
set(f "int f()<DEL>f<SOH>1,0")
set(v "int v<DEL>v<SOH>2,8")
set(ref "")
etagsection(ref x.c "${f}" "${v}")
etagsection(ref empty.c)
etags(e-ref.tags "${ref}")
etags(e-same.tags "${ref}")
set(diff "")
etagsection(diff x.c "${f}" "int v<DEL>v<SOH>3,8")
etagsection(diff empty.c)
etags(e-diff.tags "${diff}")
set(swap "")
etagsection(swap x.c "${v}" "${f}")
etagsection(swap empty.c)
etags(e-swap.tags "${swap}")
set(nosection "")
etagsection(nosection x.c "${f}" "${v}")
etags(e-nosection.tags "${nosection}")
set(spaced "")
etagsection(spaced x.c "${f}" "int  v<DEL>v<SOH>2,8")
etagsection(spaced empty.c)
etags(e-text.tags "${spaced}")
set(lf "")  # an entry's text keeping its LF, as for a name not in it
etagsection(lf x.f90 "subroutine fo&" "<DEL>foo<SOH>1,0" "${f}")
etags(e-lf.tags "${lf}")
etags(e-lf2.tags "${lf}")
set(nolf "")
etagsection(nolf x.f90 "subroutine fo&<DEL>foo<SOH>1,0" "${f}")
etags(e-nolf.tags "${nolf}")
set(dangling "")
etagsection(dangling x.f90 "${f}" "subroutine fo&")
etags(e-dangling.tags "${dangling}")
check(etags-lf 0 --etags e-lf.tags e-lf2.tags MATCH "3 ref, 3 ours")
check(etags-lf-diff 1 --etags e-lf.tags e-nolf.tags
  MATCH "- foo.x\\.f90.1.subroutine fo&\\\\n,0")
check(etags-lf-dangling 1 --etags e-lf.tags e-dangling.tags
  MATCH "malformed TAGS")
fixture(e-badsize.tags "<FF>" "x.c,99" "${f}")
fixture(e-bigsize.tags "<FF>" "x.c,99999999999999999999" "${f}")
check(etags-same 0 --etags e-ref.tags e-same.tags MATCH "4 ref, 4 ours")
check(etags-diff 1 --etags e-ref.tags e-diff.tags
  MATCH "- v.x\\.c.2.int v,8" "\\+ v.x\\.c.3.int v,8")
check(etags-section 1 --etags e-ref.tags e-nosection.tags
  MATCH "- .empty\\.c.0" "1 missing, 0 extra")
check(etags-size 1 --etags e-ref.tags e-badsize.tags MATCH "malformed TAGS")
check(etags-bigsize 1 --etags e-ref.tags e-bigsize.tags MATCH "malformed TAGS")
check(etags-format 1 --etags e-ref.tags base-ours.tags MATCH "malformed TAGS")
check(etags-sets 0 --etags e-ref.tags e-swap.tags)
check(etags-ordered 1 --etags --ordered e-ref.tags e-swap.tags
  MATCH "x\\.c: lines differ in order")
check(etags-base 0 --etags --base base-ref0.tags base-ours0.tags
  e-ref.tags e-text.tags MATCH "3 keys compared, 1 skipped")
check(etags-base-dirty 1 --etags e-ref.tags e-text.tags)

if(failed)
  message(FATAL_ERROR "${failed} tagdiff checks failed")
endif()
