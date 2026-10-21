# Run one editor plugin's or build script's ctags command lines on a
# copy of the sample tree (test/plugins), with Universal Ctags and with
# ctags, and compare. The command lines are the tools' own, cited by
# file and line; the plugins' sources are at:
#   tagbar        https://github.com/preservim/tagbar
#   vista.vim     https://github.com/liuchengxu/vista.vim
#   ctrlp.vim     https://github.com/ctrlpvim/ctrlp.vim
#   taglist       https://github.com/yegappan/taglist
#   fzf.vim       https://github.com/junegunn/fzf.vim
#   fzf-lua       https://github.com/ibhagwan/fzf-lua
#   LeaderF       https://github.com/Yggdroot/LeaderF
#   vim-easytags  https://github.com/xolox/vim-easytags
#   vim-gutentags https://github.com/ludovicchabant/vim-gutentags
#   OmniCppComplete https://github.com/vim-scripts/OmniCppComplete
#   Linux         https://github.com/torvalds/linux (scripts/tags.sh)
#   Kate          https://invent.kde.org/utilities/kate (addons/kate-ctags)
# as of 2026-10-21.
#   $ cmake -DTOOL=name -DUCTAGS=... -DOURS=... -DTAGDIFF=... -DTREE=dir
#           -DLEDGER=file -DWORK=dir [-DWINDOWS=ON] -P runplugins.cmake
#
# Universal Ctags is given --options=NONE --languages=C,C++,Fortran
# --fields=+n before the tool's arguments: no option files, only the
# languages this program parses, and line: as this program always
# writes it (spec P-1). An absolute --fields replaces the +n.
#
# Each comparison is a tagdiff run with scope plugins/TOOL, or
# plugins/TOOL/STEP for a tool of several steps, and --ordered where the
# tool asks for unsorted output (--sort=no, -u, -e). Exits 77 (skipped)
# when ctags does not support the tool's options yet.

file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})
# tree: shared by both tools, for commands that only read it; ref and
# ours: one for each, for commands that write tags files into it
foreach(dir IN ITEMS tree ref ours)
  file(COPY ${TREE}/ DESTINATION ${WORK}/${dir})
endforeach()
# File tags' epoch: fields are compared, so the copies are modified now,
# not when the tree was checked out: on Windows, Universal Ctags writes
# the epoch of a file modified outside daylight saving time an hour off
# while it is in effect (D17).
file(GLOB_RECURSE copies ${WORK}/*)
file(TOUCH ${copies})

set(plug_ref ${UCTAGS} --options=NONE --languages=C,C++,Fortran --fields=+n)

# Write TEXT to PATH as it is: file(WRITE) would make LF into CRLF on
# Windows, and file(CONFIGURE) expands @VAR@, so each @ is written as
# @at@, which expands back to @.
function(plug_write path text)
  set(at "@")
  string(REPLACE "@" "@at@" text "${text}")
  file(CONFIGURE OUTPUT ${path} CONTENT "${text}" @ONLY NEWLINE_STYLE LF)
endfunction()

# Run one tool, SIDE ref or ours, in DIR, with the arguments after OUT;
# its standard output goes to OUT unless OUT is "-".
function(plug_run side dir out)
  if(side STREQUAL "ref")
    set(cmd ${plug_ref} ${ARGN})
  else()
    set(cmd ${OURS} ${ARGN})
  endif()
  set(io OUTPUT_VARIABLE ignored)
  if(NOT out STREQUAL "-")
    set(io OUTPUT_FILE ${out})
  endif()
  execute_process(COMMAND ${cmd} WORKING_DIRECTORY ${dir} ${io}
    RESULT_VARIABLE status ERROR_VARIABLE err)
  if(NOT status EQUAL 0)
    string(REPLACE ";" " " shown "${cmd}")
    message(FATAL_ERROR "${side} failed (${status}) in ${dir}:\n"
      "  ${shown}\n${err}")
  endif()
  # Vim's system() reads standard error with the output (its
  # 'shellredir' has 2>&1), so ctags must be quiet where Universal
  # Ctags is, in the run just before for the same command
  if(side STREQUAL "ref")
    set_property(GLOBAL PROPERTY plug_referr "${err}")
  else()
    get_property(referr GLOBAL PROPERTY plug_referr)
    if(NOT err STREQUAL "" AND referr STREQUAL "")
      string(REPLACE ";" " " shown "${ARGN}")
      plug_fail("standard error where Universal Ctags has none:\n  ${shown}\n${err}")
    endif()
  endif()
endfunction()

# Run both tools in their own trees (WHERE "own") or the shared one
# ("tree"), stdout to WORK/SIDE.NAME unless NAME is "-".
function(plug_both where name)
  foreach(side IN ITEMS ref ours)
    set(dir ${WORK}/${side})
    if(where STREQUAL "tree")
      set(dir ${WORK}/tree)
    endif()
    set(out ${WORK}/${side}.${name})
    if(name STREQUAL "-")
      set(out -)
    endif()
    plug_run(${side} ${dir} ${out} ${ARGN})
  endforeach()
endfunction()

# One buffer of a per-buffer tool: both tools' standard output for the
# arguments given, appended to WORK/SIDE.NAME.
function(plug_buffer name)
  plug_both(tree buffer ${ARGN})
  foreach(side IN ITEMS ref ours)
    file(READ ${WORK}/${side}.buffer text)
    file(APPEND ${WORK}/${side}.${name} "${text}")
  endforeach()
endfunction()

# Compare REF and OURS, as STEP of this tool ("" for the tool's only
# comparison), with tagdiff flags after them.
function(plug_compare step ref ours)
  set(scope plugins/${TOOL})
  if(step)
    set(scope ${scope}/${step})
  endif()
  execute_process(
    COMMAND ${TAGDIFF} --ledger ${LEDGER} --scope ${scope} ${ARGN} ${ref} ${ours}
    RESULT_VARIABLE status OUTPUT_VARIABLE out)
  message("${scope}: ${out}")
  if(NOT status EQUAL 0)
    set_property(GLOBAL APPEND PROPERTY plug_failed ${scope})
  endif()
endfunction()

function(plug_fail what)
  message("${TOOL}: ${what}")
  set_property(GLOBAL APPEND PROPERTY plug_failed "${what}")
endfunction()

# A written tags file must begin with at least 6 pseudo-tags: LeaderF's
# tag explorer drops the first 6 lines of every tags file
# (autoload/leaderf/python/leaderf/tagExpl.py:34,40, f.readlines()[6:]).
function(plug_header path)
  file(READ ${path} head LIMIT 8192)
  if(NOT head MATCHES "^(!_[^\n]*\n)(!_[^\n]*\n)(!_[^\n]*\n)(!_[^\n]*\n)(!_[^\n]*\n)(!_[^\n]*\n)")
    plug_fail("${path}: fewer than 6 pseudo-tags at its start")
  endif()
endfunction()

# Under -n every address is a line number: LeaderF runs int() on it
# (autoload/leaderf/python/leaderf/bufTagExpl.py, _formatResult).
function(plug_numeric path)
  file(READ ${path} text)
  if(text MATCHES "(^|\n)([^!\t\n][^\t\n]*\t[^\t\n]*\t([^0-9\n]|[0-9]+[^0-9;\n]|[0-9]+;[^\"\n])[^\n]*)")
    plug_fail("${path}: an address that is not a number: ${CMAKE_MATCH_2}")
  endif()
endfunction()

# Copy a source file as Vim's writefile() writes a buffer's lines: no
# BOM, no CRs, LF after each line. tagbar and vista tag such a copy.
function(plug_vimcopy src dst)
  file(READ ${src} bom LIMIT 3 HEX)
  if(bom STREQUAL "efbbbf")
    file(READ ${src} text OFFSET 3)
  else()
    file(READ ${src} text)
  endif()
  string(REPLACE "\r" "" text "${text}")
  plug_write(${dst} "${text}")
endfunction()

# Delete the lines of a file that match REGEX (a line's text, without
# its newline, anchored by the caller), as grep -v does.
function(plug_grepv src dst regex)
  file(READ ${src} text)
  string(REGEX REPLACE "(^|\n)${regex}[^\n]*" "" text "\n${text}")
  string(REGEX REPLACE "^\n" "" text "${text}")
  plug_write(${dst} "${text}")
endfunction()

# The source files as buffers, with Vim's 'filetype' for each: Vim
# makes .h files cpp (unless c_syntax_for_h is set).
set(plug_buffers
  src/main.c:c src/crlf.c:c src/driver.c:c src/util.h:cpp
  src/shapes.hpp:cpp src/shapes.cpp:cpp src/bom.cpp:cpp
  sub/solver.f90:fortran sub/legacy.f:fortran)

# An edit to src/main.c before an incremental update: a line inserted
# at the top moves every tag, and a function is renamed.
function(plug_edit dir)
  file(READ ${dir}/src/main.c text)
  string(REPLACE "int point_sum(Point p)" "int point_total(Point p)" text "${text}")
  string(REPLACE "point_sum(p)" "point_total(p)" text "${text}")
  plug_write(${dir}/src/main.c "// Edited.\n${text}")
endfunction()


# Version checks: the --version output must pass each tool's test.
function(tool_version)
  execute_process(COMMAND ${OURS} --version
    OUTPUT_VARIABLE out RESULT_VARIABLE status)
  if(NOT status EQUAL 0)
    plug_fail("--version failed")
  endif()
  message("--version: ${out}")
  # tagbar refuses any ctags whose --version output does not match this,
  # case-sensitively (autoload/tagbar.vim:683):
  #   ctags_output !~# '\(Exuberant\|Universal\) Ctags'
  if(NOT out MATCHES "(Exuberant|Universal) Ctags")
    plug_fail("tagbar: not (Exuberant|Universal) Ctags")
  endif()
  # and takes the Universal Ctags arguments when it matches this,
  # ignoring case (autoload/tagbar.vim:745, =~? 'Universal Ctags')
  string(TOLOWER "${out}" lower)
  if(NOT lower MATCHES "universal ctags")
    plug_fail("tagbar: not Universal Ctags")
  endif()
  # easytags takes a version from the first line, which must be at
  # least 5.5 (autoload/xolox/easytags.vim:81-89, plugin/easytags.vim:66):
  #   '\(Exuberant\|Universal\) Ctags \zs\(\d\+\(\.\d\+\)*\|Development\)'
  string(REGEX MATCH "^[^\n]*" first "${out}")
  if(first MATCHES "(Exuberant|Universal) Ctags ([0-9]+(\\.[0-9]+)*|Development)")
    set(v ${CMAKE_MATCH_2})
    if(NOT v STREQUAL "Development" AND v VERSION_LESS 5.5)
      plug_fail("easytags: version ${v} is under 5.5")
    endif()
  else()
    plug_fail("easytags: no version on the first line")
  endif()
  # fzf-lua's btags adds --excmd=combine for Universal Ctags
  # (lua/fzf-lua/utils.lua:1699-1703, out:match("Universal"))
  if(NOT out MATCHES "Universal")
    plug_fail("fzf-lua: not Universal")
  endif()
  # The Linux kernel's scripts/tags.sh (xtags, line 338) gives its
  # options only to a ctags whose --version matches grep -i exuberant,
  # as Universal Ctags' does ("derived from Exuberant Ctags"); any other
  # gets plain "ctags -a FILES" (line 343). Not a failure: both run in
  # the kernel test.
  if(NOT lower MATCHES "exuberant")
    message("NOTE: the kernel's tags.sh takes the plain branch (no -I, "
            "--extras=+fq, --c-kinds=+px, --fields=+iaS, --langmap=c:+.h)")
  endif()
endfunction()

# tagbar, Universal Ctags path: s:ExecuteCtagsOnFile
# (autoload/tagbar.vim:1386-1428) on a copy of the buffer
# (tempname() plus the extension, autoload/tagbar.vim:1259-1266):
#   --extras=+F -f - --format=2 --excmd=pattern --fields=nksSafet
#   --sort=no --append=no -V --language-force=L --L-kinds=KINDS FILE
# -V only with tagbar's debug log on (1402-1404). The kinds are every
# short name in the type's list (autoload/tagbar/types/uctags.vim:
# c 282-294, c++ 311-325, fortran 606-622).
function(tool_tagbar)
  set(lang_c c)
  set(lang_cpp c++)
  set(lang_fortran fortran)
  set(kinds_c hdpgetsumvf)
  set(kinds_cpp hdpgetncsufmv)
  set(kinds_fortran mpktcbENefsMlnv)
  set(n 0)
  foreach(b IN LISTS plug_buffers)
    string(REPLACE ":" ";" b "${b}")
    list(GET b 0 file)
    list(GET b 1 ft)
    get_filename_component(ext ${file} LAST_EXT)
    math(EXPR n "${n} + 1")
    set(tmp ${WORK}/vimtmp/VI${n}${ext})
    plug_vimcopy(${WORK}/tree/${file} ${tmp})
    foreach(debug IN ITEMS "" -V)
      plug_buffer(tagbar${debug}
        --extras=+F -f - --format=2 --excmd=pattern --fields=nksSafet
        --sort=no --append=no ${debug}
        --language-force=${lang_${ft}} --${lang_${ft}}-kinds=${kinds_${ft}}
        ${tmp})
    endforeach()
  endforeach()
  plug_compare("" ${WORK}/ref.tagbar ${WORK}/ours.tagbar --ordered)
  plug_compare(debug ${WORK}/ref.tagbar-V ${WORK}/ours.tagbar-V --ordered)
endfunction()

# vista.vim without JSON support: s:GetDefaultCmd
# (autoload/vista/executive/ctags.vim:23, 39-52) and its language
# options (55-67), on a copy of the buffer (s:IntoTemp, 286-):
#   --format=2 --excmd=pattern --fields=+nksSaf --extras=+F --sort=no
#   --append=no [--extras=] --language-force=L --L-kinds=KINDS -f- FILE
# where --extras= is added for filetypes other than c and cpp, and the
# kinds are the keys of a dictionary (autoload/vista/types/uctags/
# c.vim:6-18, cpp.vim:7-21, fortran.vim:7-23), in no set order.
function(tool_vista)
  set(lang_c c)
  set(lang_cpp c++)
  set(lang_fortran fortran)
  set(kinds_c hdpgetsumvf)
  set(kinds_cpp hdpgetncsufmv)
  set(kinds_fortran mpktcbENefsMlnv)
  set(n 0)
  foreach(b IN LISTS plug_buffers)
    string(REPLACE ":" ";" b "${b}")
    list(GET b 0 file)
    list(GET b 1 ft)
    get_filename_component(ext ${file} LAST_EXT)
    math(EXPR n "${n} + 1")
    set(tmp ${WORK}/vimtmp/vista${n}${ext})
    plug_vimcopy(${WORK}/tree/${file} ${tmp})
    set(extras "")
    if(NOT ft STREQUAL "c" AND NOT ft STREQUAL "cpp")
      set(extras --extras=)
    endif()
    plug_buffer(vista
      --format=2 --excmd=pattern --fields=+nksSaf --extras=+F --sort=no
      --append=no ${extras}
      --language-force=${lang_${ft}} --${lang_${ft}}-kinds=${kinds_${ft}}
      -f- ${tmp})
  endforeach()
  plug_compare("" ${WORK}/ref.vista ${WORK}/ours.vista --ordered)
endfunction()

# ctrlp's buffertag extension: s:exectagsonfile
# (autoload/ctrlp/buffertag.vim:141-151), with the types of
# s:types (52, 53, 61) made into options at line 95:
#   -f - --sort=no --excmd=pattern --fields=nKs --extra= --file-scope=yes
#   --language-force=L --L-types=TYPES FILE
function(tool_ctrlp)
  set(types_c --language-force=c --c-types=dgsutvf)
  set(types_cpp --language-force=c++ --c++-types=nvdtcgsuf)
  set(types_fortran --language-force=fortran --fortran-types=pbceiklmntvfs)
  foreach(b IN LISTS plug_buffers)
    string(REPLACE ":" ";" b "${b}")
    list(GET b 0 file)
    list(GET b 1 ft)
    plug_buffer(ctrlp
      -f - --sort=no --excmd=pattern --fields=nKs --extra= --file-scope=yes
      ${types_${ft}} ${file})
  endforeach()
  plug_compare("" ${WORK}/ref.ctrlp ${WORK}/ours.ctrlp --ordered --nofile)
endfunction()

# taglist: s:Tlist_Process_File (autoload/taglist.vim:1761-1775), with
# the language's options from s:Tlist_FileType_Init (807-808) and the
# flags of s:tlist_lang_def (c 209-210, cpp 213-214, fortran 247-249):
#   -f - --format=2 --excmd=pattern --fields=nks --sort=no|yes
#   --language-force=L --L-types=FLAGS "FILE"
# --sort=no for Tlist_Sort_Type 'order' (the default), yes for 'name'.
function(tool_taglist)
  set(types_c --language-force=c --c-types=dgsutvf)
  set(types_cpp --language-force=c++ --c++-types=nvdtcgsuf)
  set(types_fortran --language-force=fortran --fortran-types=pbceiklmntvfs)
  foreach(b IN LISTS plug_buffers)
    string(REPLACE ":" ";" b "${b}")
    list(GET b 0 file)
    list(GET b 1 ft)
    foreach(sort IN ITEMS no yes)
      plug_buffer(taglist-${sort}
        -f - --format=2 --excmd=pattern --fields=nks --sort=${sort}
        ${types_${ft}} ${WORK}/tree/${file})
    endforeach()
  endforeach()
  plug_compare(order ${WORK}/ref.taglist-no ${WORK}/ours.taglist-no --ordered --nofile)
  plug_compare(name ${WORK}/ref.taglist-yes ${WORK}/ours.taglist-yes --nofile)
endfunction()

# fzf.vim :BTags: fzf#vim#buffer_tags (autoload/fzf/vim.vim:1766-1767),
# the first command, then the second if it fails:
#   ctags -f - --sort=yes --excmd=number --language-force=L FILE
#   ctags -f - --sort=yes --excmd=number FILE
# with L the filetype, cpp made c++. Both are run here.
function(tool_fzf-btags)
  foreach(b IN LISTS plug_buffers)
    string(REPLACE ":" ";" b "${b}")
    list(GET b 0 file)
    list(GET b 1 ft)
    if(ft STREQUAL "cpp")
      set(ft c++)
    endif()
    plug_buffer(btags -f - --sort=yes --excmd=number --language-force=${ft} ${file})
    plug_buffer(btags2 -f - --sort=yes --excmd=number ${file})
  endforeach()
  plug_compare(forced ${WORK}/ref.btags ${WORK}/ours.btags)
  plug_compare(fallback ${WORK}/ref.btags2 ${WORK}/ours.btags2)
  plug_numeric(${WORK}/ours.btags)
  plug_numeric(${WORK}/ours.btags2)
endfunction()

# fzf.vim :Tags, generating a missing tags file (autoload/fzf/vim.vim:1889):
#   ctags -R --output-format=e-ctags     (on Windows)
#   ctags -R                             (elsewhere)
function(tool_fzf-tags)
  plug_both(own - -R --output-format=e-ctags)
  plug_compare(e-ctags ${WORK}/ref/tags ${WORK}/ours/tags)
  plug_header(${WORK}/ours/tags)
  foreach(side IN ITEMS ref ours)
    file(RENAME ${WORK}/${side}/tags ${WORK}/${side}/tags.e-ctags)
  endforeach()
  plug_both(own - -R)
  plug_compare(default ${WORK}/ref/tags ${WORK}/ours/tags)
  plug_header(${WORK}/ours/tags)
endfunction()

# fzf-lua btags without a tags file: M.btags
# (lua/fzf-lua/providers/tags.lua:208-212), --excmd=combine for
# Universal Ctags:
#   ctags -f - --excmd=combine FILE
function(tool_fzf-lua-btags)
  foreach(b IN LISTS plug_buffers)
    string(REPLACE ":" ";" b "${b}")
    list(GET b 0 file)
    plug_buffer(btags -f - --excmd=combine ${file})
  endforeach()
  plug_compare("" ${WORK}/ref.btags ${WORK}/ours.btags)
endfunction()

# LeaderF BufTag (autoload/leaderf/python/leaderf/bufTagExpl.py:94-120)
# and Function (functionExpl.py:27-35, 135-138):
#   ctags -n -u --fields=Ks OPTS -f- "FILE"
#   ctags -n -u --fields=k OPTS -f- "FILE"
# BufTag's OPTS: cpp "--language-force=C++ --c++-kinds=+p", c
# "--c-kinds=+p", others none. Function's: c "--c-kinds=fp", cpp
# "--c++-kinds=fp --language-force=C++", fortran "--fortran-kinds=f".
function(tool_leaderf)
  set(buf_c --c-kinds=+p)
  set(buf_cpp --language-force=C++ --c++-kinds=+p)
  set(buf_fortran "")
  set(fun_c --c-kinds=fp)
  set(fun_cpp --c++-kinds=fp --language-force=C++)
  set(fun_fortran --fortran-kinds=f)
  foreach(b IN LISTS plug_buffers)
    string(REPLACE ":" ";" b "${b}")
    list(GET b 0 file)
    list(GET b 1 ft)
    plug_buffer(buftag -n -u --fields=Ks ${buf_${ft}} -f- ${WORK}/tree/${file})
    plug_buffer(function -n -u --fields=k ${fun_${ft}} -f- ${WORK}/tree/${file})
  endforeach()
  plug_compare(buftag ${WORK}/ref.buftag ${WORK}/ours.buftag --ordered --nofile)
  plug_compare(function ${WORK}/ref.function ${WORK}/ours.function --ordered --nofile)
  plug_numeric(${WORK}/ours.buftag)
  plug_numeric(${WORK}/ours.function)
endfunction()

# vim-easytags: s:prep_cmdline (autoload/xolox/easytags.vim:244-272),
# without easytags_include_members or easytags_autorecurse:
#   ctags --fields=+l --c-kinds=+p --c++-kinds=+p --sort=no -f-
#   --language-force=L FILE
# with cpp mapped to c++ (autoload/xolox/easytags/filetypes.vim, as
# plugin/easytags.vim:135 adds).
function(tool_easytags)
  foreach(b IN LISTS plug_buffers)
    string(REPLACE ":" ";" b "${b}")
    list(GET b 0 file)
    list(GET b 1 ft)
    if(ft STREQUAL "cpp")
      set(ft c++)
    endif()
    plug_buffer(easytags --fields=+l --c-kinds=+p --c++-kinds=+p --sort=no
      -f- --language-force=${ft} ${file})
  endforeach()
  plug_compare("" ${WORK}/ref.easytags ${WORK}/ours.easytags --ordered)
endfunction()

# vim-gutentags with plat/unix/update_tags.sh, run from the project
# root with the arguments gutentags#ctags#generate builds
# (autoload/gutentags/ctags.vim:142-195): -o ctags_recursive.options
# unless there is a file list (170), -O --tag-relative=yes when the
# tags file is in a subdirectory, as .git/tags (119, 173), -o the
# project's .gutctags (186), and -x @WILDIGNORE (191), a file of one
# 'wildignore' entry per line (271-274). The script keeps only the last
# -o (update_tags.sh:73), so .gutctags replaces ctags_recursive.options,
# and passes -O and -x arguments in order (49, 76):
#   full (132):    ctags -f "tags.temp" "--options=PROJ/.gutctags"
#                  [--tag-relative=yes] --exclude=@WILDIGNORE "."
#   list (123, 132): ... --exclude=@WILDIGNORE -L "tags.files"
#   update (102, 141): grep --text -Ev '^[^\t]+\tFILE\t' tags > tags.temp
#                  ctags -f "tags.temp" "--options=PROJ/.gutctags"
#                  [--tag-relative=yes] --exclude=@WILDIGNORE --append "FILE"
# then mv -f tags.temp tags (157). The updated FILE is relative to the
# project root (ctags.vim:149-151).
function(tool_gutentags)
  file(COPY ${WORK}/tree/wildignore.txt DESTINATION ${WORK}/wild)
  set(wild --exclude=@${WORK}/wild/wildignore.txt)

  # Tags file at the project root
  foreach(side IN ITEMS ref ours)
    set(dir ${WORK}/${side})
    plug_run(${side} ${dir} - -f tags.temp --options=${dir}/.gutctags ${wild} .)
    file(RENAME ${dir}/tags.temp ${dir}/tags)
  endforeach()
  plug_compare(full ${WORK}/ref/tags ${WORK}/ours/tags)
  plug_header(${WORK}/ours/tags)

  foreach(side IN ITEMS ref ours)
    set(dir ${WORK}/${side})
    plug_edit(${dir})
    plug_grepv(${dir}/tags ${dir}/tags.temp "[^\t\n]+\tsrc/main\\.c\t")
    plug_run(${side} ${dir} - -f tags.temp --options=${dir}/.gutctags ${wild}
      --append src/main.c)
    file(RENAME ${dir}/tags.temp ${dir}/tags)
  endforeach()
  plug_compare(update ${WORK}/ref/tags ${WORK}/ours/tags)
  plug_header(${WORK}/ours/tags)

  # Tags file in .git: --tag-relative=yes, and the update's grep, which
  # looks for "src/main.c", misses the lines for "../src/main.c"
  foreach(side IN ITEMS ref ours)
    set(dir ${WORK}/${side})
    file(COPY ${TREE}/src/main.c DESTINATION ${dir}/src)
    file(MAKE_DIRECTORY ${dir}/.git)
    plug_run(${side} ${dir} - -f .git/tags.temp --options=${dir}/.gutctags
      --tag-relative=yes ${wild} .)
    file(RENAME ${dir}/.git/tags.temp ${dir}/.git/tags)
  endforeach()
  plug_compare(relative ${WORK}/ref/.git/tags ${WORK}/ours/.git/tags)
  plug_header(${WORK}/ours/.git/tags)

  foreach(side IN ITEMS ref ours)
    set(dir ${WORK}/${side})
    plug_edit(${dir})
    plug_grepv(${dir}/.git/tags ${dir}/.git/tags.temp "[^\t\n]+\tsrc/main\\.c\t")
    plug_run(${side} ${dir} - -f .git/tags.temp --options=${dir}/.gutctags
      --tag-relative=yes ${wild} --append src/main.c)
    file(RENAME ${dir}/.git/tags.temp ${dir}/.git/tags)
  endforeach()
  plug_compare(relative-update ${WORK}/ref/.git/tags ${WORK}/ours/.git/tags)
  plug_header(${WORK}/ours/.git/tags)

  # A file list command (gutentags_file_list_command), whose output
  # is tags.files
  foreach(side IN ITEMS ref ours)
    set(dir ${WORK}/${side})
    file(COPY ${TREE}/src/main.c DESTINATION ${dir}/src)
    file(COPY_FILE ${dir}/files.txt ${dir}/tags.files)
    plug_run(${side} ${dir} - -f tags.temp --options=${dir}/.gutctags ${wild}
      -L tags.files)
    file(RENAME ${dir}/tags.temp ${dir}/tags)
  endforeach()
  plug_compare(list ${WORK}/ref/tags ${WORK}/ours/tags)
  plug_header(${WORK}/ours/tags)
endfunction()

# vim-gutentags with plat/win32/update_tags.cmd, which keeps every -o
# (line 65), so ctags_recursive.options (res/, the one line
# "--recurse=yes") comes first, and filters with findstr on the path as
# Vim gives it, with backslashes (103):
#   full (127, 133): ctags -f "tags.temp" --options="RES/ctags_recursive.options"
#                    --options="PROJ/.gutctags" --exclude=@WILDIGNORE "."
#   update (103-104, 133): findstr /V /C:"src\main.c" tags > tags.temp
#                    ctags -f "tags.temp" ... --append "src\main.c"
function(tool_gutentags-win)
  if(NOT WINDOWS)
    message("Windows only")
    cmake_language(EXIT 77)
  endif()
  file(COPY ${WORK}/tree/wildignore.txt DESTINATION ${WORK}/wild)
  plug_write(${WORK}/res/ctags_recursive.options "--recurse=yes\n")
  set(opts --options=${WORK}/res/ctags_recursive.options)
  set(wild --exclude=@${WORK}/wild/wildignore.txt)
  foreach(side IN ITEMS ref ours)
    set(dir ${WORK}/${side})
    plug_run(${side} ${dir} - -f tags.temp ${opts} --options=${dir}/.gutctags
      ${wild} .)
    file(RENAME ${dir}/tags.temp ${dir}/tags)
  endforeach()
  plug_compare(full ${WORK}/ref/tags ${WORK}/ours/tags)
  plug_header(${WORK}/ours/tags)

  foreach(side IN ITEMS ref ours)
    set(dir ${WORK}/${side})
    plug_edit(${dir})
    plug_grepv(${dir}/tags ${dir}/tags.temp "[^\n]*src\\\\main\\.c")
    plug_run(${side} ${dir} - -f tags.temp ${opts} --options=${dir}/.gutctags
      ${wild} --append "src\\main.c")
    file(RENAME ${dir}/tags.temp ${dir}/tags)
  endforeach()
  plug_compare(update ${WORK}/ref/tags ${WORK}/ours/tags)
  plug_header(${WORK}/ours/tags)
endfunction()

# The Linux kernel's "make tags": scripts/tags.sh removes tags (369),
# then for a ctags whose --version says Exuberant runs exuberant()
# (314-317), xargs splitting the sources into batches:
#   ctags -a -I IGN --extras=+fq --c-kinds=+px --fields=+iaS
#   --langmap=c:+.h REGEX... FILES
# with IGN the list of 293-313 joined by commas, and --extras for a
# --version that says Universal (287-290). The --regex-asm and
# --regex-c options (setup_regex, 252-283) are left out: they would
# change which tags exist, and ctags refuses --regex-c (OPT-5).
# Otherwise it runs "ctags -a FILES" (343), the "plain" step.
function(tool_kernel)
  set(ign
    ACPI_EXPORT_SYMBOL DECLARE_BITMAP DEFINE_TRACE DEFINE_MUTEX DEFINE_TIMER
    EXPORT_SYMBOL EXPORT_SYMBOL_GPL
    EXPORT_TRACEPOINT_SYMBOL EXPORT_TRACEPOINT_SYMBOL_GPL
    ____cacheline_aligned ____cacheline_aligned_in_smp
    ____cacheline_internodealigned_in_smp
    __acquires __aligned __always_inline __always_unused
    __attribute
    __cacheline_aligned __cacheline_aligned_in_smp
    __deprecated
    __exitdata
    __initconst __initdata __initdata_memblock
    __maybe_unused __must_check __must_hold
    __packed __packed2__
    __read_mostly __refdata __releases __ro_after_init
    __used
    const
    static)
  string(REPLACE ";" "," ign "${ign}")
  set(opts -I ${ign} --extras=+fq --c-kinds=+px --fields=+iaS --langmap=c:+.h)
  set(batch1 src/crlf.c src/driver.c src/main.c)
  set(batch2 src/util.h build/gen.c third_party/vendor.c)
  foreach(batch IN ITEMS batch1 batch2)
    plug_both(own - -a ${opts} ${${batch}})
  endforeach()
  plug_compare(exuberant ${WORK}/ref/tags ${WORK}/ours/tags)
  plug_header(${WORK}/ours/tags)
  foreach(side IN ITEMS ref ours)
    file(RENAME ${WORK}/${side}/tags ${WORK}/${side}/tags.exuberant)
  endforeach()
  foreach(batch IN ITEMS batch1 batch2)
    plug_both(own - -a ${${batch}})
  endforeach()
  plug_compare(plain ${WORK}/ref/tags ${WORK}/ours/tags)
  plug_header(${WORK}/ours/tags)
endfunction()

# OmniCppComplete's recipe (doc/omnicppcomplete.txt:103):
#   ctags -R --c++-kinds=+p --fields=+iaS --extra=+q .
function(tool_omnicppcomplete)
  plug_both(own - -R --c++-kinds=+p --fields=+iaS --extra=+q .)
  plug_compare("" ${WORK}/ref/tags ${WORK}/ours/tags)
  plug_header(${WORK}/ours/tags)
endfunction()

# Kate's CTags plugin: the default command
# (addons/kate-ctags/kate_ctags_view.cpp:46), split into arguments, and
# -f with the session's tags file and the target folders, without a
# trailing slash (530-557):
#   ctags -R --c++-types=+px --extras=+q --excmd=pattern
#   --exclude=Makefile --exclude=. -f DB FOLDER
function(tool_kate)
  foreach(side IN ITEMS ref ours)
    plug_run(${side} ${WORK}/tree - -R --c++-types=+px --extras=+q
      --excmd=pattern --exclude=Makefile --exclude=. -f ${WORK}/${side}.kate
      ${WORK}/tree)
  endforeach()
  plug_compare("" ${WORK}/ref.kate ${WORK}/ours.kate)
  plug_header(${WORK}/ours.kate)
endfunction()

# Emacs: etags mode (Universal Ctags' ctags(1), -e: "Enable etags mode,
# which will create a tag file for use with the Emacs editor"):
#   ctags -e -R
function(tool_emacs)
  execute_process(COMMAND ${OURS} -e -f - ${WORK}/tree/src/main.c
    OUTPUT_QUIET ERROR_VARIABLE err RESULT_VARIABLE status)
  if(err MATCHES "not yet supported")
    message("-e is not yet supported: ${err}")
    cmake_language(EXIT 77)
  endif()
  plug_both(own - -e -R)
  plug_compare("" ${WORK}/ref/TAGS ${WORK}/ours/TAGS --etags --ordered)
endfunction()

if(NOT COMMAND tool_${TOOL})
  message(FATAL_ERROR "no such tool: ${TOOL}")
endif()
cmake_language(CALL tool_${TOOL})
get_property(failed GLOBAL PROPERTY plug_failed)
if(failed)
  string(REPLACE ";" "\n  " failed "${failed}")
  message(FATAL_ERROR "failed:\n  ${failed}")
endif()
