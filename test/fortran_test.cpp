// Fortran: tag cases

// Each case names the rule it shows (docs/spec.md, FTN-*), or the fix
// (F*, R*) for IMPROVED cases.
static TagCase fortrancases[] = {
    {SHARED, "program", "x.f90", "",
     "program hello\n"
     "end program hello\n",
     "hello\tx.f90\t/^program hello$/;\"\tp\tline:1\n",
     ""},
    {SHARED, "FTN-1 code from column 7 stays fixed form", "x.f90", "",
     "C     comment\n"
     "      program main\n"
     "      integer i\n"
     "      end\n",
     "i\tx.f90\t/^      integer i$/;\"\tv\tline:3\tprogram:main\n"
     "main\tx.f90\t/^      program main$/;\"\tp\tline:2\n",
     ""},
    {SHARED, "FTN-1 a letter in column 1 is free form", "x.f", "",
     "module m\n"
     "  integer :: n\n"
     "end module m\n",
     "m\tx.f\t/^module m$/;\"\tm\tline:1\n"
     "n\tx.f\t/^  integer :: n$/;\"\tv\tline:2\tmodule:m\n",
     ""},
    {SHARED, "FTN-1 an ampersand ending a line is free form", "x.f", "",
     "      subroutine s(a, &\n"
     "      b)\n"
     "10    continue\n"
     "      end\n",
     "10\tx.f\t/^10    continue$/;\"\tl\tline:3\tfile:\n"
     "s\tx.f\t/^      subroutine s(/;\"\ts\tline:1\n",
     ""},
    {SHARED, "FTN-1 a line end in a string is free form", "x.f", "",
     "      program p\n"
     "      print *, 'abc\n"
     "10    continue\n"
     "      end\n",
     "p\tx.f\t/^      program p$/;\"\tp\tline:1\n",
     ""},
    {SHARED, "FTN-1 a label cannot head a continuation line", "x.f", "",
     "      program p\n"
     "   10&continue\n"
     "      end\n",
     "10\tx.f\t/^   10&continue$/;\"\tl\tline:2\tprogram:p\tfile:\n"
     "p\tx.f\t/^      program p$/;\"\tp\tline:1\n",
     ""},
    {SHARED, "FTN-2 continuation lines join, and labels are tagged", "x.f", "",
     "      subroutine longna\n"
     "     &me(a)\n"
     "10    continue\n"
     "   20 continue\n"
     "      end\n",
     "10\tx.f\t/^10    continue$/;\"\tl\tline:3\tfile:\n"
     "20\tx.f\t/^   20 continue$/;\"\tl\tline:4\tsubroutine:longname\tfile:\n"
     "longname\tx.f\t/^      subroutine longna$/;\"\ts\tline:1\n",
     ""},
    {SHARED, "FTN-2 blanks before continued text read as a space", "x.f", "",
     "      integer abc\n"
     "     +   def, g\n"
     "     +hi\n"
     "      end\n",
     "abc\tx.f\t/^      integer abc$/;\"\tv\tline:1\n"
     "def\tx.f\t/^     +   def,/;\"\tv\tline:2\n"
     "ghi\tx.f\t/^     +   def, g$/;\"\tv\tline:2\n",
     ""},
    {SHARED, "FTN-2 comment lines, inline comments, and debug lines", "x.f", "",
     "* comment\n"
     "c comment\n"
     "d     integer debug\n"
     "#define X\n"
     "      program p ! comment\n"
     "      integer a ! b\n"
     "      end\n",
     "a\tx.f\t/^      integer a /;\"\tv\tline:6\tprogram:p\n"
     "p\tx.f\t/^      program p /;\"\tp\tline:5\n",
     ""},
    {SHARED, "FTN-2 no limit at column 72", "x.f", "",
     "      program p\n"
     "      integer a                                                         xyz\n"
     "      end\n",
     "a\tx.f\t/^      integer a /;\"\tv\tline:2\tprogram:p\n"
     "p\tx.f\t/^      program p$/;\"\tp\tline:1\n"
     "xyz\tx.f\t/^      integer a                                                         xyz$/;\"\tv\tline:2\tprogram:p\n",
     ""},
    {SHARED, "FTN-2 a tab ends the margin", "x.f", "",
     "\tprogram tabbed\n"
     "10\tcontinue\n"
     "\tend\n",
     "10\tx.f\t/^10\tcontinue$/;\"\tl\tline:2\tfile:\n"
     "tabbed\tx.f\t/^\tprogram tabbed$/;\"\tp\tline:1\n",
     ""},
    {SHARED, "FTN-2 an anonymous tag takes the next line in fixed form", "x.f", "",
     "      module m\n"
     "      interface\n"
     "\n"
     "      subroutine s\n"
     "      end subroutine\n"
     "      end interface\n"
     "      end module\n",
     "__anon0b88b7b10105\tx.f\t/^      subroutine s$/;\"\ti\tline:4\tmodule:m\n"
     "m\tx.f\t/^      module m$/;\"\tm\tline:1\n",
     ""},
    {SHARED, "FTN-3 continuation skips comments and a leading ampersand", "x.f90", "--kinds-Fortran=+L",
     "subroutine s(a, & ! first\n"
     "! comment line\n"
     "\n"
     "    & b)\n"
     "  integer :: a, &\n"
     "#ifdef X\n"
     "             b\n"
     "end subroutine\n",
     "a\tx.f90\t/^  integer :: a,/;\"\tL\tline:5\tsubroutine:s\tfile:\n"
     "b\tx.f90\t/^             b$/;\"\tL\tline:7\tsubroutine:s\tfile:\n"
     "s\tx.f90\t/^subroutine s(a, & ! first/;\"\ts\tline:1\n",
     ""},
    {SHARED, "FTN-3 semicolons separate statements", "x.f90", "",
     "module m; integer :: a; end module m\n"
     "module n\n"
     "end module\n",
     "a\tx.f90\t/^module m; integer :: a;/;\"\tv\tline:1\tmodule:m\n"
     "m\tx.f90\t/^module m; integer :: a; end module m$/;\"\tm\tline:1\n"
     "n\tx.f90\t/^module n$/;\"\tm\tline:2\n",
     ""},
    {SHARED, "FTN-3 labels follow a line end, up to five digits", "x.f90", "",
     "program p\n"
     "10 continue\n"
     " 123456 continue\n"
     "x = 1; 20 continue\n"
     "30 continue ! a\n"
     "end program\n",
     "10\tx.f90\t/^10 continue$/;\"\tl\tline:2\tprogram:p\tfile:\n"
     "12345\tx.f90\t/^ 123456 continue$/;\"\tl\tline:3\tprogram:p\tfile:\n"
     "30\tx.f90\t/^30 continue ! a$/;\"\tl\tline:5\tprogram:p\tfile:\n"
     "p\tx.f90\t/^program p$/;\"\tp\tline:1\n",
     ""},
    {SHARED, "FTN-3 a comment ending in an ampersand swallows a line", "x.f90", "",
     "module m\n"
     "  integer :: a ! note &\n"
     "  integer :: b\n"
     "  integer :: c\n"
     "end module m\n",
     "a\tx.f90\t/^  integer :: a /;\"\tv\tline:2\tmodule:m\n"
     "c\tx.f90\t/^  integer :: c$/;\"\tv\tline:4\tmodule:m\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n",
     ""},
    {SHARED, "FTN-3 a hash only in column 1 is a comment", "x.f90", "",
     "module m\n"
     "#ifdef A\n"
     "  integer :: a\n"
     "#else\n"
     "  integer :: b\n"
     "#endif\n"
     "  include 'x.inc'\n"
     "  integer :: c\n"
     "end module\n",
     "a\tx.f90\t/^  integer :: a$/;\"\tv\tline:3\tmodule:m\n"
     "b\tx.f90\t/^  integer :: b$/;\"\tv\tline:5\tmodule:m\n"
     "c\tx.f90\t/^  integer :: c$/;\"\tv\tline:8\tmodule:m\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n",
     ""},
    {SHARED, "FTN-4 keywords ignore case and name things", "x.f90", "",
     "MODULE Mixed\n"
     "  INTEGER :: Value1\n"
     "  Type :: Type\n"
     "    integer :: integer\n"
     "  END TYPE\n"
     "CONTAINS\n"
     "  FUNCTION Fn(X)\n"
     "  END FUNCTION\n"
     "END MODULE\n",
     "Fn\tx.f90\t/^  FUNCTION Fn(/;\"\tf\tline:7\tmodule:Mixed\n"
     "Mixed\tx.f90\t/^MODULE Mixed$/;\"\tm\tline:1\n"
     "Type\tx.f90\t/^  Type :: Type$/;\"\tt\tline:3\tmodule:Mixed\n"
     "Value1\tx.f90\t/^  INTEGER :: Value1$/;\"\tv\tline:2\tmodule:Mixed\n"
     "integer\tx.f90\t/^    integer :: integer$/;\"\tk\tline:4\ttype:Type\n",
     ""},
    {SHARED, "FTN-4 identifiers may hold digits, underscores, and dollars", "x.f90", "",
     "module m\n"
     "  integer :: a_1, b$2, c3\n"
     "end module\n",
     "a_1\tx.f90\t/^  integer :: a_1,/;\"\tv\tline:2\tmodule:m\n"
     "b$2\tx.f90\t/^  integer :: a_1, b$2,/;\"\tv\tline:2\tmodule:m\n"
     "c3\tx.f90\t/^  integer :: a_1, b$2, c3$/;\"\tv\tline:2\tmodule:m\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n",
     ""},
    {IMPROVED, "F5 a joined END ignores case", "x.f90", "",
     "module m\n"
     "contains\n"
     "  subroutine a\n"
     "  endsubroutine\n"
     "  subroutine b\n"
     "  end subroutine\n"
     "  SUBROUTINE C\n"
     "  ENDSUBROUTINE\n"
     "  SUBROUTINE D\n"
     "  END SUBROUTINE\n"
     "end module\n",
     "C\tx.f90\t/^  SUBROUTINE C$/;\"\ts\tline:7\tmodule:m\n"
     "D\tx.f90\t/^  SUBROUTINE D$/;\"\ts\tline:9\tmodule:m\n"
     "a\tx.f90\t/^  subroutine a$/;\"\ts\tline:3\tmodule:m\n"
     "b\tx.f90\t/^  subroutine b$/;\"\ts\tline:5\tmodule:m\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n",
     "C\tx.f90\t/^  SUBROUTINE C$/;\"\ts\tline:7\n"
     "a\tx.f90\t/^  subroutine a$/;\"\ts\tline:3\tmodule:m\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"},
    {IMPROVED, "F5 a joined END ends a specification part", "x.f90", "",
     "module m\n"
     "  integer :: x\n"
     "endmodule\n"
     "module n\n"
     "end module\n"
     "pure subroutine s()\n"
     "  integer :: y\n"
     "ENDSUBROUTINE\n"
     "subroutine t\n"
     "end subroutine\n",
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"
     "n\tx.f90\t/^module n$/;\"\tm\tline:4\n"
     "s\tx.f90\t/^pure subroutine s(/;\"\ts\tline:6\n"
     "t\tx.f90\t/^subroutine t$/;\"\ts\tline:9\n"
     "x\tx.f90\t/^  integer :: x$/;\"\tv\tline:2\tmodule:m\n",
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"
     "s\tx.f90\t/^pure subroutine s(/;\"\ts\tline:6\n"
     "x\tx.f90\t/^  integer :: x$/;\"\tv\tline:2\tmodule:m\n"},
    {IMPROVED, "F3 a name may start with an underscore", "x.f90", "",
     "module __m\n"
     "  integer :: x, _y\n"
     "  interface __g\n"
     "  end interface\n"
     "end module\n",
     "__g\tx.f90\t/^  interface __g$/;\"\ti\tline:3\tmodule:__m\n"
     "__m\tx.f90\t/^module __m$/;\"\tm\tline:1\n"
     "_y\tx.f90\t/^  integer :: x, _y$/;\"\tv\tline:2\tmodule:__m\n"
     "x\tx.f90\t/^  integer :: x,/;\"\tv\tline:2\tmodule:__m\n",
     "x\tx.f90\t/^  integer :: x,/;\"\tv\tline:2\n"},
    {SHARED, "FTN-5 locals and prototypes are off by default", "x.f90", "",
     "subroutine s(a)\n"
     "  integer :: a\n"
     "  interface\n"
     "    function f(x)\n"
     "    end function\n"
     "  end interface\n"
     "end subroutine\n",
     "__anon10956f7a0105\tx.f90\t/^  interface$/;\"\ti\tline:3\tsubroutine:s\n"
     "s\tx.f90\t/^subroutine s(/;\"\ts\tline:1\n",
     ""},
    {SHARED, "FTN-5 all kinds", "x.f90", "--kinds-Fortran=*",
     "subroutine s(a)\n"
     "  integer :: a\n"
     "  interface\n"
     "    function f(x)\n"
     "      real :: x\n"
     "    end function\n"
     "  end interface\n"
     "end subroutine\n",
     "__anon10956f7a0105\tx.f90\t/^  interface$/;\"\ti\tline:3\tsubroutine:s\n"
     "a\tx.f90\t/^  integer :: a$/;\"\tL\tline:2\tsubroutine:s\tfile:\n"
     "f\tx.f90\t/^    function f(/;\"\tP\tline:4\tinterface:__anon10956f7a0105\n"
     "s\tx.f90\t/^subroutine s(/;\"\ts\tline:1\n"
     "x\tx.f90\t/^      real :: x$/;\"\tL\tline:5\tprototype:f\tfile:\n",
     ""},
    {SHARED, "FTN-6 entity kinds follow the enclosing unit", "x.f90", "--kinds-Fortran=*",
     "module m\n"
     "  integer :: mv\n"
     "  type :: t\n"
     "    integer :: comp\n"
     "    procedure(f), pointer :: pp\n"
     "  contains\n"
     "    procedure :: meth\n"
     "    final :: fin\n"
     "    generic :: gen => meth\n"
     "    generic :: operator(+) => meth\n"
     "  end type\n"
     "  enum, bind(c)\n"
     "    enumerator :: red = 1, blue\n"
     "  end enum\n"
     "contains\n"
     "  subroutine s(arg)\n"
     "    integer :: arg, loc\n"
     "  end subroutine\n"
     "end module\n"
     "program p\n"
     "  integer :: pv\n"
     "end program\n"
     "integer :: implied\n",
     "+\tx.f90\t/^    generic :: operator(+)/;\"\tM\tline:10\ttype:t\n"
     "__anon10956f7a0103\tx.f90\t/^  enum, bind(c)$/;\"\tE\tline:12\tmodule:m\n"
     "arg\tx.f90\t/^    integer :: arg,/;\"\tL\tline:17\tsubroutine:s\tfile:\n"
     "blue\tx.f90\t/^    enumerator :: red = 1, blue$/;\"\tN\tline:13\tenum:__anon10956f7a0103\n"
     "comp\tx.f90\t/^    integer :: comp$/;\"\tk\tline:4\ttype:t\n"
     "fin\tx.f90\t/^    final :: fin$/;\"\tM\tline:8\ttype:t\n"
     "gen\tx.f90\t/^    generic :: gen /;\"\tM\tline:9\ttype:t\n"
     "implied\tx.f90\t/^integer :: implied$/;\"\tv\tline:23\n"
     "loc\tx.f90\t/^    integer :: arg, loc$/;\"\tL\tline:17\tsubroutine:s\tfile:\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"
     "meth\tx.f90\t/^    procedure :: meth$/;\"\tM\tline:7\ttype:t\n"
     "mv\tx.f90\t/^  integer :: mv$/;\"\tv\tline:2\tmodule:m\n"
     "p\tx.f90\t/^program p$/;\"\tp\tline:20\n"
     "pp\tx.f90\t/^    procedure(f), pointer :: pp$/;\"\tM\tline:5\ttype:t\n"
     "pv\tx.f90\t/^  integer :: pv$/;\"\tv\tline:21\tprogram:p\n"
     "red\tx.f90\t/^    enumerator :: red /;\"\tN\tline:13\tenum:__anon10956f7a0103\n"
     "s\tx.f90\t/^  subroutine s(/;\"\ts\tline:16\tmodule:m\n"
     "t\tx.f90\t/^  type :: t$/;\"\tt\tline:3\tmodule:m\n",
     ""},
    {SHARED, "FTN-6 type declarations: specifiers, attributes, initializers", "x.f90", "",
     "module m\n"
     "  character(len=*), parameter :: s1 = 'a', s2 = 'b'\n"
     "  character*8 c8, c9*4\n"
     "  double precision :: dp\n"
     "  real(kind=8), dimension(3) :: arr = [1, 2, 3], arr2(2, 2)\n"
     "  integer, pointer :: p => null(), q\n"
     "  class(*), allocatable :: anything\n"
     "  type(point), target :: pt\n"
     "  logical*1, save :: flag\n"
     "  integer, intent(in), optional :: io\n"
     "  integer :: after\n"
     "end module\n",
     "after\tx.f90\t/^  integer :: after$/;\"\tv\tline:11\tmodule:m\n"
     "anything\tx.f90\t/^  class(*), allocatable :: anything$/;\"\tv\tline:7\tmodule:m\n"
     "arr\tx.f90\t/^  real(kind=8), dimension(3) :: arr = [1, 2, 3], arr2/;\"\tv\tline:5\tmodule:m\n"
     "arr2\tx.f90\t/^  real(kind=8), dimension(3) :: arr = [1, 2, 3], arr2(/;\"\tv\tline:5\tmodule:m\n"
     "c8\tx.f90\t/^  character*8 c8,/;\"\tv\tline:3\tmodule:m\n"
     "c9\tx.f90\t/^  character*8 c8, c9*/;\"\tv\tline:3\tmodule:m\n"
     "dp\tx.f90\t/^  double precision :: dp$/;\"\tv\tline:4\tmodule:m\n"
     "flag\tx.f90\t/^  logical*1, save :: flag$/;\"\tv\tline:9\tmodule:m\n"
     "io\tx.f90\t/^  integer, intent(in), optional :: io$/;\"\tv\tline:10\tmodule:m\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"
     "p\tx.f90\t/^  integer, pointer :: p /;\"\tv\tline:6\tmodule:m\n"
     "pt\tx.f90\t/^  type(point), target :: pt$/;\"\tv\tline:8\tmodule:m\n"
     "q\tx.f90\t/^  integer, pointer :: p => null(), q$/;\"\tv\tline:6\tmodule:m\n"
     "s1\tx.f90\t/^  character(len=*), parameter :: s1 /;\"\tv\tline:2\tmodule:m\n"
     "s2\tx.f90\t/^  character(len=*), parameter :: s1 = 'a', s2 /;\"\tv\tline:2\tmodule:m\n",
     ""},
    {SHARED, "FTN-6 statements that skip without tags", "x.f90", "",
     "module m\n"
     "  use iso_c_binding\n"
     "  implicit none\n"
     "  parameter (n = 3)\n"
     "  dimension a(10)\n"
     "  external f\n"
     "  public :: g\n"
     "  data a /10*0/\n"
     "  integer :: kept\n"
     "end module\n",
     "kept\tx.f90\t/^  integer :: kept$/;\"\tv\tline:9\tmodule:m\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n",
     ""},
    {SHARED, "FTN-6 common blocks and namelists", "x.f90", "--kinds-Fortran=*",
     "subroutine s\n"
     "  common /blk/ a, b(10), /other/ c\n"
     "  common d\n"
     "  namelist /nl/ a, b\n"
     "end subroutine\n",
     "a\tx.f90\t/^  common \\/blk\\/ a,/;\"\tL\tline:2\tsubroutine:s\tfile:\n"
     "a\tx.f90\t/^  namelist \\/nl\\/ a,/;\"\tL\tline:4\tsubroutine:s\tfile:\n"
     "b\tx.f90\t/^  common \\/blk\\/ a, b(/;\"\tL\tline:2\tsubroutine:s\tfile:\n"
     "b\tx.f90\t/^  namelist \\/nl\\/ a, b$/;\"\tL\tline:4\tsubroutine:s\tfile:\n"
     "blk\tx.f90\t2;\"\tc\tline:2\tsubroutine:s\n"
     "c\tx.f90\t/^  common \\/blk\\/ a, b(10), \\/other\\/ c$/;\"\tL\tline:2\tsubroutine:s\tfile:\n"
     "d\tx.f90\t/^  common d$/;\"\tL\tline:3\tsubroutine:s\tfile:\n"
     "nl\tx.f90\t/^  namelist \\/nl\\//;\"\tn\tline:4\tsubroutine:s\n"
     "other\tx.f90\t2;\"\tc\tline:2\tsubroutine:s\n"
     "s\tx.f90\t/^subroutine s$/;\"\ts\tline:1\n",
     ""},
    {SHARED, "FTN-6 block data and entry points", "x.f90", "",
     "block data init\n"
     "  common /c/ x\n"
     "  integer :: y\n"
     "end block data\n"
     "block data\n"
     "  common /d/ z\n"
     "end block data\n"
     "subroutine s\n"
     "  x = 1\n"
     "entry e\n"
     "end subroutine\n",
     "c\tx.f90\t2;\"\tc\tline:2\tblockData:init\n"
     "d\tx.f90\t6;\"\tc\tline:6\n"
     "e\tx.f90\t/^entry e$/;\"\te\tline:10\tsubroutine:s\n"
     "init\tx.f90\t/^block data init$/;\"\tb\tline:1\n"
     "s\tx.f90\t/^subroutine s$/;\"\ts\tline:8\n"
     "y\tx.f90\t/^  integer :: y$/;\"\tv\tline:3\tblockData:init\n",
     ""},
    {SHARED, "FTN-6 submodules", "x.f90", "",
     "submodule (m) sm\n"
     "  integer :: x\n"
     "end submodule\n"
     "submodule (m:sm) sm2\n"
     "end submodule\n"
     "submodule () bad\n"
     "end submodule\n",
     "sm\tx.f90\t/^submodule (m) sm$/;\"\tS\tline:1\n"
     "sm2\tx.f90\t/^submodule (m:sm) sm2$/;\"\tS\tline:4\n"
     "x\tx.f90\t/^  integer :: x$/;\"\tv\tline:2\tsubmodule:sm\n",
     ""},
    {SHARED, "FTN-6 DEC structures, unions, and maps", "x.f", "--kinds-Fortran=*",
     "      program p\n"
     "      structure /point/\n"
     "        integer x, y\n"
     "        structure /inner/ in1, in2\n"
     "          real z\n"
     "        end structure\n"
     "        union\n"
     "          map\n"
     "            integer m1\n"
     "          end map\n"
     "          map\n"
     "            real m2\n"
     "          end map\n"
     "        end union\n"
     "      end structure\n"
     "      structure nameless\n"
     "        integer q /1/, %fill\n"
     "      end structure\n"
     "      record /point/ r\n"
     "      end\n",
     "__anon0b88b7b10106\tx.f\t/^      structure nameless$/;\"\tt\tline:16\tprogram:p\n"
     "in1\tx.f\t/^        structure \\/inner\\/ in1,/;\"\tk\tline:4\ttype:point\n"
     "in2\tx.f\t/^        structure \\/inner\\/ in1, in2$/;\"\tk\tline:4\ttype:point\n"
     "inner\tx.f\t/^        structure \\/inner\\//;\"\tt\tline:4\ttype:point\n"
     "m1\tx.f\t/^            integer m1$/;\"\tk\tline:9\ttype:point\n"
     "m2\tx.f\t/^            real m2$/;\"\tk\tline:12\ttype:point\n"
     "nameless\tx.f\t/^      structure nameless$/;\"\tk\tline:16\tprogram:p\n"
     "p\tx.f\t/^      program p$/;\"\tp\tline:1\n"
     "point\tx.f\t/^      structure \\/point\\//;\"\tt\tline:2\tprogram:p\n"
     "q\tx.f\t/^        integer q /;\"\tk\tline:17\ttype:__anon0b88b7b10106\n"
     "r\tx.f\t/^      record \\/point\\/ r$/;\"\tv\tline:19\tprogram:p\n"
     "x\tx.f\t/^        integer x,/;\"\tk\tline:3\ttype:point\n"
     "y\tx.f\t/^        integer x, y$/;\"\tk\tline:3\ttype:point\n"
     "z\tx.f\t/^          real z$/;\"\tk\tline:5\ttype:inner\n",
     ""},
    {SHARED, "FTN-6 enums", "x.f90", "",
     "module m\n"
     "  enum, bind(c)\n"
     "    enumerator :: a = 1, b\n"
     "  end enum\n"
     "  enum, bind(c) :: named\n"
     "    enumerator c\n"
     "  end enum\n"
     "  enumerator :: outside\n"
     "end module\n",
     "__anon10956f7a0103\tx.f90\t/^  enum, bind(c)$/;\"\tE\tline:2\tmodule:m\n"
     "a\tx.f90\t/^    enumerator :: a /;\"\tN\tline:3\tenum:__anon10956f7a0103\n"
     "b\tx.f90\t/^    enumerator :: a = 1, b$/;\"\tN\tline:3\tenum:__anon10956f7a0103\n"
     "c\tx.f90\t/^    enumerator c$/;\"\tN\tline:6\tenum:named\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"
     "named\tx.f90\t/^  enum, bind(c) :: named$/;\"\tE\tline:5\tmodule:m\n"
     "outside\tx.f90\t/^  enumerator :: outside$/;\"\tv\tline:8\tmodule:m\n",
     ""},
    {SHARED, "FTN-6 interfaces", "x.f90", "--kinds-Fortran=*",
     "module m\n"
     "  interface gen\n"
     "    module procedure a, b\n"
     "  end interface\n"
     "  interface operator(.plus.)\n"
     "    pure integer function plus(x, y)\n"
     "      integer, intent(in) :: x, y\n"
     "    end function\n"
     "  end interface\n"
     "  interface assignment(=)\n"
     "    subroutine assign(a, b)\n"
     "    end subroutine\n"
     "  end interface\n"
     "  abstract interface\n"
     "    subroutine cb()\n"
     "    end subroutine\n"
     "  end interface\n"
     "end module\n",
     ".plus.\tx.f90\t/^  interface operator(.plus.)/;\"\ti\tline:5\tmodule:m\n"
     "=\tx.f90\t/^  interface assignment(=)/;\"\ti\tline:10\tmodule:m\n"
     "__anon10956f7a0105\tx.f90\t/^  abstract interface$/;\"\ti\tline:14\tmodule:m\n"
     "assign\tx.f90\t/^    subroutine assign(/;\"\tP\tline:11\tmodule:m\n"
     "cb\tx.f90\t/^    subroutine cb(/;\"\tP\tline:15\tinterface:__anon10956f7a0105\n"
     "gen\tx.f90\t/^  interface gen$/;\"\ti\tline:2\tmodule:m\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"
     "plus\tx.f90\t/^    pure integer function plus(/;\"\tP\tline:6\tmodule:m\n"
     "x\tx.f90\t/^      integer, intent(in) :: x,/;\"\tL\tline:7\tprototype:plus\tfile:\n"
     "y\tx.f90\t/^      integer, intent(in) :: x, y$/;\"\tL\tline:7\tprototype:plus\tfile:\n",
     ""},
    {SHARED, "FTN-6 type extension and type-bound procedures", "x.f90", "",
     "module m\n"
     "  type, abstract, extends(base) :: child(k)\n"
     "    integer, kind :: k\n"
     "    sequence\n"
     "    integer :: c\n"
     "  contains\n"
     "    procedure, pass(self), deferred :: run\n"
     "    procedure(iface), nopass :: a => impl, b\n"
     "  end type child\n"
     "end module\n",
     "a\tx.f90\t/^    procedure(iface), nopass :: a /;\"\tM\tline:8\ttype:child\n"
     "b\tx.f90\t/^    procedure(iface), nopass :: a => impl, b$/;\"\tM\tline:8\ttype:child\n"
     "c\tx.f90\t/^    integer :: c$/;\"\tk\tline:5\ttype:child\n"
     "child\tx.f90\t/^  type, abstract, extends(base) :: child(/;\"\tt\tline:2\tmodule:m\n"
     "k\tx.f90\t/^    integer, kind :: k$/;\"\tk\tline:3\ttype:child\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"
     "run\tx.f90\t/^    procedure, pass(self), deferred :: run$/;\"\tM\tline:7\ttype:child\n",
     ""},
    {SHARED, "FTN-6 subprogram prefixes and typed functions", "x.f90", "",
     "pure function a()\n"
     "end function\n"
     "elemental real function b(x)\n"
     "end function\n"
     "recursive integer(8) function c(n) result(r)\n"
     "end function\n"
     "double precision function d()\n"
     "end function\n"
     "character(len=*) function e()\n"
     "end function\n",
     "a\tx.f90\t/^pure function a(/;\"\tf\tline:1\n"
     "b\tx.f90\t/^elemental real function b(/;\"\tf\tline:3\n"
     "c\tx.f90\t/^recursive integer(8) function c(/;\"\tf\tline:5\n"
     "d\tx.f90\t/^double precision function d(/;\"\tf\tline:7\n"
     "e\tx.f90\t/^character(len=*) function e(/;\"\tf\tline:9\n",
     ""},
    {SHARED, "FTN-6 a statement function ends nothing", "x.f90", "",
     "program p\n"
     "  integer :: a\n"
     "  f(x) = x + 1\n"
     "  integer :: b\n"
     "  call g(a)\n"
     "  integer :: c\n"
     "end program\n",
     "a\tx.f90\t/^  integer :: a$/;\"\tv\tline:2\tprogram:p\n"
     "b\tx.f90\t/^  integer :: b$/;\"\tv\tline:4\tprogram:p\n"
     "p\tx.f90\t/^program p$/;\"\tp\tline:1\n",
     ""},
    {SHARED, "FTN-6 the execution part stops at the unit's END", "x.f90", "",
     "subroutine s\n"
     "  do i = 1, 3\n"
     "  end do\n"
     "  if (x) then\n"
     "  endif\n"
     "  block\n"
     "    integer :: inblock\n"
     "  end block\n"
     "  select case (i)\n"
     "  end select\n"
     "  associate (a => b)\n"
     "  end associate\n"
     "  entry e2\n"
     "contains\n"
     "  subroutine inner\n"
     "  end subroutine\n"
     "end subroutine\n"
     "subroutine t\n"
     "end subroutine\n",
     "e2\tx.f90\t/^  entry e2$/;\"\te\tline:13\tsubroutine:s\n"
     "inner\tx.f90\t/^  subroutine inner$/;\"\ts\tline:15\tsubroutine:s\n"
     "s\tx.f90\t/^subroutine s$/;\"\ts\tline:1\n"
     "t\tx.f90\t/^subroutine t$/;\"\ts\tline:18\n",
     ""},
    {SHARED, "FTN-7 scopes are the nearest named unit", "x.f90", "--kinds-Fortran=*",
     "program p\n"
     "  integer :: v\n"
     "contains\n"
     "  function f(x)\n"
     "    real :: x\n"
     "  contains\n"
     "    subroutine g\n"
     "      integer :: y\n"
     "    end subroutine\n"
     "  end function\n"
     "end program\n",
     "f\tx.f90\t/^  function f(/;\"\tf\tline:4\tprogram:p\n"
     "g\tx.f90\t/^    subroutine g$/;\"\ts\tline:7\tfunction:f\n"
     "p\tx.f90\t/^program p$/;\"\tp\tline:1\n"
     "v\tx.f90\t/^  integer :: v$/;\"\tv\tline:2\tprogram:p\n"
     "x\tx.f90\t/^    real :: x$/;\"\tL\tline:5\tfunction:f\tfile:\n"
     "y\tx.f90\t/^      integer :: y$/;\"\tL\tline:8\tsubroutine:g\tfile:\n",
     ""},
    {IMPROVED, "F6 a subprogram named by a keyword is a scope", "x.f90", "--kinds-Fortran=*",
     "module m\n"
     "contains\n"
     "  function sequence(n)\n"
     "    integer :: n\n"
     "  end function\n"
     "  subroutine inline\n"
     "  contains\n"
     "    subroutine g\n"
     "    end subroutine\n"
     "  end subroutine\n"
     "end module\n",
     "g\tx.f90\t/^    subroutine g$/;\"\ts\tline:8\tsubroutine:inline\n"
     "inline\tx.f90\t/^  subroutine inline$/;\"\ts\tline:6\tmodule:m\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"
     "n\tx.f90\t/^    integer :: n$/;\"\tL\tline:4\tfunction:sequence\tfile:\n"
     "sequence\tx.f90\t/^  function sequence(/;\"\tf\tline:3\tmodule:m\n",
     "g\tx.f90\t/^    subroutine g$/;\"\ts\tline:8\tmodule:m\n"
     "inline\tx.f90\t/^  subroutine inline$/;\"\ts\tline:6\tmodule:m\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"
     "n\tx.f90\t/^    integer :: n$/;\"\tL\tline:4\tmodule:m\tfile:\n"
     "sequence\tx.f90\t/^  function sequence(/;\"\tf\tline:3\tmodule:m\n"},
    {SHARED, "FTN-7 a subroutine's first label is outside it", "x.f90", "",
     "subroutine s(a)\n"
     "10 continue\n"
     "20 continue\n"
     "end subroutine\n"
     "function f(a)\n"
     "30 continue\n"
     "end function\n",
     "10\tx.f90\t/^10 continue$/;\"\tl\tline:2\tfile:\n"
     "20\tx.f90\t/^20 continue$/;\"\tl\tline:3\tsubroutine:s\tfile:\n"
     "30\tx.f90\t/^30 continue$/;\"\tl\tline:6\tfunction:f\tfile:\n"
     "f\tx.f90\t/^function f(/;\"\tf\tline:5\n"
     "s\tx.f90\t/^subroutine s(/;\"\ts\tline:1\n",
     ""},
    {SHARED, "FTN-8 patterns end after the last occurrence of the name", "x.f90", "",
     "module m\n"
     "  integer, parameter :: n = 1, nn = n\n"
     "  integer :: x ! x\n"
     "end module\n",
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"
     "n\tx.f90\t/^  integer, parameter :: n = 1, nn = n$/;\"\tv\tline:2\tmodule:m\n"
     "nn\tx.f90\t/^  integer, parameter :: n = 1, nn /;\"\tv\tline:2\tmodule:m\n"
     "x\tx.f90\t/^  integer :: x ! x$/;\"\tv\tline:3\tmodule:m\n",
     ""},
    {SHARED, "FTN-8 a name on a continuation line", "x.f90", "",
     "module m\n"
     "  procedure(sub), pointer :: one => null(), &\n"
     "    two => null()\n"
     "end module\n",
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"
     "one\tx.f90\t/^  procedure(sub), pointer :: one /;\"\tv\tline:2\tmodule:m\n"
     "two\tx.f90\t/^    two /;\"\tv\tline:3\tmodule:m\n",
     ""},
    {SHARED, "FTN-9 anonymous names count on from a failed fixed pass", "x.f", "",
     "      module m\n"
     "      interface\n"
     "      end interface\n"
     "      enum, bind(c)\n"
     "      end enum\n"
     "end module\n",
     "__anon0b88b7b10305\tx.f\t/^      interface$/;\"\ti\tline:2\tmodule:m\n"
     "__anon0b88b7b10403\tx.f\t/^      enum, bind(c)$/;\"\tE\tline:4\tmodule:m\n"
     "m\tx.f\t/^      module m$/;\"\tm\tline:1\n",
     ""},
    {SHARED, "FTN-9 disabled kinds advance the anonymous counter", "x.f90", "--kinds-Fortran=-i",
     "module m\n"
     "  interface\n"
     "  end interface\n"
     "  enum, bind(c)\n"
     "    enumerator :: e\n"
     "  end enum\n"
     "end module\n",
     "__anon10956f7a0203\tx.f90\t/^  enum, bind(c)$/;\"\tE\tline:4\tmodule:m\n"
     "e\tx.f90\t/^    enumerator :: e$/;\"\tN\tline:5\tenum:__anon10956f7a0203\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n",
     ""},
    {SHARED, "FTN-1 a character read ahead by a failed fixed pass starts the free pass", "x.f", "",
     "      type :: t\n"
     " bad\n"
     "      procedure(f) :: ff\n",
     "",
     ""},
    {SHARED, "FTN-6 a bare END after a type body is a variable", "x.f90", "",
     "module m\n"
     "  type t\n"
     "    integer :: x\n"
     "  end\n"
     "end module\n",
     "end\tx.f90\t/^  end$/;\"\tv\tline:4\tmodule:m\n"
     "end\tx.f90\t/^end /;\"\tv\tline:5\tmodule:m\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"
     "module\tx.f90\t/^end module$/;\"\tv\tline:5\tmodule:m\n"
     "t\tx.f90\t/^  type t$/;\"\tt\tline:2\tmodule:m\n"
     "x\tx.f90\t/^    integer :: x$/;\"\tk\tline:3\ttype:t\n",
     ""},
    {IMPROVED, "F7 a GENERIC statement outside a type declares an interface", "x.f90", "",
     "module m\n"
     "  generic :: g => a, b\n"
     "  generic, public :: operator(.plus.) => a\n"
     "  generic :: assignment(=) => b\n"
     "  integer :: x\n"
     "  type t\n"
     "  contains\n"
     "    generic :: tg => a\n"
     "  end type\n"
     "contains\n"
     "  subroutine a()\n"
     "  end subroutine\n"
     "end module\n",
     ".plus.\tx.f90\t/^  generic, public :: operator(.plus.)/;\"\ti\tline:3\tmodule:m\n"
     "=\tx.f90\t/^  generic :: assignment(=) =>/;\"\ti\tline:4\tmodule:m\n"
     "a\tx.f90\t/^  subroutine a(/;\"\ts\tline:11\tmodule:m\n"
     "g\tx.f90\t/^  generic :: g /;\"\ti\tline:2\tmodule:m\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"
     "t\tx.f90\t/^  type t$/;\"\tt\tline:6\tmodule:m\n"
     "tg\tx.f90\t/^    generic :: tg /;\"\tM\tline:8\ttype:t\n"
     "x\tx.f90\t/^  integer :: x$/;\"\tv\tline:5\tmodule:m\n",
     "a\tx.f90\t/^  subroutine a(/;\"\ts\tline:11\tmodule:m\n"
     "assignment\tx.f90\t/^  generic :: assignment(/;\"\tv\tline:4\tmodule:m\n"
     "b\tx.f90\t/^  generic :: g => a, b$/;\"\tv\tline:2\tmodule:m\n"
     "g\tx.f90\t/^  generic :: g /;\"\tv\tline:2\tmodule:m\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"
     "operator\tx.f90\t/^  generic, public :: operator(/;\"\tv\tline:3\tmodule:m\n"
     "t\tx.f90\t/^  type t$/;\"\tt\tline:6\tmodule:m\n"
     "tg\tx.f90\t/^    generic :: tg /;\"\tM\tline:8\ttype:t\n"
     "x\tx.f90\t/^  integer :: x$/;\"\tv\tline:5\tmodule:m\n"},
    {IMPROVED, "F8 unknown attributes and attribute statements", "x.f90", "",
     "module m\n"
     "  real, contiguous :: a(:)\n"
     "  integer, volatile, device :: b\n"
     "  real, rank(2), allocatable :: r\n"
     "  asynchronous :: c\n"
     "  integer :: d\n"
     "  common /blk/ d\n"
     "  bind(c, name=\"x\") :: /blk/\n"
     "  contiguous e\n"
     "  codimension f[*]\n"
     "  integer :: g\n"
     "contains\n"
     "  subroutine s\n"
     "  end subroutine\n"
     "end module\n",
     "a\tx.f90\t/^  real, contiguous :: a(/;\"\tv\tline:2\tmodule:m\n"
     "b\tx.f90\t/^  integer, volatile, device :: b$/;\"\tv\tline:3\tmodule:m\n"
     "blk\tx.f90\t7;\"\tc\tline:7\tmodule:m\n"
     "d\tx.f90\t/^  integer :: d$/;\"\tv\tline:6\tmodule:m\n"
     "g\tx.f90\t/^  integer :: g$/;\"\tv\tline:11\tmodule:m\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"
     "r\tx.f90\t/^  real, rank(2), allocatable :: r$/;\"\tv\tline:4\tmodule:m\n"
     "s\tx.f90\t/^  subroutine s$/;\"\ts\tline:13\tmodule:m\n",
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"},
    {IMPROVED, "F1 IMPURE, NON_RECURSIVE, and SIMPLE are prefixes", "x.f90", "",
     "module m\n"
     "contains\n"
     "  impure elemental subroutine a()\n"
     "  end subroutine\n"
     "end module\n"
     "impure function b()\n"
     "end function\n"
     "non_recursive subroutine c()\n"
     "end subroutine\n"
     "simple function d()\n"
     "end function\n"
     "subroutine e()\n"
     "end subroutine\n",
     "a\tx.f90\t/^  impure elemental subroutine a(/;\"\ts\tline:3\tmodule:m\n"
     "b\tx.f90\t/^impure function b(/;\"\tf\tline:6\n"
     "c\tx.f90\t/^non_recursive subroutine c(/;\"\ts\tline:8\n"
     "d\tx.f90\t/^simple function d(/;\"\tf\tline:10\n"
     "e\tx.f90\t/^subroutine e(/;\"\ts\tline:12\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n",
     "a\tx.f90\t/^  impure elemental subroutine a(/;\"\ts\tline:3\tmodule:m\n"
     "e\tx.f90\t/^subroutine e(/;\"\ts\tline:12\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"},
    {IMPROVED, "F1 a prefix inside an interface", "x.f90", "",
     "subroutine s(a)\n"
     "  interface\n"
     "    simple elemental real function f(x)\n"
     "    end function\n"
     "  end interface\n"
     "  real :: a(:)\n"
     "  integer :: simple, impure\n"
     "  simple = 1\n"
     "end subroutine\n"
     "subroutine t\n"
     "end subroutine\n",
     "__anon10956f7a0105\tx.f90\t/^  interface$/;\"\ti\tline:2\tsubroutine:s\n"
     "s\tx.f90\t/^subroutine s(/;\"\ts\tline:1\n"
     "t\tx.f90\t/^subroutine t$/;\"\ts\tline:10\n",
     "__anon10956f7a0105\tx.f90\t/^  interface$/;\"\ti\tline:2\tsubroutine:s\n"
     "a\tx.f90\t/^  real :: a(/;\"\tv\tline:6\n"
     "impure\tx.f90\t/^  integer :: simple, impure$/;\"\tv\tline:7\n"
     "s\tx.f90\t/^subroutine s(/;\"\ts\tline:1\n"
     "simple\tx.f90\t/^  integer :: simple,/;\"\tv\tline:7\n"
     "t\tx.f90\t/^subroutine t$/;\"\ts\tline:10\n"},
    {IMPROVED, "F2 MODULE FUNCTION and MODULE SUBROUTINE in interfaces", "x.f90", "",
     "module m\n"
     "  interface\n"
     "    module function f(x)\n"
     "      integer :: x\n"
     "    end function\n"
     "    pure module subroutine s(y)\n"
     "    end subroutine\n"
     "    integer module function g()\n"
     "    end function\n"
     "  end interface\n"
     "  integer :: after\n"
     "contains\n"
     "  module function f(x)\n"
     "  end function\n"
     "end module\n",
     "__anon10956f7a0105\tx.f90\t/^  interface$/;\"\ti\tline:2\tmodule:m\n"
     "after\tx.f90\t/^  integer :: after$/;\"\tv\tline:11\tmodule:m\n"
     "f\tx.f90\t/^  module function f(/;\"\tf\tline:13\tmodule:m\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n",
     "__anon10956f7a0105\tx.f90\t/^  interface$/;\"\ti\tline:2\tmodule:m\n"
     "after\tx.f90\t/^  integer :: after$/;\"\tv\tline:11\n"
     "function\tx.f90\t/^  module function /;\"\tm\tline:13\n"
     "g\tx.f90\t/^    integer module function g(/;\"\tf\tline:8\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"
     "module\tx.f90\t/^    integer module /;\"\tv\tline:8\n"},
    {IMPROVED, "F2 MODULE as a prefix outside a module", "x.f90", "",
     "module function f(x)\n"
     "  integer :: x\n"
     "end function\n"
     "module subroutine\n"
     "end module\n"
     "module procedure\n"
     "end module\n"
     "module real(8) function g()\n"
     "end function\n",
     "f\tx.f90\t/^module function f(/;\"\tf\tline:1\n"
     "g\tx.f90\t/^module real(8) function g(/;\"\tf\tline:8\n"
     "procedure\tx.f90\t/^module procedure$/;\"\tm\tline:6\n"
     "subroutine\tx.f90\t/^module subroutine$/;\"\tm\tline:4\n",
     "function\tx.f90\t/^module function /;\"\tm\tline:1\n"
     "procedure\tx.f90\t/^module procedure$/;\"\tm\tline:6\n"
     "real\tx.f90\t/^module real(/;\"\tm\tline:8\n"
     "subroutine\tx.f90\t/^module subroutine$/;\"\tm\tline:4\n"
     "x\tx.f90\t/^  integer :: x$/;\"\tv\tline:2\tmodule:function\n"},
    {IMPROVED, "F4 END CRITICAL, END TEAM, and END FILE stay in the unit", "x.f90", "",
     "module m\n"
     "contains\n"
     "  subroutine s\n"
     "    critical\n"
     "    end critical\n"
     "10  continue\n"
     "    change team (t)\n"
     "    End Team\n"
     "20  end file 7\n"
     "30  continue\n"
     "  end subroutine\n"
     "  subroutine t\n"
     "  end subroutine\n"
     "end module\n",
     "10\tx.f90\t/^10  continue$/;\"\tl\tline:6\tsubroutine:s\tfile:\n"
     "20\tx.f90\t/^20  end file 7$/;\"\tl\tline:9\tsubroutine:s\tfile:\n"
     "30\tx.f90\t/^30  continue$/;\"\tl\tline:10\tsubroutine:s\tfile:\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"
     "s\tx.f90\t/^  subroutine s$/;\"\ts\tline:3\tmodule:m\n"
     "t\tx.f90\t/^  subroutine t$/;\"\ts\tline:12\tmodule:m\n",
     "10\tx.f90\t/^10  continue$/;\"\tl\tline:6\tsubroutine:s\tfile:\n"
     "20\tx.f90\t/^20  end file 7$/;\"\tl\tline:9\tmodule:m\tfile:\n"
     "30\tx.f90\t/^30  continue$/;\"\tl\tline:10\tfile:\n"
     "m\tx.f90\t/^module m$/;\"\tm\tline:1\n"
     "s\tx.f90\t/^  subroutine s$/;\"\ts\tline:3\tmodule:m\n"
     "t\tx.f90\t/^  subroutine t$/;\"\ts\tline:12\n"},
    {SHARED, "extras without fileScope", "f.f90", "--extras=-F --kinds-Fortran=+L",
     "program main\n"
     "  integer :: i\n"
     "10 continue\n"
     "end program\n"
     "subroutine s(n)\n"
     "  integer :: n, k\n"
     "end subroutine\n",
     "i\tf.f90\t/^  integer :: i$/;\"\tv\tline:2\tprogram:main\n"
     "main\tf.f90\t/^program main$/;\"\tp\tline:1\n"
     "s\tf.f90\t/^subroutine s(/;\"\ts\tline:5\n",
     ""},
    {SHARED, "forced Fortran", "f.c", "--language-force=Fortran",
     "      program main\n"
     "      integer i\n"
     "      end\n",
     "i\tf.c\t/^      integer i$/;\"\tv\tline:2\tprogram:main\n"
     "main\tf.c\t/^      program main$/;\"\tp\tline:1\n",
     ""},
    {SHARED, "a C extension mapped to Fortran", "f.c", "--langmap=fortran:+.c",
     "      program main\n"
     "      end\n",
     "main\tf.c\t/^      program main$/;\"\tp\tline:1\n",
     ""},
    {SHARED, "a tie with C++ decided by the template", "f.f90.in",
     "--map-Fortran=+.in --map-C++=+.in",
     "program main\n"
     "end program\n",
     "main\tf.f90.in\t/^program main$/;\"\tp\tline:1\n",
     ""},
};

// Subroutine i of the nested ones in deepcase, as a tag line.
static Str deepline(Arena *a, i32 i)
{
    Buf b(a, 96);
    print(&b, "s");
    print(&b, (i64)i);
    print(&b, "\tx.f90\t/^subroutine s");
    print(&b, (i64)i);
    print(&b, "$/;\"\ts\tline:");
    print(&b, (i64)(2*i + 1));
    if (i) {
        print(&b, "\tsubroutine:s");
        print(&b, (i64)(i - 1));
    }
    return finish(&b);
}

// Sorted tag lines for the first n nested subroutines.
static Str deeptags(Arena *a, i32 n)
{
    Slice<Str> lines = {};
    for (i32 i = 0; i < n; i++) {
        lines = push(a, lines, deepline(a, i));
    }
    sort(lines.data, lines.len, strless, *a);
    Buf b(a, 1<<16);
    for (iz i = 0; i < lines.len; i++) {
        print(&b, lines[i]);
        putbyte(&b, '\n');
    }
    return finish(&b);
}

// R1: subroutines nested deeper than the limit, which is too much to
// write out. Universal Ctags follows them all, until its stack runs out
// at about 20000 levels.
static TagCase deepcase(Arena *a, i32 depth)
{
    Buf src(a, 1<<16);
    for (i32 i = 0; i < depth; i++) {
        print(&src, "subroutine s");
        print(&src, (i64)i);
        print(&src, "\ncontains\n");
    }
    for (i32 i = 0; i < depth; i++) {
        print(&src, "end subroutine\n");
    }
    TagCase r = {};
    r.kind   = IMPROVED;
    r.name   = "R1 nesting stops at FTN_DEPTHMAX";
    r.file   = "x.f90";
    r.args   = "";
    r.src    = finish(&src);
    r.want   = deeptags(a, FTN_DEPTHMAX);
    r.uctags = deeptags(a, depth);
    return r;
}

static void test_fortran(Test *t)
{
    runcases(t, fortrancases, countof(fortrancases));

    Arena   save = t->perm;
    TagCase deep = deepcase(&t->perm, FTN_DEPTHMAX+6);
    runcases(t, &deep, 1);
    t->perm = save;
}
