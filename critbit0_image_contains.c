/*
%use str0_length;
%use str0_equalc;
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

int critbit0_image_contains(critbit0_image *m,str0 *u)
{
  const uint8 *in = (const void *) m->bytes;
  const uint8 *ubytes = (void *) *u;
  int64 ulen = str0_length(u);
  uint64 p = m->root;
  uint64 limit = m->length;
  uint64 at;
  uint32 byte;
  uint32 otherbits;
  uint8 c;
  int direction;

  if (!p) return 0; /* S = {} */

  while (1 & p) {
    if (!valid(in,p,limit)) return -1;
    at = p - 1;
    byte = unpack(in + at + 16,4);
    otherbits = in[at + 20];
    c = 0;
    if (byte < ulen) c = ubytes[byte];
    direction = (1 + (otherbits | c)) >> 8;
    p = unpack(in + at + 8 * direction,8);
    limit = at;
  }

  if (!valid(in,p,limit)) return -1;
  return str0_equalc(u,(const char *) in + p);
}
