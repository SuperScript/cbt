#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "critbit0.h"

static str0 suffix;
static int output_failed;
static const char *match_prefix;
static FILE *snapshot_output;
static FILE *dump_output;
static int raw_stream;
static int terminator = '\n';
static int mapped;
static const char *map;
static int64 map_length;
static critbit0_image image;

static void response(const char *message)
{
  fputs(message, stdout);
  putchar(terminator);
}

/* Report a failure of command, or of cbt itself when command is null. */
static int fail(const char *command, const char *message)
{
  fflush(stdout);
  fprintf(stderr, "cbt: fatal: %s%s%s\n", command ? command : "",
          command ? ": " : "", message);
  return 1;
}

/* In -m mode, search the mapped dump instead of the tree. */
static int contains(critbit0_tree *tree, str0 *key)
{
  if (mapped) return critbit0_image_contains(&image, key);
  return critbit0_contains(tree, key);
}

static int firstprefixed(critbit0_tree *tree, limitmalloc_pool *pool,
                         str0 *prefix)
{
  if (mapped)
    return critbit0_image_firstprefixed(&image, pool, &suffix, prefix);
  return critbit0_firstprefixed(tree, pool, &suffix, prefix);
}

static int allprefixed(critbit0_tree *tree, limitmalloc_pool *pool,
                       str0 *prefix, int (*handle)(void))
{
  if (mapped)
    return critbit0_image_allprefixed(&image, pool, &suffix, prefix, handle);
  return critbit0_allprefixed(tree, pool, &suffix, prefix, handle);
}

static int save_key(void)
{
  fputs(suffix, snapshot_output);
  fputc(terminator, snapshot_output);
  return !ferror(snapshot_output);
}

enum { RECORDS_OK, RECORDS_MALFORMED, RECORDS_READ_ERROR, RECORDS_NO_MEMORY };

static const char *const records_failure[] = {
  0, "malformed record", "read error", "out of memory"
};

static int read_records(FILE *input, critbit0_tree *tree,
                        limitmalloc_pool *pool, int *saw_marker)
{
  char *line = 0;
  size_t capacity = 0;
  ssize_t length;
  int result = RECORDS_OK;

  *saw_marker = 0;
  while ((length = getdelim(&line, &capacity, terminator, input)) != -1) {
    str0 key;
    if (length == 0 || line[length - 1] != terminator ||
        memchr(line, 0, (size_t) length - 1)) {
      result = RECORDS_MALFORMED;
      break;
    }
    line[length - 1] = 0;
    if (line[0] == 0) {
      *saw_marker = 1;
      break;
    }
    key = line;
    if (!critbit0_insert(tree, pool, &key)) {
      result = RECORDS_NO_MEMORY;
      break;
    }
  }
  if (ferror(input)) result = RECORDS_READ_ERROR;
  free(line);
  return result;
}

static critbit0_tree *layer_destination;
static limitmalloc_pool *layer_pool;

struct layer_added {
  struct layer_added *next;
  char *key;
};

static struct layer_added *added_keys;

static void discard_added(void)
{
  while (added_keys) {
    struct layer_added *item = added_keys;
    added_keys = item->next;
    free(item);
  }
}

static void undo_added(void)
{
  while (added_keys) {
    struct layer_added *item = added_keys;
    str0 key = item->key;
    added_keys = item->next;
    critbit0_delete(layer_destination, layer_pool, &key);
    free(item);
  }
}

static int layer_key(void)
{
  struct layer_added *item;
  size_t length;

  if (critbit0_contains(layer_destination, &suffix)) return 1;
  if (!critbit0_insert(layer_destination, layer_pool, &suffix)) return 0;
  length = strlen(suffix);
  item = malloc(sizeof *item + length + 1);
  if (!item) {
    critbit0_delete(layer_destination, layer_pool, &suffix);
    return 0;
  }
  item->key = (char *) (item + 1);
  memcpy(item->key, suffix, length + 1);
  item->next = added_keys;
  added_keys = item;
  return 1;
}

static int layer_records(critbit0_tree *source, critbit0_tree *destination,
                         limitmalloc_pool *pool)
{
  str0 prefix = "";
  int result;

  layer_destination = destination;
  layer_pool = pool;
  added_keys = 0;
  result = critbit0_allprefixed(source, pool, &suffix, &prefix, layer_key);
  str0_free(&suffix, pool);
  if (result != 1) undo_added();
  else discard_added();
  critbit0_clear(source, pool);
  return result == 1;
}

static int load_snapshot(const char *path, critbit0_tree *tree,
                         limitmalloc_pool *pool)
{
  critbit0_tree incoming = { 0 };
  FILE *input = fopen(path, "r");
  const char *failure = 0;
  int saw_marker = 0;
  int result;

  if (!input) return fail("load", "cannot open file");
  result = read_records(input, &incoming, pool, &saw_marker);
  if (result != RECORDS_OK) failure = records_failure[result];
  else if (!saw_marker) failure = "missing marker";
  else {
    char *extra = 0;
    size_t capacity = 0;
    if (getdelim(&extra, &capacity, terminator, input) != -1)
      failure = "data after marker";
    else if (ferror(input)) failure = "read error";
    free(extra);
  }
  if (fclose(input) == EOF && !failure) failure = "read error";
  if (failure) {
    critbit0_clear(&incoming, pool);
    return fail("load", failure);
  }
  if (!layer_records(&incoming, tree, pool))
    return fail("load", "out of memory");
  return 0;
}

static int read_snapshot(critbit0_tree *tree, limitmalloc_pool *pool)
{
  critbit0_tree incoming = { 0 };
  const char *failure = 0;
  int saw_marker = 0;
  int result;

  result = read_records(stdin, &incoming, pool, &saw_marker);
  if (result != RECORDS_OK) failure = records_failure[result];
  else if (!saw_marker) failure = "missing marker";
  if (failure) {
    critbit0_clear(&incoming, pool);
    return fail("read", failure);
  }
  if (!layer_records(&incoming, tree, pool))
    return fail("read", "out of memory");
  return 0;
}

static int save_snapshot(const char *path, critbit0_tree *tree,
                         limitmalloc_pool *pool)
{
  FILE *output;
  str0 prefix = "";
  str0 empty = "";
  int result;
  int ok = 1;

  result = contains(tree, &empty);
  if (result < 0) return fail("save", "malformed dump");
  if (result) return fail("save", "empty key cannot be stored");
  output = fopen(path, "w");
  if (!output) return fail("save", "cannot open file");
  snapshot_output = output;
  result = allprefixed(tree, pool, &prefix, save_key);
  str0_free(&suffix, pool);
  if (result < 0) {
    fclose(output);
    if (result == -2) return fail("save", "malformed dump");
    return fail("save", "out of memory");
  }
  if (result != 1 || ferror(output)) ok = 0;
  if (ok && fputc(terminator, output) == EOF) ok = 0;
  if (fclose(output) == EOF) ok = 0;
  if (!ok) return fail("save", "write error");
  return 0;
}

static int write_snapshot(critbit0_tree *tree, limitmalloc_pool *pool)
{
  str0 empty = "";
  str0 prefix = "";
  int result;

  result = contains(tree, &empty);
  if (result < 0) return fail("write", "malformed dump");
  if (result) return fail("write", "empty key cannot be stored");
  snapshot_output = stdout;
  result = allprefixed(tree, pool, &prefix, save_key);
  str0_free(&suffix, pool);
  if (result == -2) return fail("write", "malformed dump");
  if (result < 0) return fail("write", "out of memory");
  if (result != 1 || ferror(stdout)) return fail("write", "output failed");
  if (fputc(terminator, stdout) == EOF) return fail("write", "output failed");
  return 0;
}

static int dump_put(const char *bytes, int64 length)
{
  return fwrite(bytes, 1, (size_t) length, dump_output) == (size_t) length;
}

/* Unlink path before creating it, as tinycdb's cdb -c -t - does, so a
   process that has the old file mapped keeps the old bytes. */
static int dump_tree(const char *path, critbit0_tree *tree)
{
  const char *failure = 0;
  int fd;

  unlink(path);
  fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
  if (fd == -1) return fail("dump", "cannot open file");
  dump_output = fdopen(fd, "w");
  if (!dump_output) {
    close(fd);
    return fail("dump", "cannot open file");
  }
  if (!(mapped ? dump_put(map, map_length) : critbit0_dump(tree, dump_put)) ||
      fflush(dump_output) == EOF ||
      fsync(fd) == -1) failure = "write error";
  if (fclose(dump_output) == EOF && !failure) failure = "write error";
  if (failure) return fail("dump", failure);
  return 0;
}

static int match(void)
{
  fputs(match_prefix, stdout);
  fputs(suffix, stdout);
  putchar(terminator);
  if (ferror(stdout)) {
    output_failed = 1;
    return 0;
  }
  return 1;
}

static int extension(void)
{
  if (!*suffix) return 1;
  fputs(suffix, stdout);
  putchar(terminator);
  if (ferror(stdout)) {
    output_failed = 1;
    return 0;
  }
  return 1;
}

static int command_has(char *operand, critbit0_tree *tree,
                       limitmalloc_pool *pool)
{
  str0 key = operand;

  int result = contains(tree, &key);

  (void) pool;
  if (result < 0) return fail("has", "malformed dump");
  response(result ? "1" : "0");
  return 0;
}

static int command_add(char *operand, critbit0_tree *tree,
                       limitmalloc_pool *pool)
{
  str0 key = operand;
  int result = critbit0_insert(tree, pool, &key);

  response(result == 2 ? "2" : result == 1 ? "1" : "0");
  if (!result) return fail("add", "out of memory");
  return 0;
}

static int command_rm(char *operand, critbit0_tree *tree,
                      limitmalloc_pool *pool)
{
  str0 key = operand;

  response(critbit0_delete(tree, pool, &key) ? "1" : "0");
  return 0;
}

static int command_after(char *operand, critbit0_tree *tree,
                         limitmalloc_pool *pool)
{
  str0 key = operand;
  int result;

  errno = 0;
  result = firstprefixed(tree, pool, &key);
  if (result < 0) return fail("after", "malformed dump");
  if (!result && errno) return fail("after", "out of memory");
  if (result) {
    response(suffix);
    str0_free(&suffix, pool);
  }
  return 0;
}

static int command_list(char *operand, critbit0_tree *tree,
                        limitmalloc_pool *pool)
{
  str0 key = operand;
  int result;

  if (!*key) {
    result = contains(tree, &key);
    if (result < 0) return fail("list", "malformed dump");
    if (result) return fail("list", "empty key cannot be listed");
  }
  match_prefix = key;
  result = allprefixed(tree, pool, &key, match);
  str0_free(&suffix, pool);
  if (result == -2) return fail("list", "malformed dump");
  if (result < 0) return fail("list", "out of memory");
  if (output_failed || result == 0) return fail("list", "output failed");
  response("");
  return 0;
}

static int command_complete(char *operand, critbit0_tree *tree,
                            limitmalloc_pool *pool)
{
  str0 key = operand;
  int result;

  result = allprefixed(tree, pool, &key, extension);
  str0_free(&suffix, pool);
  if (result == -2) return fail("complete", "malformed dump");
  if (result < 0) return fail("complete", "out of memory");
  if (output_failed || result == 0)
    return fail("complete", "output failed");
  response("");
  return 0;
}

static int command_load(char *operand, critbit0_tree *tree,
                        limitmalloc_pool *pool)
{
  return load_snapshot(operand, tree, pool);
}

static int command_save(char *operand, critbit0_tree *tree,
                        limitmalloc_pool *pool)
{
  return save_snapshot(operand, tree, pool);
}

static int command_dump(char *operand, critbit0_tree *tree,
                        limitmalloc_pool *pool)
{
  (void) pool;
  return dump_tree(operand, tree);
}

static int command_clear(char *operand, critbit0_tree *tree,
                         limitmalloc_pool *pool)
{
  (void) operand;
  critbit0_clear(tree, pool);
  return 0;
}

static int command_read(char *operand, critbit0_tree *tree,
                        limitmalloc_pool *pool)
{
  (void) operand;
  return read_snapshot(tree, pool);
}

static int command_write(char *operand, critbit0_tree *tree,
                         limitmalloc_pool *pool)
{
  (void) operand;
  return write_snapshot(tree, pool);
}

static int command_exit(char *operand, critbit0_tree *tree,
                        limitmalloc_pool *pool)
{
  (void) operand;
  (void) tree;
  (void) pool;
  return -1;
}

enum { OPERAND_NONE, OPERAND_REQUIRED, OPERAND_OPTIONAL };

static const struct command {
  const char *name;
  int takes_operand;
  int changes_tree;
  int (*run)(char *, critbit0_tree *, limitmalloc_pool *);
} commands[] = {
  { "has", OPERAND_REQUIRED, 0, command_has },
  { "add", OPERAND_REQUIRED, 1, command_add },
  { "rm", OPERAND_REQUIRED, 1, command_rm },
  { "after", OPERAND_REQUIRED, 0, command_after },
  { "list", OPERAND_OPTIONAL, 0, command_list },
  { "complete", OPERAND_REQUIRED, 0, command_complete },
  { "load", OPERAND_REQUIRED, 1, command_load },
  { "save", OPERAND_REQUIRED, 0, command_save },
  { "dump", OPERAND_REQUIRED, 0, command_dump },
  { "clear", OPERAND_NONE, 1, command_clear },
  { "read", OPERAND_NONE, 1, command_read },
  { "write", OPERAND_NONE, 0, command_write },
  { "exit", OPERAND_NONE, 0, command_exit },
  { 0, 0, 0, 0 }
};

/* Return 0 to continue, 1 after reporting a failure, or -1 to exit. */
static int dispatch(char *line, critbit0_tree *tree, limitmalloc_pool *pool)
{
  static char empty[1];
  char *operand = strchr(line, ' ');
  const struct command *command;

  if (operand) *operand++ = 0;
  for (command = commands; command->name; command++)
    if (strcmp(line, command->name) == 0) break;
  if (!command->name) return fail(0, "unknown command");
  if (command->takes_operand == OPERAND_OPTIONAL && !operand) operand = empty;
  if (command->takes_operand == OPERAND_REQUIRED && !operand)
    return fail(command->name, "missing operand");
  if (command->takes_operand == OPERAND_NONE && operand)
    return fail(command->name, "unexpected operand");
  if (mapped && command->changes_tree)
    return fail(command->name, "read-only tree");
  return command->run(operand, tree, pool);
}

/* Map the dump at path read-only and search it in place, as tinycdb's
   cdb_init maps a cdb file. */
static int map_dump(const char *path)
{
  struct stat status;
  void *bytes;
  int fd;

  fd = open(path, O_RDONLY);
  if (fd == -1) return fail("-m", "cannot open file");
  if (fstat(fd, &status) == -1) {
    close(fd);
    return fail("-m", "cannot map file");
  }
  if (!S_ISREG(status.st_mode)) {
    close(fd);
    return fail("-m", "cannot open file");
  }
  if (status.st_size < 32) {
    close(fd);
    return fail("-m", "malformed dump");
  }
  if ((uintmax_t) status.st_size > SIZE_MAX) {
    close(fd);
    return fail("-m", "cannot map file");
  }
  bytes = mmap(0, (size_t) status.st_size, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (bytes == MAP_FAILED) return fail("-m", "cannot map file");
  map = bytes;
  map_length = status.st_size;
  if (!critbit0_image_init(&image, map, map_length))
    return fail("-m", "malformed dump");
  mapped = 1;
  return 0;
}

int main(int argc, char **argv)
{
  critbit0_tree tree = { 0 };
  limitmalloc_pool pool = { LIMITMALLOC_MAX, 0, 0 };
  const char *dump_path = 0;
  char *line = 0;
  size_t capacity = 0;
  ssize_t length;
  int status = 0;
  int option;

  while ((option = getopt(argc, argv, ":0m:")) != -1)
    switch (option) {
      case '0':
        if (raw_stream) return fail(0, "unsupported option");
        raw_stream = 1;
        terminator = 0;
        break;
      case 'm':
        if (dump_path) return fail(0, "unsupported option");
        dump_path = optarg;
        break;
      default:
        return fail(0, "unsupported option");
    }
  if (optind != argc) return fail(0, "unsupported option");
  if (dump_path && map_dump(dump_path)) return 1;
  signal(SIGPIPE, SIG_IGN);

  while ((length = getdelim(&line, &capacity, terminator, stdin)) != -1) {
    if (length == 0 || line[length - 1] != terminator) {
      status = fail(0, "unterminated request");
      break;
    }
    line[length - 1] = 0;
    if (!raw_stream && memchr(line, 0, (size_t) length - 1)) {
      status = fail(0, "NUL in request");
      break;
    }
    status = dispatch(line, &tree, &pool);
    if (status < 0) {
      status = 0;
      break;
    }
    if (status || fflush(stdout) == EOF || ferror(stdout)) {
      if (!status) status = fail(0, "output failed");
      break;
    }
  }
  if (!status && ferror(stdin)) status = fail(0, "input failed");
  if (!status && fflush(stdout) == EOF) status = fail(0, "output failed");
  critbit0_clear(&tree, &pool);
  free(line);
  return status;
}
