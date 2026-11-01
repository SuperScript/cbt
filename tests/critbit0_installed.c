#include <stdio.h>
#include <string.h>
#include <critbit0.h>

static str0 suffix;
static int visits;

static int collect(void)
{
  if (strcmp(suffix, "a") == 0 || strcmp(suffix, "b") == 0) ++visits;
  return 1;
}

static int check(const char *name, int ok)
{
  printf("installed %s: %s\n", name, ok ? "yes" : "no");
  return ok;
}

int main(void)
{
  critbit0_tree tree = { 0 };
  limitmalloc_pool pool = { 1024, 0, 0 };
  str0 ca = "ca";
  str0 cb = "cb";
  str0 c = "c";
  str0 result = 0;
  int ok = 1;

  ok &= check("insert", critbit0_insert(&tree, &pool, &ca) == 2 &&
                        critbit0_insert(&tree, &pool, &cb) == 2);
  ok &= check("contains", critbit0_contains(&tree, &ca) == 1);
  ok &= check("first", critbit0_firstprefixed(&tree, &pool, &result, &c) == 1 &&
                       strcmp(result, "a") == 0);
  ok &= check("all", critbit0_allprefixed(&tree, &pool, &suffix, &c, collect) == 1 &&
                     visits == 2);
  str0_free(&suffix, &pool);
  str0_free(&result, &pool);
  ok &= check("delete", critbit0_delete(&tree, &pool, &ca) == 1 &&
                        critbit0_contains(&tree, &ca) == 0);
  critbit0_clear(&tree, &pool);
  ok &= check("clear", critbit0_contains(&tree, &cb) == 0 &&
                       pool.limitmalloc_private_allocated == 0);
  return ok ? 0 : 1;
}
