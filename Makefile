PROG            = unit_tests
OBJ             = svs_cpu.o \
                  svs_arith.o \
                  svs_trace.o \
                  svs_util.o \
                  svs_mmu.o
CINYTEST        = cinytest/ciny.o \
                  cinytest/ciny_posix.o
CFLAGS		= -std=c11 -g -O -Wall -Werror
LDFLAGS         = -g

all:		$(PROG)

test:           unit_tests
		./unit_tests

clean:
		rm -f $(PROG) *.o *.a cinytest/*.o

unit_tests:     unit_tests.o libsvs.a libtest.a
		$(CC) $(LDFLAGS) unit_tests.o libsvs.a libtest.a -o $@

libsvs.a:       $(OBJ)
		$(AR) rc $@ $(OBJ)

libtest.a:      $(CINYTEST)
		$(AR) rc $@ $(CINYTEST)
