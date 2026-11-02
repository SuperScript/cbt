#define main cbt_program_main
#include "../cbt.c"
#undef main

static int check(const char *name, int result)
{
  printf("%s: %s\n", name, result ? "yes" : "no");
  return result;
}

int main(int argc, char **argv)
{
  critbit0_tree tree = { 0 };
  limitmalloc_pool pool = { LIMITMALLOC_MAX, 0, 0 };
  str0 key = "retained";
  FILE *file;
  char contents[16];
  int pass = 1;

  if (argc != 3) return 1;
  if (critbit0_insert(&tree, &pool, &key) != 2) return 1;
  file = fopen(argv[1], "w");
  if (!file) return 1;
  if (fwrite("valid\nx\0y\n", 1, sizeof "valid\nx\0y\n" - 1, file) !=
      sizeof "valid\nx\0y\n" - 1 || fclose(file) == EOF) return 1;
  pass &= check("invalid load rejected", load_snapshot(argv[1], &tree, &pool) != 0);
  pass &= check("invalid load retains tree", critbit0_contains(&tree, &key));
  file = fopen(argv[1], "w");
  if (!file) return 1;
  if (fwrite("valid\n", 1, 6, file) != 6 || fclose(file) == EOF) return 1;
  raw_stream = 1;
  terminator = 0;
  pass &= check("other-mode load rejected", load_snapshot(argv[1], &tree, &pool) != 0);
  pass &= check("other-mode load retains tree", critbit0_contains(&tree, &key));
  raw_stream = 0;
  terminator = '\n';
  {
    str0 extra_a = "a";
    str0 extra_b = "b";
    critbit0_tree probe = { 0 };
    int64 base = pool.limitmalloc_private_allocated;
    int64 incoming_cost;
    int64 copy_cost;

    file = fopen(argv[1], "w");
    if (!file) return 1;
    if (fputs("a\nb\n\n", file) == EOF || fclose(file) == EOF) return 1;
    if (critbit0_insert(&probe, &pool, &extra_a) != 2) return 1;
    if (critbit0_insert(&probe, &pool, &extra_b) != 2) return 1;
    incoming_cost = pool.limitmalloc_private_allocated - base;
    critbit0_clear(&probe, &pool);
    if (critbit0_insert(&tree, &pool, &extra_a) != 2) return 1;
    copy_cost = pool.limitmalloc_private_allocated - base;
    if (!critbit0_delete(&tree, &pool, &extra_a)) return 1;
    if (pool.limitmalloc_private_allocated != base) return 1;
    pool.limitmalloc_private_limit = base + incoming_cost + 2 + copy_cost;
    pass &= check("failed copy leaves the tree intact",
                  load_snapshot(argv[1], &tree, &pool) != 0 &&
                  critbit0_contains(&tree, &key) &&
                  !critbit0_contains(&tree, &extra_a) &&
                  !critbit0_contains(&tree, &extra_b) &&
                  pool.limitmalloc_private_allocated == base);
    pool.limitmalloc_private_limit = LIMITMALLOC_MAX;
  }
  raw_stream = 1;
  terminator = 0;
  pool.limitmalloc_private_limit = pool.limitmalloc_private_allocated;
  pass &= check("failed save rejected", save_snapshot(argv[2], &tree, &pool) != 0);
  file = fopen(argv[2], "r");
  if (!file) return 1;
  pass &= check("failed save leaves the file empty", fgetc(file) == EOF);
  if (fclose(file) == EOF) return 1;
  pool.limitmalloc_private_limit = LIMITMALLOC_MAX;
  pass &= check("save succeeds", save_snapshot(argv[2], &tree, &pool) == 0);
  file = fopen(argv[2], "r");
  if (!file) return 1;
  pass &= check("NUL save bytes", fread(contents, 1, 10, file) == 10 &&
                memcmp(contents, "retained\0\0", 10) == 0 && fgetc(file) == EOF);
  if (fclose(file) == EOF) return 1;
  raw_stream = 0;
  terminator = '\n';
  pass &= check("NUL load in LF mode rejected", load_snapshot(argv[2], &tree, &pool) != 0);
  pass &= check("NUL load in LF mode retains tree", critbit0_contains(&tree, &key));
  critbit0_clear(&tree, &pool);
  return pass ? 0 : 1;
}
