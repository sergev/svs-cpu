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

###
svs_arith.o: svs_arith.c el_svs_api.h el_svs_internal.h
svs_cpu.o: svs_cpu.c el_master_api.h el_svs_api.h el_svs_internal.h
svs_mmu.o: svs_mmu.c el_master_api.h el_svs_api.h el_svs_internal.h
svs_trace.o: svs_trace.c el_svs_internal.h
svs_util.o: svs_util.c el_master_api.h el_svs_api.h el_svs_internal.h
unit_tests.o: unit_tests.c cinytest/ciny.h el_master_api.h el_svs_api.h
