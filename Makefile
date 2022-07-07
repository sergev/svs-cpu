PROG            = ifcomp unit_tests
CFLAGS		= -g -O3 -Wall -Werror
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
		rm -f $(PROG) *.o *.input *.output

ifcomp:         main.o ifcomp.o
		$(CC) $(LDFLAGS) main.o ifcomp.o -o $@

unit_tests:     unit_tests.o ifcomp.o
		$(CC) $(LDFLAGS) unit_tests.o ifcomp.o $(LIBCMOCKA) -o $@
