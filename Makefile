CFLAGS+=-std=c99 -pedantic -Wall -Wextra -march=native -O3 -D_XOPEN_SOURCE -D_GNU_SOURCE -fopenmp
LDFLAGS+=-fopenmp
LDLIBS+=
BINS=parser

CFLAGS+=$(EXTRA_CFLAGS)
LDFLAGS+=$(EXTRA_LDFLAGS)
LDLIBS+=$(EXTRA_LDLIBS)

.PHONY: all
all: $(BINS)

.PHONY: clean
clean:
	$(RM) -- $(BINS)

.PHONY: distclean
distclean: clean
	$(RM) -- *.gcda

parser: parser.o io.o
