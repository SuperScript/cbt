# Crit Bit Trees

## Why

The project makes crit-bit trees available as a simple, flexible structure for many applications.

## Payoff

It offers an alternative to CDB that can perform better when reads are few relative to writes.

## Users and use

Anyone who needs crit-bit trees can use the library from a program or run its commands.

## Installation

Run `make install PREFIX=/path/to/prefix` to install `libcritbit0.a` in `PREFIX/lib`, the public headers in `PREFIX/include`, and `cbt` in `PREFIX/bin`.

## Daily use

Call the library from a program, or run `./cbt` and send tab-separated requests, one per line. For example, `printf 'insert\talpha\ncontains\talpha\n' | ./cbt` prints `inserted` and then `present`. The process retains its tree across requests. Send `load\t/path/to/snapshot` to replace the tree from a snapshot, or `save\t/path/to/snapshot` to write it. A snapshot has one escaped key per line; saving is explicit.

## Imported source

The baseline implementation comes from Daniel J. Bernstein's qhasm archive, version 20070207: https://cr.yp.to/qhasm/qhasm-20070207.tar.gz. The repository root holds the archive's six `critbit0_*.c` members unchanged. The imported `critbit0.h`, `str0.h`, `limitmalloc.h`, `str0_*.c`, and `limitmalloc_*.c` members provide their build dependencies. The `int64.h`, `uint8.h`, `uint32.h`, and `uint64.h` files come from the archive's corresponding `:8-stdint` members.
