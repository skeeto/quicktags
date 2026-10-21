# Tests of caseopts.cmake, the choice of a Units case's options.
#   $ cmake -P caseopts_test.cmake
cmake_minimum_required(VERSION 3.21...4.4)

include(${CMAKE_CURRENT_LIST_DIR}/caseopts.cmake)
set(failed 0)

function(expect name got want)
  if(NOT got STREQUAL want)
    message("FAIL: ${name}\n  got:  [${got}]\n  want: [${want}]")
    math(EXPR n "${failed} + 1")
    set(failed ${n} PARENT_SCOPE)
  endif()
endfunction()

# --fields specs: stripped letters and names, merged signs, absolute
# specs kept absolute, and "*" as the fields it keeps
foreach(pair IN ITEMS
    "+K{nth}=--fields=+K"
    "+K=--fields=+K"
    "+e="
    "+eo{end}="
    "+iaSm{inherits}=--fields=+iaSm{inherits}"
    "e=--fields="
    "=--fields="
    "+K-e+n=--fields=+Kn"
    "Ke+n=--fields=Kn"
    "-o+n=--fields=+n"
    "-S+n=--fields=-S+n"
    "-k=--fields=-k"
    "+e-n=--fields=-n"
    "+{end}{scope}=--fields=+{scope}"
    "+{end}-{nth}="
    "o+n=--fields=n"
    "e+n=--fields=n"
    "{end}+K=--fields=K"
    "oe-k=--fields="
    "e-n=--fields="
    "o-n+k=--fields=k"
    "on-n=--fields=n-n"
    "Sn-n=--fields=Sn-n"
    "*=--fields=NFPCEKRSTZafiklmnprstxz"
    "*-o=--fields=NFPCEKRSTZafiklmnprstxz"
    "*-S=--fields=NFPCEKRSTZafiklmnprstxz-S"
    "*-n=--fields=NFPCEKRSTZafiklmnprstxz-n")
  string(FIND "${pair}" "=" eq)
  string(SUBSTRING "${pair}" 0 ${eq} spec)
  math(EXPR rest "${eq} + 1")
  string(SUBSTRING "${pair}" ${rest} -1 want)
  ctags_filterfields(got "${spec}")
  expect("fields ${spec}" "${got}" "${want}")
endforeach()

# Stripped from both command lines
foreach(opt IN ITEMS
    "#--undef=X" "--fields-C++=+{properties}" "--extras-Fortran=+{x}"
    "--param-CPreProcessor._expand=1" "-DX=1" "-D X(a)=a" "--if0=yes"
    "--line-directives" "--roles-C++.{macro}=+{undef}" "--langdef=Foo"
    "--kinddef-C=x,x,x" "--regex-C=/x/" "--mline-regex-C=/x/"
    "--_xformat=%N" "--kinds-CPreProcessor=+d" "--kinds-all=*"
    "--map-NEWLANG=+.x" "--language-force=Java")
  ctags_stripped(got "${opt}")
  expect("strip ${opt}" "${got}" ON)
endforeach()
foreach(opt IN ITEMS
    "--kinds-c++=+p" "--kinds-Fortran=+L" "--map-C=+.x"
    "--language-force=C++" "--languages=C" "--fields=+K" "--sort=no"
    "--extras=+q" "-n" "-I X+" "--param-CPreProcessor.ignore=X"
    "--param-cpreprocessor:ignore=X")
  ctags_stripped(got "${opt}")
  expect("keep ${opt}" "${got}" OFF)
endforeach()

# What each mode keeps
set(lines
  "--kinds-C++=+p" "  --extras=+qr" "--extras=+r" "--fields=+K{nth}"
  "--sort=no" "# a comment" "--KINDS-c=+l" "--C++-kinds=+z"
  "--kinds-CPreProcessor=+d" "--fields-C++=+{template}" "-I X=int"
  "--param-CPreProcessor.ignore=Y" "--param-CPreProcessor.if0=no" "")
ctags_caseopts("${lines}")
expect("args" "${ctags_args}"
  "--kinds-C++=+p;--extras=+q;--KINDS-c=+l;--C++-kinds=+z;-I X=int;--param-CPreProcessor.ignore=Y")
expect("fmt" "${ctags_fmt}" "--fields=+K;--sort=no")
expect("noepoch" "${ctags_noepoch}" ON)

# The file tags' extra, kept, but their epochs not compared (D17)
ctags_caseopts("--file-tags;--fields=+K")
expect("file-tags" "${ctags_args}" "--file-tags")
expect("file-tags noepoch" "${ctags_noepoch}" ON)
ctags_caseopts("--kinds-C=+p;--fields=+K")
expect("no extras noepoch" "${ctags_noepoch}" OFF)

set(formats
  "-n" "-N" "-nN" "--excmd=combine" "--format=1" "--pattern-length-limit=9"
  "--output-format=e-ctags" "--output-format=etags" "--fields=+e" "-e"
  "--sort=foldcase" "-u" "-uR")
ctags_caseopts("${formats}")
expect("args formats" "${ctags_args}" "")
expect("fmt formats" "${ctags_fmt}"
  "-n;-N;--excmd=combine;--format=1;--pattern-length-limit=9;--output-format=e-ctags;--sort=foldcase;-u")

# The options that choose languages, for these languages
ctags_caseopts("--langmap=c:.h.c;--MAP-c++=+.x;--map-Python=+.x;--languages=-LdScript;--language-force=Fortran;--language-force=CPreProcessor;-h;.x;--sort=no")
expect("languages" "${ctags_args}"
  "--langmap=c:.h.c;--MAP-c++=+.x;--languages=-LdScript;--language-force=Fortran;-h;.x")


# A run compared by key names files as its base run does
ctags_keyedopts(got --sort=no)
expect("keyed" "${got}" "--sort=no")
ctags_keyedopts(got -e --sort=no)
expect("keyed -e" "${got}" "-e;--sort=no;--tag-relative=no")
ctags_keyedopts(got --output-format=etags)
expect("keyed etags" "${got}" "--output-format=etags;--tag-relative=no")

if(failed)
  message(FATAL_ERROR "${failed} caseopts checks failed")
endif()
