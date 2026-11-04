#ifndef CRITBIT0_H
#define CRITBIT0_H

#include "str0.h"
#include "uint32.h"
#include "uint8.h"
#include "limitmalloc.h"

/* Keys are NUL-terminated byte strings. Initialize a tree to { 0 } before
   use. The tree owns copies of inserted keys and internal nodes; pass the
   same allocation pool to every allocating operation, deletion, and clear.
   Clear releases all tree storage. Never modify a key held by the tree. */

typedef struct {
  void *critbit0_private_child[2];
  uint32 critbit0_private_byte;
  uint8 critbit0_private_otherbits;
} critbit0_node;

typedef struct {
  void *critbit0_private_root;
} critbit0_tree;

extern int critbit0_contains(critbit0_tree *,str0 *);
/* Return 1 if the key is present, 0 otherwise. */

/* Return 2 for insertion, 1 for an existing key, 0 on allocation failure.
   Failure leaves the tree unchanged and sets errno (normally ENOMEM). */
extern int critbit0_insert(critbit0_tree *,limitmalloc_pool *,str0 *);
/* Return 1 if deleted, 0 if absent. The tree releases the deleted key. */
extern int critbit0_delete(critbit0_tree *,limitmalloc_pool *,str0 *);
/* Release every key and node; an empty tree may also be cleared. */
extern void critbit0_clear(critbit0_tree *,limitmalloc_pool *);
/* Store one matching key's suffix after prefix in *result. Return 1 when
   found, 0 when absent or when copying the suffix fails. On failure,
   *result is unchanged; errno distinguishes allocation failure (ENOMEM).
   The caller owns *result and frees it with str0_free using the same pool. */
extern int critbit0_firstprefixed(critbit0_tree *,limitmalloc_pool *,str0 *,str0 *);
/* Visit each matching suffix in tree order. Before each callback, *result
   holds an allocated suffix owned by the caller. The callback takes no
   arguments and may access *result through caller-managed state. It returns
   1 to continue or 0 to stop. Return 1 after all matches (including none),
   0 on early stop, -1 on suffix allocation failure. A null callback stops
   after the first match. On allocation failure, *result keeps its last
   successful value; free it with str0_free using the same pool. */
extern int critbit0_allprefixed(critbit0_tree *,limitmalloc_pool *,str0 *,str0 *,int (*)(void));
/* Pass the tree's dump image, in order, to put(bytes, length), which returns
   1 to continue or 0 to stop. Return 1 after the whole image, 0 when put
   stops. Allocates nothing. The image is a 32-byte header (the bytes
   "critbit0", then little-endian uint64 version 1, total length, and root
   reference) followed by records in postorder; README.md gives the layout. */
extern int critbit0_dump(critbit0_tree *,int (*)(const char *,int64));

#endif
