#include <stdio.h>
#include <critbit0.h>

static str0 suffix;
static int visits;

static int emit_insert(void)
{
  ++visits;
  return printf("insert\tc%s\n", suffix) > 0;
}

int main(void)
{
  critbit0_tree tree = { 0 };
  limitmalloc_pool pool = { 1024, 0, 0 };
  str0 ca = "ca";
  str0 cb = "cb";
  str0 c = "c";
  int ok;

  ok = critbit0_insert(&tree, &pool, &ca) == 2 &&
       critbit0_insert(&tree, &pool, &cb) == 2;
  if (ok)
    ok = critbit0_allprefixed(&tree, &pool, &suffix, &c, emit_insert) == 1 &&
         visits == 2 && !ferror(stdout);

  str0_free(&suffix, &pool);
  critbit0_clear(&tree, &pool);
  return ok ? 0 : 1;
}
