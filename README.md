# Crit Bit Trees

## Why

The project makes crit-bit trees available as a simple, flexible structure for many applications.

## Payoff

It offers an alternative to CDB that can perform better when reads are few relative to writes.

## Users and use

Anyone who needs crit-bit trees can use the library from a program or run its commands.

## Installation

Run `make install PREFIX=/path/to/prefix` to install `libcritbit0.a` in `PREFIX/lib`, the public headers in `PREFIX/include`, and `cbt` in `PREFIX/bin`.

## C library

Include `<critbit0.h>` and link with `libcritbit0.a`. Keys are NUL-terminated byte strings (`str0`). Initialize a `critbit0_tree` to zero and set the `limitmalloc_pool` allocation limit (the example uses 1024 bytes). The tree copies inserted keys and owns those copies and its nodes. Use the same pool for insertion, deletion, prefix lookups, and clearing. Free each caller-owned suffix with `str0_free` using that pool. Clear the tree before discarding it.

The operations and their results are:

| Operation | Result |
| --- | --- |
| `critbit0_contains` | `1` if present, `0` if absent |
| `critbit0_insert` | `2` if inserted, `1` if already present, `0` on allocation failure; failure leaves the tree unchanged and sets `errno` |
| `critbit0_delete` | `1` if deleted, `0` if absent |
| `critbit0_clear` | Releases all keys and nodes |
| `critbit0_firstprefixed` | `1` and one suffix after the prefix, or `0` if absent or allocation fails; `errno` distinguishes allocation failure |
| `critbit0_allprefixed` | Visits suffixes in tree order; `1` after all matches, `0` if the callback stops, `-1` on suffix allocation failure |

The `allprefixed` callback takes no arguments. It can read the suffix through caller-managed state and returns `1` to continue or `0` to stop. A null callback stops after the first match. The result buffer belongs to the caller after either prefix operation; see `critbit0.h` for its state on failure.

Save this example as `example.c` in the repository root:

```c
#include <stdio.h>
#include <critbit0.h>

static str0 suffix;

static int show_suffix(void)
{
  printf("match: %s\n", suffix);
  return 1;
}

int main(void)
{
  critbit0_tree tree = { 0 };
  limitmalloc_pool pool = { 1024, 0, 0 };
  str0 ca = "ca", cb = "cb", prefix = "c", result = 0;

  if (critbit0_insert(&tree, &pool, &ca) != 2 ||
      critbit0_insert(&tree, &pool, &cb) != 2) return 1;
  printf("contains ca: %d\n", critbit0_contains(&tree, &ca));
  if (critbit0_firstprefixed(&tree, &pool, &result, &prefix) != 1) return 1;
  printf("first suffix: %s\n", result);
  str0_free(&result, &pool);
  if (critbit0_allprefixed(&tree, &pool, &suffix, &prefix, show_suffix) != 1)
    return 1;
  str0_free(&suffix, &pool);
  printf("deleted ca: %d\n", critbit0_delete(&tree, &pool, &ca));
  critbit0_clear(&tree, &pool);
  return 0;
}
```

After `make install PREFIX=tests/prefix`, compile and run it with the installed header and library:

```sh
cc -Itests/prefix/include -o example example.c tests/prefix/lib/libcritbit0.a
./example
```

The output is:

```text
contains ca: 1
first suffix: a
match: a
match: b
deleted ca: 1
```

## Command stream

The installed `tests/prefix/bin/cbt` reads requests from standard input and writes results to standard output. By default, each record ends with LF. The `-0` option uses NUL. A request is a command, a space, and its data. `clear`, `read`, and `write` have no data and no space. The first space separates the command from its data, and later spaces stay in the data. An empty operand is a space with nothing after it. One process keeps its tree across requests. Errors go to standard error and stop the process.

| Request | Response |
| --- | --- |
| `has <key>` | `1` if present, `0` if absent |
| `add <key>` | `2` if added, `1` if present, `0` on allocation failure |
| `rm <key>` | `1` if deleted, `0` if absent |
| `clear` | No output |
| `after <prefix>` | The suffix when a match exists; no output when it does not |
| `list <prefix>` | Zero or more bare full keys, then an empty record |
| `load <path>` | No output. Reads the `list` output format |
| `save <path>` | No output. Writes the `list` output format |
| `read` | No output. Reads the `list` output format |
| `write` | Sorted keys, then an empty record |

A `list` result containing the empty key fails before output. An empty `after` result is an empty record. An `after` allocation failure reports on standard error and exits nonzero. Operands and returned keys or suffixes carry raw bytes in both modes; backslashes, tabs, and control bytes have no special encoding. NUL cannot occur in a key. In the default mode, LF ends a record and cannot occur inside a key. The `-0` mode permits LF inside a key. A request without its selected ending delimiter fails. Snapshot files use the selected mode's delimiter as described below. For example:

```sh
printf 'add ca\nadd cb\nafter c\nlist c\n' | tests/prefix/bin/cbt
```

The response is `2`, `2`, `a`, `ca`, `cb`, and an empty record, each terminated by LF.

The following command sends a key containing a literal newline and prints the NUL-framed response as hexadecimal bytes:

```sh
printf 'add A\nB\000after A\000' | tests/prefix/bin/cbt -0 | od -An -tx1
```

## Snapshots

A snapshot has the same format as the output from `list`: raw keys sorted by byte order, followed by one empty record. `read`, `save`, and `load` use that format. In default mode, LF ends each key; in `-0` mode, NUL ends each key and LF may occur inside it. NUL cannot occur inside a key in either mode. Backslashes and old escape text are literal bytes. The empty record marks the end of the snapshot. An empty snapshot is that marker alone. A file load rejects a missing marker, bytes after the marker, or an empty file. Operand-free `read` and `write` use the same marker. `read` adds the snapshot's keys to the current tree after a valid marker, and later commands then run. `write` emits the sorted keys and one marker. `write` and `save` fail before emitting or replacing data when the tree holds the empty key. A nonempty snapshot using the other delimiter is rejected. Saving is explicit, so changes in memory are not durable until `save` succeeds. A successful `load` or `read` adds the snapshot's keys to the current tree and leaves keys already present in place. An unsuccessful one leaves the tree intact. Saving writes the snapshot to the named file.

This example uses a temporary snapshot path under `tests/prefix`:

```sh
printf 'add B\nadd A\nsave tests/prefix/example.snapshot\nclear\nload tests/prefix/example.snapshot\nhas A\n' | tests/prefix/bin/cbt
cat tests/prefix/example.snapshot
```

The output from `cbt` followed by `cat` is:

```text
2
2
1
A
B

```

## Imported source

The baseline implementation comes from Daniel J. Bernstein's qhasm archive, version 20070207: https://cr.yp.to/qhasm/qhasm-20070207.tar.gz. The repository root holds the archive's six `critbit0_*.c` members unchanged. The imported `critbit0.h`, `str0.h`, `limitmalloc.h`, `str0_*.c`, and `limitmalloc_*.c` members provide their build dependencies. The `int64.h`, `uint8.h`, `uint32.h`, and `uint64.h` files come from the archive's corresponding `:8-stdint` members.
