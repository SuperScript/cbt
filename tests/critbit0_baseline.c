#include <stdio.h>
#include <string.h>
#include "critbit0.h"

static str0 suffix;
static int saw_a;
static int saw_b;

static int collect(void)
{
  if (strcmp(suffix, "a") == 0) saw_a = 1;
  if (strcmp(suffix, "b") == 0) saw_b = 1;
  return 1;
}

int main(void)
{
  critbit0_tree tree = { 0 };
  limitmalloc_pool pool = { 1024, 0, 0 };
  str0 a = "ca";
  str0 b = "cb";
  str0 prefix = "c";
  str0 result = 0;

  printf("insert a: %d\n", critbit0_insert(&tree, &pool, &a));
  printf("insert b: %d\n", critbit0_insert(&tree, &pool, &b));
  printf("insert existing: %d\n", critbit0_insert(&tree, &pool, &a));
  printf("contains a: %d\n", critbit0_contains(&tree, &a));
  printf("first prefixed: %d\n", critbit0_firstprefixed(&tree, &pool, &result, &prefix));
  printf("first suffix: %s\n", result);
  str0_free(&result, &pool);
  printf("all prefixed: %d\n", critbit0_allprefixed(&tree, &pool, &suffix, &prefix, collect));
  printf("suffixes a,b: %d,%d\n", saw_a, saw_b);
  str0_free(&suffix, &pool);
  printf("delete a: %d\n", critbit0_delete(&tree, &pool, &a));
  printf("contains a after delete: %d\n", critbit0_contains(&tree, &a));
  critbit0_clear(&tree, &pool);
  printf("contains b after clear: %d\n", critbit0_contains(&tree, &b));
  return 0;
}
