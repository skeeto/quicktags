C     Fixed-form Fortran 77 for the plugin tests. Public domain.
      BLOCK DATA INIT
      COMMON /STATE/ N, TOTAL
      INTEGER N
      REAL TOTAL
      DATA N, TOTAL /0, 0.0/
      END

      SUBROUTINE ACCUM(X)
      COMMON /STATE/ N, TOTAL
      INTEGER N
      REAL TOTAL, X
      N = N + 1
      TOTAL = TOTAL + X
      RETURN
      ENTRY RESET
      N = 0
      TOTAL = 0.0
   10 CONTINUE
      END

      REAL FUNCTION MEAN()
      COMMON /STATE/ N, TOTAL
      INTEGER N
      REAL TOTAL
      MEAN = TOTAL / N
      END
