// Fortran: tag cases

static TagCase fortrancases[] = {
    {SHARED, "program", "x.f90", "",
     "program hello\n"
     "end program hello\n",
     "hello\tx.f90\t/^program hello$/;\"\tp\tline:1\n",
     ""},
};

static void test_fortran(Test *t)
{
    runcases(t, fortrancases, countof(fortrancases));
}
