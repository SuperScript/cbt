/*
%use str0_length;
%use str0_copyc;
*/

#include "critbit0.h"
#include "uint64.h"
#define bytes critbit0_private_bytes
#define length critbit0_private_length
#define root critbit0_private_root

#define HEADER 32
#define NODE 24

static uint64 unpack(const uint8 *p,int64 n)
{
  uint64 u = 0;

  while (n-- > 0) u = (u << 8) | p[n];
  return u;
}

/* Return 1 when r names a record in [HEADER, limit): a whole node, or a key
   whose NUL lies before limit. */
static int valid(const uint8 *in,uint64 r,uint64 limit)
{
  uint64 at = r & ~(uint64) 1;

  if (at & 7) return 0;
  if (at < HEADER || at >= limit) return 0;
  if (r & 1) return limit - at >= NODE;
  while (at < limit) if (!in[at++]) return 1;
  return 0;
}

static int traverse(const uint8 *in,uint64 top,uint64 limit,limitmalloc_pool *pool,str0 *result,int (*handle)(void),int64 prefixlen)
{
  uint64 at;
  int direction;

  if (!valid(in,top,limit)) return -2;

  if (1 & top) {
    at = top - 1;
    for (direction = 0;direction < 2;++direction)
      switch(traverse(in,unpack(in + at + 8 * direction,8),at,pool,result,handle,prefixlen)) {
        case 1: break;
        case 0: return 0;
        case -1: return -1;
        default: return -2;
      }
    return 1;
  }

  while (prefixlen > 0 && in[top]) { ++top; --prefixlen; }

  if (!str0_copyc(result,pool,(const char *) in + top)) return -1;
  if (!handle) return 0;
  return handle();
}

int critbit0_image_allprefixed(critbit0_image *m,limitmalloc_pool *pool,str0 *result,str0 *prefix,int (*handle)(void))
{
  const uint8 *in = (const void *) m->bytes;
  const uint8 *ubytes = (void *) *prefix;
  int64 ulen = str0_length(prefix);
  uint64 p = m->root;
  uint64 limit = m->length;
  uint64 top = p;
  uint64 toplimit = limit;
  uint64 at;
  uint32 byte;
  uint32 otherbits;
  uint8 c;
  int direction;
  int64 i;

  if (!p) return 1; /* S = {} */

  while (1 & p) {
    if (!valid(in,p,limit)) return -2;
    at = p - 1;
    byte = unpack(in + at + 16,4);
    otherbits = in[at + 20];
    c = 0;
    if (byte < ulen) c = ubytes[byte];
    direction = (1 + (otherbits | c)) >> 8;
    p = unpack(in + at + 8 * direction,8);
    limit = at;
    if (byte < ulen) { top = p; toplimit = limit; }
  }

  if (!valid(in,p,limit)) return -2;
  for (i = 0;i < ulen;++i)
    if (!in[p + i] || in[p + i] != ubytes[i])
      return 1;

  return traverse(in,top,toplimit,pool,result,handle,ulen);
}
