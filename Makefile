needvars :=
CC := cc
AR := ar
PREFIX := /usr/local
LIB_SOURCES := critbit0_contains.c critbit0_insert.c critbit0_delete.c critbit0_clear.c critbit0_firstprefixed.c critbit0_allprefixed.c str0_length.c str0_equalc.c str0_copyc.c str0_free.c limitmalloc_open.c limitmalloc_open_if2.c limitmalloc_close.c
LIB_OBJECTS := $(LIB_SOURCES:.c=.o)
PUBLIC_HEADERS := critbit0.h str0.h limitmalloc.h uint32.h uint8.h uint64.h int64.h

include mk/help.mk

#_# clean
#_#   Remove test artifacts
#_#
.PHONY: clean
clean:
	@rm -f tests/output
	@rm -rf tests/basedir/"*"
	@rmdir tests/basedir 2>/dev/null; true

#_# test
#_#   Run test suite and diff with expected results
#_#
.PHONY: test
test:
	@rm -f tests/output
	@CC="$(CC)" tests/run > tests/output
	@diff tests/output tests/expected

#_# install
#_#   Install the static library and public headers under PREFIX
#_#
.PHONY: install
install: libcritbit0.a
	@mkdir -p "$(PREFIX)/lib" "$(PREFIX)/include"
	@cp libcritbit0.a "$(PREFIX)/lib/libcritbit0.a"
	@for header in $(PUBLIC_HEADERS); do cp "$$header" "$(PREFIX)/include/$$header" || exit 1; done

libcritbit0.a: $(LIB_OBJECTS)
	@temp="$$(mktemp -d "$@.XXXXXX")" && \
	  $(AR) rcs "$$temp/libcritbit0.a" $(LIB_OBJECTS) && \
	  mv "$$temp/libcritbit0.a" "$@" && \
	  rmdir "$$temp"

.SUFFIXES: .c .o
.c.o:
	@$(CC) -I. -c -o "$@" "$<"
