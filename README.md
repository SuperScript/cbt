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
cbt [-0] [-m file]
```

The `cbt` program keeps one crit-bit tree in memory for the life of the process. It reads requests from standard input, writes results to standard output, and writes errors to standard error. It writes each request's output before reading the next request, so another program can keep it running and exchange one request and response at a time. The tree starts empty, or as the dump named by `-m`, and is discarded at exit; `save`, `write`, and `dump` keep a copy.

### Options

- `-0`: End every request, response, and snapshot record with NUL instead of LF. Keys may then contain LF.
- `-m file`: Map the file, written by `dump`, read-only with `mmap` and answer requests from its bytes in place, without building a tree. See [Read-only mode](#read-only-mode).

The options combine in either order. A repeated or unknown option, `-m` without a file, or any other argument fails with `cbt: fatal: unsupported option`.

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
| `complete <prefix>` | The bytes after the prefix of each longer matching key in byte order, then one empty record |
| `load <path>` | None |
| `save <path>` | None |
| `dump <file>` | None |
| `read` | None; the snapshot records follow the request on standard input |
| `write` | The snapshot: every key in byte order, then one empty record |
| `exit` | None |

- `has`: Reports membership; the tree is unchanged.
- `add`: Inserts a copy of the key. On allocation failure, `add` prints `0`, reports `cbt: fatal: add: out of memory`, and stops; the tree is unchanged.
- `rm`: Deletes the key.
- `clear`: Removes every key.
- `after`: Finds the first key in byte order that starts with the prefix and prints the bytes after the prefix. A key equal to the prefix gives an empty suffix, printed as an empty record. A prefix no key starts with prints no record, so a caller that needs to tell these cases apart uses `has` or `list`.
- `list`: Prints the full keys that start with the prefix. The empty prefix matches every key. Because the empty record ends the list, `list` fails before printing when its results include the empty key.
- `complete`: Prints the bytes after the prefix of each key that starts with the prefix, in byte order, skipping a key equal to the prefix. The empty prefix matches every nonempty key. A caller tells whether the prefix is itself a key with `has`.
- `load`: Reads the snapshot file at the path. See [Snapshots](#snapshots).
- `save`: Writes the snapshot to the named file.
- `dump`: Writes the tree to the named file in the binary dump layout. See [Dumps](#dumps).
- `read`: Reads a snapshot from standard input up to its empty record, then resumes reading requests.
- `write`: Writes the snapshot to standard output.
- `exit`: Stops `cbt` with status 0 without reading further requests. Output for earlier requests is already written.

An unknown command fails with `cbt: fatal: unknown command`. A command that takes an operand, sent with none, is a fatal error: `cbt: fatal: <command>: missing operand`. A command that takes no operand, sent with one, even an empty one, is a fatal error: `cbt: fatal: <command>: unexpected operand`.

### Read-only mode

With `-m file`, `cbt` searches the mapped dump instead of an in-memory tree. The commands `has`, `after`, `list`, `complete`, `save`, `write`, `dump`, and `exit` work as described above; `dump` writes the mapped bytes unchanged. The commands that change the tree, `add`, `rm`, `clear`, `load`, and `read`, fail with `cbt: fatal: <command>: read-only tree`.

At startup, `cbt` checks that the file is a regular file of at least 32 bytes whose header holds the magic, version 1, the file's own length, and a root reference to an 8-aligned record inside the file. Each search then checks every record it reaches: a child lies after the header and ends before its parent, and a key's NUL lies before its parent. A search that meets a bad record fails with `cbt: fatal: <command>: malformed dump`, after any output already written for that request.

The file must not change in place while `cbt` has it mapped. The `dump` command replaces a file by unlinking it, so dumping to the mapped path leaves the mapped bytes intact.

### Snapshots

A snapshot is raw keys, each ending with the selected delimiter, followed by one empty record. The `save` and `write` commands emit the keys sorted by byte order, the same format as the output from `list`. The `load` and `read` commands accept keys in any order and add a repeated key once. The empty record is the end marker, so an empty tree's snapshot is the marker alone, and a tree holding the empty key cannot be stored. In `-0` mode, LF may occur inside a key. Backslashes and escape text are literal bytes.

The `load` and `read` commands add the snapshot's keys to the current tree. Keys already present, including keys the snapshot lacks, stay in place. A snapshot that fails to load, because it is malformed or memory runs out, leaves the tree as it was. The `load` command rejects a file that is empty, lacks the end marker, or holds bytes after it. A nonempty snapshot written with the other delimiter is rejected.

The `save` and `write` commands fail before opening the file or writing output when the tree holds the empty key. The `save` command writes the named file in place: it truncates the file, then writes the snapshot, and a save through a symlink writes the symlink's target. A failed save can leave the file empty or partial. Changes in memory are durable only after a `save` succeeds.

### Dumps

A dump holds the tree's nodes and keys in a fixed binary layout that a later process can map with `mmap` and search in place, without rebuilding the tree. The layout is the same in both delimiter modes, and the empty key can be dumped. Every integer is little-endian on every host.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 8 | The ASCII bytes `critbit0` |
| 8 | 8 | Version, `1` |
| 16 | 8 | Total file length in bytes |
| 24 | 8 | Root reference; `0` for the empty tree |

Records follow the 32-byte header. A reference is a byte offset from the start of the file. A reference with low bit 1 names an internal node at the offset minus 1; one with low bit 0 names a key. An internal node is 24 bytes: the uint64 reference of child 0, the uint64 reference of child 1, the uint32 critical byte index, the uint8 `otherbits` mask (every bit set except the critical bit), and three zero bytes. A key record is the key's bytes and a NUL, padded with zero bytes to a multiple of 8, so every record starts on an 8-byte boundary. Records are in postorder: the child 0 subtree, the child 1 subtree, then the node. Each child sits before its parent, the root record is last, and the same set of keys gives the same bytes.

The `dump` command removes any existing file at the path, then creates it with the permissions `save` gives a new file, 0666 less the umask. A process that already has the old file open or mapped keeps the old bytes. A dump through a symlink replaces the symlink. A failed dump can leave a partial file. Success prints nothing.

For example, after `add b` and `add a`, `dump` writes 72 bytes: the header with length 72 and root reference 49, the key `a` at offset 32, the key `b` at offset 40, and at offset 48 the node with children 32 and 40, byte 0, and `otherbits` `0xfd`.

### Errors and exit status

On the first error, `cbt` writes one message to standard error and exits with status 1. Output already written for earlier requests stays written, and later requests are not read. At the end of input or on `exit`, with no error, `cbt` exits with status 0.

| Message | Cause |
| --- | --- |
| `cbt: fatal: unsupported option` | A repeated or unknown option, `-m` without a file, or any other argument |
| `cbt: fatal: -m: cannot open file` | The `-m` file cannot be opened or is not a regular file |
| `cbt: fatal: -m: cannot map file` | The `-m` file cannot be examined with `fstat`, is too large to map, or `mmap` fails |
| `cbt: fatal: -m: malformed dump` | The `-m` file is shorter than 32 bytes or its header is not a version 1 dump header matching the file |
| `cbt: fatal: unterminated request` | Input ends without a final delimiter |
| `cbt: fatal: NUL in request` | A NUL byte inside a default-mode request |
| `cbt: fatal: unknown command` | A request whose command name is not a command |
| `cbt: fatal: has: missing operand` | `has` with no operand |
| `cbt: fatal: add: missing operand` | `add` with no operand |
| `cbt: fatal: rm: missing operand` | `rm` with no operand |
| `cbt: fatal: after: missing operand` | `after` with no operand |
| `cbt: fatal: list: missing operand` | `list` with no operand |
| `cbt: fatal: complete: missing operand` | `complete` with no operand |
| `cbt: fatal: load: missing operand` | `load` with no operand |
| `cbt: fatal: save: missing operand` | `save` with no operand |
| `cbt: fatal: dump: missing operand` | `dump` with no operand |
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
| `cbt: fatal: complete: out of memory` | `complete` ran out of memory |
| `cbt: fatal: complete: output failed` | `complete` cannot write its extensions to standard output |
| `cbt: fatal: write: empty key cannot be stored` | `write` while the tree holds the empty key |
| `cbt: fatal: write: out of memory` | `write` ran out of memory |
| `cbt: fatal: write: output failed` | `write` cannot write the snapshot to standard output |
| `cbt: fatal: save: empty key cannot be stored` | `save` while the tree holds the empty key |
| `cbt: fatal: save: cannot open file` | `save` cannot open or create the file |
| `cbt: fatal: save: out of memory` | `save` ran out of memory |
| `cbt: fatal: save: write error` | `save` cannot write or close the file |
| `cbt: fatal: dump: cannot open file` | `dump` cannot create the file, as when its directory is missing or the path names a directory |
| `cbt: fatal: dump: write error` | `dump` cannot write, flush, sync, or close the file |
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
| `cbt: fatal: add: read-only tree` | `add` under `-m` |
| `cbt: fatal: rm: read-only tree` | `rm` under `-m` |
| `cbt: fatal: clear: read-only tree` | `clear` under `-m` |
| `cbt: fatal: load: read-only tree` | `load` under `-m` |
| `cbt: fatal: read: read-only tree` | `read` under `-m` |
| `cbt: fatal: has: malformed dump` | `has` meets a bad record in the `-m` file |
| `cbt: fatal: after: malformed dump` | `after` meets a bad record in the `-m` file |
| `cbt: fatal: list: malformed dump` | `list` meets a bad record in the `-m` file |
| `cbt: fatal: complete: malformed dump` | `complete` meets a bad record in the `-m` file |
| `cbt: fatal: save: malformed dump` | `save` meets a bad record in the `-m` file |
| `cbt: fatal: write: malformed dump` | `write` meets a bad record in the `-m` file |

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

Dump a tree, then answer requests from the dump in read-only mode:

```sh
printf 'add ca\nadd cb\nadd a\ndump tests/prefix/m.dump\n' | tests/prefix/bin/cbt
printf 'has ca\nlist c\nadd x\n' | tests/prefix/bin/cbt -m tests/prefix/m.dump
```

The first command prints `2`, `2`, `2`. The second prints `1`, `ca`, `cb`, and an empty record, then writes `cbt: fatal: add: read-only tree` to standard error and exits with status 1.

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
| `critbit0_dump` | Passes the tree's [dump](#dumps) image, in order, to a callback; `1` after the whole image, `0` if the callback stops |
| `critbit0_image_init` | Sets up a `critbit0_image` over dump bytes the caller holds; `1` when the header is valid for the given length, `0` otherwise |
| `critbit0_image_contains` | As `critbit0_contains` on the image; `-1` on a malformed record |
| `critbit0_image_firstprefixed` | As `critbit0_firstprefixed` on the image; `-1` on a malformed record |
| `critbit0_image_allprefixed` | As `critbit0_allprefixed` on the image; `-2` on a malformed record, after any callbacks for earlier matches |

The `allprefixed` callback takes no arguments. It can read the suffix through caller-managed state and returns `1` to continue or `0` to stop. A null callback stops after the first match. The result buffer belongs to the caller after either prefix operation; see `critbit0.h` for its state on failure.

The `critbit0_dump` callback takes a pointer to bytes and their count as an `int64`, and returns `1` to continue or `0` to stop. The function allocates nothing and needs no pool.

The `critbit0_image_*` functions search a dump image in place and do no I/O: the caller reads or maps the bytes, which must stay unchanged while in use, and passes them with their length to `critbit0_image_init`. The init call checks only the header and root reference; each lookup checks every record it reaches, so a lookup on a damaged image ends with a malformed result instead of reading outside the bytes. Prefix results are allocated from the pool as for the tree functions.

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

The baseline implementation comes from Daniel J. Bernstein's qhasm archive, version 20070207: https://cr.yp.to/qhasm/qhasm-20070207.tar.gz. The repository root holds the archive's six `critbit0_*.c` members unchanged; `critbit0_dump.c` is project code. The imported `critbit0.h`, `str0.h`, `limitmalloc.h`, `str0_*.c`, and `limitmalloc_*.c` members provide their build dependencies. The `int64.h`, `uint8.h`, `uint32.h`, and `uint64.h` files come from the archive's corresponding `:8-stdint` members.
