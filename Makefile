CFLAGS+=-std=c99 -pedantic -Wall -Wextra -march=native -O3 -D_XOPEN_SOURCE -D_GNU_SOURCE
LDFLAGS+=
LDLIBS+=-lm
BINS=parser

CFLAGS+=$(EXTRA_CFLAGS)
LDFLAGS+=$(EXTRA_LDFLAGS)
LDLIBS+=$(EXTRA_LDLIBS)

.PHONY: all
all: $(BINS)

.PHONY: clean
clean:
	$(RM) -- $(BINS) *.o

.PHONY: distclean
distclean: clean
	$(RM) -- *.gcda

parser: parser.o common.o io.o huffman.o coeffs.o imgproc.o frame.o
