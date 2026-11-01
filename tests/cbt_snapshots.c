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
  struct stat before;
  struct stat after;
  int pass = 1;

  if (argc != 3) return 1;
  if (critbit0_insert(&tree, &pool, &key) != 2) return 1;
  file = fopen(argv[1], "w");
  if (!file) return 1;
  if (fputs("valid\n\\x00\n", file) == EOF || fclose(file) == EOF) return 1;
  pass &= check("invalid load rejected", load_snapshot(argv[1], &tree, &pool) != 0);
  pass &= check("invalid load retains tree", critbit0_contains(&tree, &key));
  if (chmod(argv[2], 0640) == -1 || stat(argv[2], &before) == -1) return 1;
  pool.limitmalloc_private_limit = pool.limitmalloc_private_allocated;
  pass &= check("failed save rejected", save_snapshot(argv[2], &tree, &pool) != 0);
  pass &= check("failed save retains file", stat(argv[2], &after) == 0 &&
                before.st_ino == after.st_ino &&
                before.st_mode == after.st_mode);
  file = fopen(argv[2], "r");
  if (!file) return 1;
  pass &= check("failed save retains content",
                fgets(contents, sizeof contents, file) != 0 &&
                strcmp(contents, "prior\n") == 0 && fgetc(file) == EOF);
  if (fclose(file) == EOF) return 1;
  pool.limitmalloc_private_limit = LIMITMALLOC_MAX;
  pass &= check("save succeeds", save_snapshot(argv[2], &tree, &pool) == 0);
  pass &= check("save retains metadata", stat(argv[2], &after) == 0 &&
                (after.st_mode & 07777) == 0640 &&
                after.st_uid == before.st_uid && after.st_gid == before.st_gid);
  critbit0_clear(&tree, &pool);
  return pass ? 0 : 1;
}
