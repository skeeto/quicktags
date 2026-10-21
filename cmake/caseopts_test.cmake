# Tests of caseopts.cmake, the choice of a Units case's options.
#   $ cmake -P caseopts_test.cmake

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
    "+K{inherits}=--fields=+K"
    "+K=--fields=+K"
    "+e="
    "+iaS="
    "e=--fields="
    "=--fields="
    "+K-e+n=--fields=+Kn"
    "Ke+n=--fields=Kn"
    "-S+n=--fields=+n"
    "-k=--fields=-k"
    "+e-n=--fields=-n"
    "+{end}{scope}=--fields=+{scope}"
    "+{signature}-{nth}="
    "S+n=--fields=n"
    "e+n=--fields=n"
    "{end}+K=--fields=K"
    "Se-k=--fields="
    "e-n=--fields="
    "S-n+k=--fields=k"
    "Sn-n=--fields=n-n"
    "*=--fields=NFPCEKRTZfklnprstxz"
    "*-S=--fields=NFPCEKRTZfklnprstxz"
    "*-n=--fields=NFPCEKRTZfklnprstxz-n")
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
    "--extras=+q" "-n")
  ctags_stripped(got "${opt}")
  expect("keep ${opt}" "${got}" OFF)
endforeach()

# What each mode keeps
set(lines
  "--kinds-C++=+p" "  --extras=+qr" "--extras=+r" "--fields=+K{inherits}"
  "--sort=no" "# a comment" "--KINDS-c=+l" "--C++-kinds=+z"
  "--kinds-CPreProcessor=+d" "--fields-C++=+{template}" "")
ctags_caseopts("${lines}")
expect("args" "${ctags_args}"
  "--kinds-C++=+p;--extras=+q;--KINDS-c=+l;--C++-kinds=+z")
expect("fmt" "${ctags_fmt}" "--fields=+K")

set(formats
  "-n" "-N" "-nN" "--excmd=combine" "--format=1" "--pattern-length-limit=9"
  "--output-format=e-ctags" "--output-format=etags" "--fields=+e" "-e")
ctags_caseopts("${formats}")
expect("args formats" "${ctags_args}" "")
expect("fmt formats" "${ctags_fmt}"
  "-n;-N;--excmd=combine;--format=1;--pattern-length-limit=9;--output-format=e-ctags")

set(ctags_fmt_keep "^--fields=" "^--sort=")
ctags_caseopts("${lines}")
expect("fmt kept" "${ctags_fmt}" "--fields=+K;--sort=no")

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
