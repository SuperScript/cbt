#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "critbit0.h"

static str0 suffix;
static int visits;
static int saw_a;
static int saw_b;

static int collect(void)
{
  ++visits;
  if (strcmp(suffix, "a") == 0) saw_a = 1;
  if (strcmp(suffix, "b") == 0) saw_b = 1;
  return 1;
}

static int stop(void)
{
  ++visits;
  return 0;
}

static void check(const char *name, int ok)
{
  printf("%s: %s\n", name, ok ? "yes" : "no");
}

int main(void)
{
  critbit0_tree tree = { 0 };
  limitmalloc_pool pool = { 1024, 0, 0 };
  str0 ca = "ca";
  str0 cb = "cb";
  str0 c = "c";
  str0 d = "d";
  str0 longer = "cab";
  str0 result = 0;
  int64 used;

  check("empty membership", critbit0_contains(&tree, &ca) == 0);
  check("empty delete", critbit0_delete(&tree, &pool, &ca) == 0);
  check("empty first", critbit0_firstprefixed(&tree, &pool, &result, &c) == 0);
  check("empty all", critbit0_allprefixed(&tree, &pool, &result, &c, collect) == 1 && visits == 0);
  critbit0_clear(&tree, &pool);
  check("empty clear", pool.limitmalloc_private_allocated == 0);

  check("insert new", critbit0_insert(&tree, &pool, &ca) == 2);
  check("insert existing", critbit0_insert(&tree, &pool, &ca) == 1);
  check("insert branch", critbit0_insert(&tree, &pool, &cb) == 2);
  check("membership present", critbit0_contains(&tree, &ca) == 1);
  check("membership missing", critbit0_contains(&tree, &d) == 0);
  check("first suffix", critbit0_firstprefixed(&tree, &pool, &result, &c) == 1 && strcmp(result, "a") == 0);
  errno = 0;
  check("first missing", critbit0_firstprefixed(&tree, &pool, &result, &d) == 0 && strcmp(result, "a") == 0 && errno == 0);
  check("first longer prefix", critbit0_firstprefixed(&tree, &pool, &result, &longer) == 0);
  str0_free(&result, &pool);

  check("all suffixes", critbit0_allprefixed(&tree, &pool, &suffix, &c, collect) == 1 && saw_a && saw_b && visits == 2);
  visits = 0;
  check("all early stop", critbit0_allprefixed(&tree, &pool, &suffix, &c, stop) == 0 && visits == 1);
  check("all null callback", critbit0_allprefixed(&tree, &pool, &suffix, &c, 0) == 0 && strcmp(suffix, "a") == 0);
  visits = 0;
  check("all missing", critbit0_allprefixed(&tree, &pool, &suffix, &d, collect) == 1 && visits == 0);
  check("all longer prefix", critbit0_allprefixed(&tree, &pool, &suffix, &longer, collect) == 1 && visits == 0);
  check("seed prior suffix", str0_copyc(&result, &pool, "a") == 1);
  used = pool.limitmalloc_private_allocated;
  pool.limitmalloc_private_limit = used;
  check("insert allocation failure", critbit0_insert(&tree, &pool, &d) == 0 && critbit0_contains(&tree, &d) == 0 && pool.limitmalloc_private_allocated == used);
  check("existing with full pool", critbit0_insert(&tree, &pool, &ca) == 1);
  errno = 0;
  check("first allocation failure", critbit0_firstprefixed(&tree, &pool, &result, &c) == 0 && strcmp(result, "a") == 0 && errno == ENOMEM);
  check("all allocation failure", critbit0_allprefixed(&tree, &pool, &suffix, &c, collect) == -1 && strcmp(suffix, "a") == 0);
  pool.limitmalloc_private_limit = 1024;
  str0_free(&suffix, &pool);
  str0_free(&result, &pool);

  check("delete missing", critbit0_delete(&tree, &pool, &d) == 0);
  check("delete existing", critbit0_delete(&tree, &pool, &ca) == 1 && critbit0_contains(&tree, &ca) == 0);
  check("remaining key", critbit0_contains(&tree, &cb) == 1);
  critbit0_clear(&tree, &pool);
  check("clear releases", critbit0_contains(&tree, &cb) == 0 && pool.limitmalloc_private_allocated == 0);
  return 0;
}
