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

int critbit0_image_init(critbit0_image *m,const char *image,int64 imagelen)
{
  static const char magic[8] = "critbit0";
  const uint8 *in = (const void *) image;
  uint64 top;
  uint64 at;
  int64 i;

  if (imagelen < HEADER) return 0;
  for (i = 0;i < 8;++i) if (image[i] != magic[i]) return 0;
  if (unpack(in + 8,8) != 1) return 0;
  if (unpack(in + 16,8) != (uint64) imagelen) return 0;
  top = unpack(in + 24,8);
  if (top) {
    at = top & ~(uint64) 1;
    if (at & 7) return 0;
    if (at < HEADER || at >= (uint64) imagelen) return 0;
    if ((top & 1) && (uint64) imagelen - at < NODE) return 0;
  }
  m->bytes = image;
  m->length = imagelen;
  m->root = top;
  return 1;
}
