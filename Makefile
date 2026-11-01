needvars =

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
	@tests/run > tests/output
	@diff tests/output tests/expected
