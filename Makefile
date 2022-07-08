PROG            = libsvs.a unit_tests
OBJ             = svs_cpu.o \
                  svs_arith.o \
                  svs_trace.o \
                  svs_util.o \
                  svs_mmu.o
CFLAGS		= -std=c11 -g -O -Wall -Werror
LDFLAGS         = -g
LIBCMOCKA       = -lcmocka

ifneq ($(wildcard /usr/local/include),)
CFLAGS		+= -I/usr/local/include
endif
ifneq ($(wildcard /opt/homebrew/include),)
CFLAGS		+= -I/opt/homebrew/include
endif
ifneq ($(wildcard /usr/local/lib),)
LIBCMOCKA       += -L/usr/local/lib
endif
ifneq ($(wildcard /opt/homebrew/lib),)
LIBCMOCKA       += -L/opt/homebrew/lib
endif

all:		$(PROG)

test:           unit_tests
		./unit_tests

clean:
		rm -f $(PROG) *.o *.a *.input *.output

libsvs.a:       $(OBJ)
		$(AR) rc $@ $(OBJ)

unit_tests:     unit_tests.o libsvs.a
		$(CC) $(LDFLAGS) unit_tests.o libsvs.a $(LIBCMOCKA) -o $@
