.PHONY: all clean debug install format release

CFLAGS_release += -O2 -Werror
CFLAGS_debug += -O0 -g

CFLAGS += -Wall -Wextra
CFLAGS += $(CFLAGS_$(MODE))

LDFLAGS += -lcurl -llua

PREFIX ?= usr

all: release

release debug:
	$(MAKE) MODE=$@ icky

clean:
	$(RM) -f *.o

install:
	install -Dt $(DESTDIR)/$(PREFIX)/bin icky

format:
	clang-format *.c *.h

analysis: clean
	scan-build --status-bugs -enable-checker alpha,nullability,security,unix $(MAKE)

icky: icky.o
