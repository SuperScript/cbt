#include "critbit0.h"
#include "uint64.h"
#define child critbit0_private_child
#define root critbit0_private_root
#define byte critbit0_private_byte
#define otherbits critbit0_private_otherbits

#define HEADER 32
#define NODE 24

static const char zero[8];

static void pack(uint8 *out,uint64 u,int64 n)
{
  int64 i;

  for (i = 0;i < n;++i) { out[i] = u; u >>= 8; }
}

static int64 keylength(uint8 *p)
{
  int64 i;

  i = 0; while (p[i]) ++i;
  return i;
}

/* Bytes the key's record takes: the key and NUL, padded to 8. */
static uint64 keysize(uint8 *p)
{
  return (keylength(p) + 8) & ~(uint64) 7;
}

static uint64 size(void *top)
{
  uint8 *p;
  critbit0_node *q;

  p = top;
  if (1 & (int) p) {
    q = (void *) (p - 1);
    return size(q->child[0]) + size(q->child[1]) + NODE;
  }
  return keysize(p);
}

/* Emit the subtree in postorder from *offset on. Return its reference,
   which is never 0, or 0 when put stops. */
static uint64 emit(void *top,uint64 *offset,int (*put)(const char *,int64))
{
  uint8 *p;
  critbit0_node *q;
  uint8 record[NODE];
  uint64 r0;
  uint64 r1;
  uint64 at;
  int64 i;
  int64 n;

  p = top;
  if (1 & (int) p) {
    q = (void *) (p - 1);
    r0 = emit(q->child[0],offset,put);
    if (!r0) return 0;
    r1 = emit(q->child[1],offset,put);
    if (!r1) return 0;
    pack(record,r0,8);
    pack(record + 8,r1,8);
    pack(record + 16,q->byte,4);
    pack(record + 20,q->otherbits,4);
    at = *offset;
    if (!put((const char *) record,NODE)) return 0;
    *offset += NODE;
    return at + 1;
  }
  i = keylength(p);
  n = keysize(p);
  at = *offset;
  if (!put((const char *) p,i + 1)) return 0;
  if (n > i + 1 && !put(zero,n - i - 1)) return 0;
  *offset += n;
  return at;
}

int critbit0_dump(critbit0_tree *t,int (*put)(const char *,int64))
{
  static const char magic[8] = "critbit0";
  uint8 header[HEADER];
  uint64 length;
  uint64 top;
  uint64 offset;
  int64 i;

  length = HEADER;
  top = 0;
  if (t->root) {
    length += size(t->root);
    if (1 & (int) t->root) top = length - NODE + 1;
    else top = HEADER;
  }
  for (i = 0;i < 8;++i) header[i] = magic[i];
  pack(header + 8,1,8);
  pack(header + 16,length,8);
  pack(header + 24,top,8);
  if (!put((const char *) header,HEADER)) return 0;
  offset = HEADER;
  if (t->root && !emit(t->root,&offset,put)) return 0;
  return 1;
}
