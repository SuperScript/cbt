needvars :=
CC := cc
AR := ar
PREFIX := /usr/local
LIB_SOURCES := critbit0_contains.c critbit0_insert.c critbit0_delete.c critbit0_clear.c critbit0_firstprefixed.c critbit0_allprefixed.c critbit0_dump.c critbit0_image_init.c critbit0_image_contains.c critbit0_image_firstprefixed.c critbit0_image_allprefixed.c str0_length.c str0_equalc.c str0_copyc.c str0_free.c limitmalloc_open.c limitmalloc_open_if2.c limitmalloc_close.c
LIB_OBJECTS := $(LIB_SOURCES:.c=.o)
LIMITMALLOC_HEADERS := limitmalloc.h int64.h
STR0_HEADERS := str0.h uint64.h $(LIMITMALLOC_HEADERS)
CRITBIT0_HEADERS := critbit0.h uint32.h uint8.h $(STR0_HEADERS)
PUBLIC_HEADERS := critbit0.h str0.h limitmalloc.h uint32.h uint8.h uint64.h int64.h

#_# help
#_#   Display this helpful message
#_#
.PHONY: help
help:
	@seen=; for m in $(MAKEFILE_LIST) $(.MAKE.MAKEFILES); do \
	  case "$$m" in (/usr/share/mk/*) continue;; esac; \
	  case " $$seen " in (*" $$m "*) continue;; esac; \
	  echo "$$m:" && \
	  sed -n 's/^#_#/ /p' "$$m"; \
	  seen="$$seen $$m"; \
	done

#_# needvar.X
#_#   Check whether $(X) is nonempty
#_#   Add X to needvars to enable
#_#
_NEEDVAR_TARGETS != for v in $(needvars); do printf 'needvar.%s ' "$$v"; done
_NEEDVAR_LOOKUP != for v in $(needvars); do printf '%s) val='"'"'$$(%s)'"'"';; ' "$$v" "$$v"; done

$(_NEEDVAR_TARGETS):
	@v=$@; v=$${v#needvar.}; val=; \
	  case "$$v" in $(_NEEDVAR_LOOKUP) esac; \
	  test -n "$$val" || { echo "variable not defined: $$v" >&2; exit 1; }

#_# clean
#_#   Remove build and test artifacts
#_#
.PHONY: clean
clean:
	@rm -f $(LIB_OBJECTS) libcritbit0.a cbt tests/output
	@rm -rf tests/basedir tests/prefix

#_# test
#_#   Run test suite and diff with expected results
#_#
.PHONY: test
test: cbt
	@$(MAKE) install PREFIX=tests/prefix
	@rm -f tests/output
	@CC="$(CC)" MAKE="$(MAKE)" tests/run > tests/output
	@diff tests/output tests/expected

#_# install
#_#   Install the static library, public headers, and cbt under PREFIX
#_#
.PHONY: install
install: libcritbit0.a cbt
	@mkdir -p "$(PREFIX)/lib" "$(PREFIX)/include" "$(PREFIX)/bin"
	@install -m 755 cbt "$(PREFIX)/bin/cbt"
	@install -m 644 libcritbit0.a "$(PREFIX)/lib/libcritbit0.a"
	@for header in $(PUBLIC_HEADERS); do install -m 644 "$$header" "$(PREFIX)/include/$$header" || exit 1; done

libcritbit0.a: $(LIB_OBJECTS)
	@temp="$$(mktemp -d "$@.XXXXXX")" || exit 1; \
	  $(AR) rcs "$$temp/libcritbit0.a" $(LIB_OBJECTS) && \
	  mv "$$temp/libcritbit0.a" "$@"; \
	  status=$$?; rm -rf "$$temp"; exit "$$status"

cbt: cbt.c libcritbit0.a $(CRITBIT0_HEADERS)
	@temp="$$(mktemp "$@.XXXXXX")" || exit 1; \
	  $(CC) -I. -o "$$temp" cbt.c libcritbit0.a && \
	  mv "$$temp" "$@"; \
	  status=$$?; rm -f "$$temp"; exit "$$status"

critbit0_contains.o critbit0_insert.o critbit0_delete.o critbit0_clear.o critbit0_firstprefixed.o critbit0_allprefixed.o critbit0_dump.o critbit0_image_init.o critbit0_image_contains.o critbit0_image_firstprefixed.o critbit0_image_allprefixed.o: $(CRITBIT0_HEADERS)
str0_length.o str0_equalc.o str0_copyc.o str0_free.o: $(STR0_HEADERS)
limitmalloc_open.o limitmalloc_open_if2.o limitmalloc_close.o: $(LIMITMALLOC_HEADERS)

.SUFFIXES: .c .o
.c.o:
	@$(CC) -I. -c -o "$@" "$<"
