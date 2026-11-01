#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "critbit0.h"

static str0 suffix;
static int output_failed;
static FILE *snapshot_output;
static int raw_stream;

static void response(const char *message)
{
  fputs(message, stdout);
  putchar(raw_stream ? 0 : '\n');
}

static void response_suffix(const char *tag, const char *value)
{
  fputs(tag, stdout);
  putchar('\t');
  fputs(value, stdout);
  putchar(raw_stream ? 0 : '\n');
}

static int fail(const char *message)
{
  fprintf(stderr, "cbt: %s\n", message);
  return 1;
}

static int save_key(void)
{
  fputs(suffix, snapshot_output);
  fputc('\n', snapshot_output);
  return !ferror(snapshot_output);
}

static int load_snapshot(const char *path, critbit0_tree *tree,
                         limitmalloc_pool *pool)
{
  critbit0_tree replacement = { 0 };
  FILE *input = fopen(path, "r");
  char *line = 0;
  size_t capacity = 0;
  ssize_t length;
  int ok = 1;

  if (!input) return fail("load failed");
  while ((length = getline(&line, &capacity, input)) != -1) {
    str0 key;
    if (length == 0 || line[length - 1] != '\n' ||
        memchr(line, 0, (size_t) length - 1)) {
      ok = 0;
      break;
    }
    line[length - 1] = 0;
    key = line;
    if (!critbit0_insert(&replacement, pool, &key)) {
      ok = 0;
      break;
    }
  }
  if (ferror(input)) ok = 0;
  if (fclose(input) == EOF) ok = 0;
  free(line);
  if (!ok) {
    critbit0_clear(&replacement, pool);
    return fail("load failed");
  }
  critbit0_clear(tree, pool);
  *tree = replacement;
  response("loaded");
  return 0;
}

static int save_snapshot(const char *path, critbit0_tree *tree,
                         limitmalloc_pool *pool)
{
  struct stat prior;
  mode_t mode;
  mode_t mask;
  int existed = 1;
  char *temporary;
  size_t length = strlen(path);
  int fd;
  FILE *output;
  str0 prefix = "";
  int result;
  int ok = 1;

  if (lstat(path, &prior) == -1) {
    if (errno != ENOENT) return fail("save failed");
    existed = 0;
  } else if (!S_ISREG(prior.st_mode)) {
    return fail("save failed");
  }
  if (length > (size_t) -1 - 8) return fail("save failed");
  temporary = malloc(length + 8);
  if (!temporary) return fail("save failed");
  memcpy(temporary, path, length);
  memcpy(temporary + length, ".XXXXXX", 8);
  fd = mkstemp(temporary);
  if (fd == -1) {
    free(temporary);
    return fail("save failed");
  }
  if (existed) {
    mode = prior.st_mode & 07777;
    if (fchown(fd, prior.st_uid, prior.st_gid) == -1) ok = 0;
  } else {
    mask = umask(0);
    umask(mask);
    mode = 0666 & ~mask;
  }
  if (ok && fchmod(fd, mode) == -1) ok = 0;
  output = fdopen(fd, "w");
  if (!output) {
    close(fd);
    ok = 0;
  }
  if (ok) {
    snapshot_output = output;
    result = critbit0_allprefixed(tree, pool, &suffix, &prefix, save_key);
    str0_free(&suffix, pool);
    if (result != 1 || ferror(output)) ok = 0;
  }
  if (output && fclose(output) == EOF) ok = 0;
  if (ok) {
    struct stat current;
    if (existed) {
      if (lstat(path, &current) == -1 || !S_ISREG(current.st_mode)) ok = 0;
    } else if (lstat(path, &current) != -1 || errno != ENOENT) {
      ok = 0;
    }
  }
  if (ok && rename(temporary, path) == -1) ok = 0;
  if (!ok) unlink(temporary);
  free(temporary);
  if (!ok) return fail("save failed");
  response("saved");
  return 0;
}

static int match(void)
{
  response_suffix("match", suffix);
  if (ferror(stdout)) {
    output_failed = 1;
    return 0;
  }
  return 1;
}

static int dispatch(char *line, critbit0_tree *tree, limitmalloc_pool *pool)
{
  char *tab = strchr(line, '\t');
  str0 key;
  int result;

  if (!tab) {
    if (strcmp(line, "clear") != 0) return fail("invalid request");
    critbit0_clear(tree, pool);
    response("cleared");
    return 0;
  }
  *tab++ = 0;
  key = tab;
  if (strcmp(line, "load") == 0) {
    return load_snapshot(key, tree, pool);
  } else if (strcmp(line, "save") == 0) {
    return save_snapshot(key, tree, pool);
  } else if (strcmp(line, "contains") == 0) {
    response(critbit0_contains(tree, &key) ? "present" : "absent");
  } else if (strcmp(line, "insert") == 0) {
    result = critbit0_insert(tree, pool, &key);
    if (!result) return fail("insert failed");
    response(result == 2 ? "inserted" : "exists");
  } else if (strcmp(line, "delete") == 0) {
    response(critbit0_delete(tree, pool, &key) ? "deleted" : "missing");
  } else if (strcmp(line, "firstprefixed") == 0) {
    errno = 0;
    result = critbit0_firstprefixed(tree, pool, &suffix, &key);
    if (!result && errno) return fail("prefix lookup failed");
    if (result) {
      response_suffix("found", suffix);
      str0_free(&suffix, pool);
    } else response("notfound");
  } else if (strcmp(line, "allprefixed") == 0) {
    result = critbit0_allprefixed(tree, pool, &suffix, &key, match);
    str0_free(&suffix, pool);
    if (result < 0) return fail("prefix lookup failed");
    if (output_failed || result == 0) return fail("output failed");
    response("end");
  } else {
    return fail("invalid request");
  }
  return 0;
}

int main(int argc, char **argv)
{
  critbit0_tree tree = { 0 };
  limitmalloc_pool pool = { LIMITMALLOC_MAX, 0, 0 };
  char *line = 0;
  size_t capacity = 0;
  ssize_t length;
  int status = 0;
  int terminator;

  if (argc == 1) raw_stream = 0;
  else if (argc == 2 && strcmp(argv[1], "-0") == 0) raw_stream = 1;
  else return fail("unsupported option");
  terminator = raw_stream ? 0 : '\n';

  while ((length = getdelim(&line, &capacity, terminator, stdin)) != -1) {
    if (length == 0 || line[length - 1] != terminator) {
      status = fail("unterminated request");
      break;
    }
    line[length - 1] = 0;
    if (!raw_stream && memchr(line, 0, (size_t) length - 1)) {
      status = fail("invalid request");
      break;
    }
    status = dispatch(line, &tree, &pool);
    if (status || ferror(stdout)) {
      if (!status) status = fail("output failed");
      break;
    }
  }
  if (!status && ferror(stdin)) status = fail("input failed");
  if (!status && fflush(stdout) == EOF) status = fail("output failed");
  critbit0_clear(&tree, &pool);
  free(line);
  return status;
}
