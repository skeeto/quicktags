! A free-form Fortran module for the plugin tests. Public domain.
module solver
    implicit none
    private
    public :: vector, norm, step, solve

    integer, parameter :: dp = kind(1.0d0)
    real(dp), save :: tolerance = 1.0e-8_dp

    type :: vector
        real(dp) :: x, y
    contains
        procedure :: length => vector_length
    end type vector

    type, extends(vector) :: weighted
        real(dp) :: weight
    end type weighted

    enum, bind(c)
        enumerator :: east = 1, west
    end enum

    interface norm
        module procedure vector_length
    end interface norm

contains

    function vector_length(v) result(r)
        class(vector), intent(in) :: v
        real(dp) :: r
        r = sqrt(v%x**2 + v%y**2)
    end function vector_length

    subroutine step(v, h)
        type(vector), intent(inout) :: v
        real(dp), intent(in) :: h
        v%x = v%x + h
    end subroutine step

    subroutine solve(v, iterations)
        type(vector), intent(inout) :: v
        integer, intent(in) :: iterations
        integer :: i
        namelist /config/ i
        do i = 1, iterations
            call step(v, tolerance)
        end do
    end subroutine solve

end module solver

program driver
    use solver
    implicit none
    type(vector) :: v
    v = vector(1.0, 2.0)
    call solve(v, 10)
    print *, norm(v)
end program driver
