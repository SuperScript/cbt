# Crit Bit Trees

## Why

The project makes crit-bit trees available as a simple, flexible structure for many applications.

## Payoff

It offers an alternative to CDB that can perform better when reads are few relative to writes.

## Users and use

Anyone who needs crit-bit trees can use the library from a program or run its commands.

## Installation

Run `make install PREFIX=/path/to/prefix` to install `libcritbit0.a` in `PREFIX/lib`, the public headers in `PREFIX/include`, and `cbt` in `PREFIX/bin`.

## Command-line program

### Synopsis

```text
cbt [-0]
```

The `cbt` program keeps one crit-bit tree in memory for the life of the process. It reads requests from standard input, writes results to standard output, and writes errors to standard error. It writes each request's output before reading the next request, so another program can keep it running and exchange one request and response at a time. The tree starts empty and is discarded at exit; `save` and `write` keep a copy.

### Options

- `-0`: End every request, response, and snapshot record with NUL instead of LF. Keys may then contain LF.

Any other argument fails with `cbt: fatal: unsupported option`.

### Records

Every request and every output record ends with the selected delimiter: LF by default, NUL with `-0`. A command that takes an operand is sent as `<command> <operand>`: the command name, one space, then the operand up to the delimiter. The first space ends the command name; later spaces belong to the operand, so `add  a` adds the key ` a`. A request with no space has no operand. A space followed directly by the delimiter gives the empty operand, which names the empty key. The commands `clear`, `read`, `write`, and `exit` take no operand and are sent as the command name alone.

Keys, prefixes, and paths are raw bytes. Backslashes, tabs, spaces, and control bytes have no special meaning. NUL cannot occur in a key. In default mode, LF ends a record and cannot occur inside a key, and a NUL byte inside a request is a fatal error: `cbt: fatal: NUL in request`. A final request without its delimiter is a fatal error: `cbt: fatal: unterminated request`.

### Commands

| Request | Output |
| --- | --- |
| `has <key>` | `1` if the key is present, `0` if absent |
| `add <key>` | `2` if added, `1` if already present, `0` if out of memory, a fatal error |
| `rm <key>` | `1` if deleted, `0` if absent |
| `clear` | None |
| `after <prefix>` | The suffix of the first matching key, or nothing when no key matches |
| `list <prefix>` | Each matching full key in byte order, then one empty record |
| `load <path>` | None |
| `save <path>` | None |
| `read` | None; the snapshot records follow the request on standard input |
| `write` | The snapshot: every key in byte order, then one empty record |
| `exit` | None |

- `has`: Reports membership; the tree is unchanged.
- `add`: Inserts a copy of the key. On allocation failure, `add` prints `0`, reports `cbt: fatal: add: out of memory`, and stops; the tree is unchanged.
- `rm`: Deletes the key.
- `clear`: Removes every key.
- `after`: Finds the first key in byte order that starts with the prefix and prints the bytes after the prefix. A key equal to the prefix gives an empty suffix, printed as an empty record. A prefix no key starts with prints no record, so a caller that needs to tell these cases apart uses `has` or `list`.
- `list`: Prints the full keys that start with the prefix. The empty prefix matches every key. Because the empty record ends the list, `list` fails before printing when its results include the empty key.
- `load`: Reads the snapshot file at the path. See [Snapshots](#snapshots).
- `save`: Writes the snapshot to the named file.
- `read`: Reads a snapshot from standard input up to its empty record, then resumes reading requests.
- `write`: Writes the snapshot to standard output.
- `exit`: Stops `cbt` with status 0 without reading further requests. Output for earlier requests is already written.

An unknown command fails with `cbt: fatal: unknown command`. A command that takes an operand, sent with none, is a fatal error: `cbt: fatal: <command>: missing operand`. A command that takes no operand, sent with one, even an empty one, is a fatal error: `cbt: fatal: <command>: unexpected operand`.

### Snapshots

A snapshot is raw keys, each ending with the selected delimiter, followed by one empty record. The `save` and `write` commands emit the keys sorted by byte order, the same format as the output from `list`. The `load` and `read` commands accept keys in any order and add a repeated key once. The empty record is the end marker, so an empty tree's snapshot is the marker alone, and a tree holding the empty key cannot be stored. In `-0` mode, LF may occur inside a key. Backslashes and escape text are literal bytes.

The `load` and `read` commands add the snapshot's keys to the current tree. Keys already present, including keys the snapshot lacks, stay in place. A snapshot that fails to load, because it is malformed or memory runs out, leaves the tree as it was. The `load` command rejects a file that is empty, lacks the end marker, or holds bytes after it. A nonempty snapshot written with the other delimiter is rejected.

The `save` and `write` commands fail before opening the file or writing output when the tree holds the empty key. The `save` command writes the named file in place: it truncates the file, then writes the snapshot, and a save through a symlink writes the symlink's target. A failed save can leave the file empty or partial. Changes in memory are durable only after a `save` succeeds.

### Errors and exit status

On the first error, `cbt` writes one message to standard error and exits with status 1. Output already written for earlier requests stays written, and later requests are not read. At the end of input or on `exit`, with no error, `cbt` exits with status 0.

| Message | Cause |
| --- | --- |
| `cbt: fatal: unsupported option` | An argument other than a single `-0` |
| `cbt: fatal: unterminated request` | Input ends without a final delimiter |
| `cbt: fatal: NUL in request` | A NUL byte inside a default-mode request |
| `cbt: fatal: unknown command` | A request whose command name is not a command |
| `cbt: fatal: has: missing operand` | `has` with no operand |
| `cbt: fatal: add: missing operand` | `add` with no operand |
| `cbt: fatal: rm: missing operand` | `rm` with no operand |
| `cbt: fatal: after: missing operand` | `after` with no operand |
| `cbt: fatal: list: missing operand` | `list` with no operand |
| `cbt: fatal: load: missing operand` | `load` with no operand |
| `cbt: fatal: save: missing operand` | `save` with no operand |
| `cbt: fatal: clear: unexpected operand` | `clear` with an operand, even an empty one |
| `cbt: fatal: read: unexpected operand` | `read` with an operand, even an empty one |
| `cbt: fatal: write: unexpected operand` | `write` with an operand, even an empty one |
| `cbt: fatal: exit: unexpected operand` | `exit` with an operand, even an empty one |
| `cbt: fatal: input failed` | Standard input cannot be read |
| `cbt: fatal: output failed` | A response cannot be written to standard output, including when its reader has closed it |
| `cbt: fatal: add: out of memory` | `add` ran out of memory |
| `cbt: fatal: after: out of memory` | `after` ran out of memory |
| `cbt: fatal: list: out of memory` | `list` ran out of memory |
| `cbt: fatal: list: empty key cannot be listed` | `list` would print the empty key |
| `cbt: fatal: list: output failed` | `list` cannot write its keys to standard output |
| `cbt: fatal: write: empty key cannot be stored` | `write` while the tree holds the empty key |
| `cbt: fatal: write: out of memory` | `write` ran out of memory |
| `cbt: fatal: write: output failed` | `write` cannot write the snapshot to standard output |
| `cbt: fatal: save: empty key cannot be stored` | `save` while the tree holds the empty key |
| `cbt: fatal: save: cannot open file` | `save` cannot open or create the file |
| `cbt: fatal: save: out of memory` | `save` ran out of memory |
| `cbt: fatal: save: write error` | `save` cannot write or close the file |
| `cbt: fatal: load: cannot open file` | `load` cannot open the file |
| `cbt: fatal: load: read error` | `load` cannot read the file |
| `cbt: fatal: load: malformed record` | The file holds a NUL inside a default-mode record or ends with a record lacking its delimiter, as a snapshot in the other delimiter does |
| `cbt: fatal: load: missing marker` | The file ends before the empty record, including an empty file |
| `cbt: fatal: load: data after marker` | The file holds bytes after the empty record |
| `cbt: fatal: load: out of memory` | `load` ran out of memory |
| `cbt: fatal: read: read error` | `read` cannot read standard input |
| `cbt: fatal: read: malformed record` | The stream snapshot holds a NUL inside a default-mode record or ends with a record lacking its delimiter |
| `cbt: fatal: read: missing marker` | Standard input ends before the snapshot's empty record |
| `cbt: fatal: read: out of memory` | `read` ran out of memory |

### Examples

These examples run the installed program from `tests/prefix/bin/cbt`, after `make install PREFIX=tests/prefix`.

Add two keys, then query them:

```sh
printf 'add ca\nadd cb\nhas ca\nhas x\nafter c\nlist c\n' | tests/prefix/bin/cbt
```

The output is `2`, `2`, `1`, `0`, `a`, `ca`, `cb`, and an empty record, each ending with LF.

Load keys from a stream, add one, and write the result:

```sh
{ printf 'read\n'; printf 'b\na\n\n'; printf 'add c\nwrite\n'; } | tests/prefix/bin/cbt
```

The output is `2`, then `a`, `b`, `c`, and an empty record.

Save a tree to a file, clear it, and load it back:

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

Send a key containing a literal newline in `-0` mode and print the NUL-framed response as hexadecimal bytes:

```sh
printf 'add A\nB\000after A\000' | tests/prefix/bin/cbt -0 | od -An -tx1
```

The output is `32 00 0a 42 00`: `2`, then the suffix LF `B`.

A failure stops the stream and sets the exit status:

```sh
printf 'add a\nbogus\nhas a\n' | tests/prefix/bin/cbt; echo "status $?"
```

Standard output holds `2` and `status 1`; standard error holds `cbt: fatal: unknown command`.

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

## Imported source

The baseline implementation comes from Daniel J. Bernstein's qhasm archive, version 20070207: https://cr.yp.to/qhasm/qhasm-20070207.tar.gz. The repository root holds the archive's six `critbit0_*.c` members unchanged. The imported `critbit0.h`, `str0.h`, `limitmalloc.h`, `str0_*.c`, and `limitmalloc_*.c` members provide their build dependencies. The `int64.h`, `uint8.h`, `uint32.h`, and `uint64.h` files come from the archive's corresponding `:8-stdint` members.
