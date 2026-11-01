#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "critbit0.h"

static str0 suffix;
static int output_failed;

static int fail(const char *message)
{
  fprintf(stderr, "cbt: %s\n", message);
  return 1;
}

static int hex_digit(unsigned char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static int decode(char *s)
{
  char *in = s;
  char *out = s;
  unsigned char c;
  int high;
  int low;

  while ((c = (unsigned char) *in++) != 0) {
    if (c == '\\') {
      c = (unsigned char) *in++;
      switch (c) {
        case '\\': c = '\\'; break;
        case 't': c = '\t'; break;
        case 'n': c = '\n'; break;
        case 'r': c = '\r'; break;
        case 'x':
          high = hex_digit((unsigned char) in[0]);
          if (high < 0 || in[0] == 0) return 0;
          low = hex_digit((unsigned char) in[1]);
          if (low < 0 || in[1] == 0) return 0;
          c = (unsigned char) ((high << 4) | low);
          in += 2;
          if (c == 0 || c == '\t' || c == '\n' || c == '\r' ||
              c == '\\' || (c >= 0x20 && c <= 0x7e)) return 0;
          break;
        default: return 0;
      }
    } else if (c < 0x20 || c > 0x7e) {
      return 0;
    }
    *out++ = (char) c;
  }
  *out = 0;
  return 1;
}

static void encode(const char *s)
{
  static const char hex[] = "0123456789ABCDEF";
  const unsigned char *p = (const unsigned char *) s;
  unsigned char c;

  while ((c = *p++) != 0) {
    if (c == '\\') fputs("\\\\", stdout);
    else if (c == '\t') fputs("\\t", stdout);
    else if (c == '\n') fputs("\\n", stdout);
    else if (c == '\r') fputs("\\r", stdout);
    else if (c < 0x20 || c > 0x7e) {
      fputs("\\x", stdout);
      putchar(hex[c >> 4]);
      putchar(hex[c & 15]);
    } else putchar(c);
  }
}

static int match(void)
{
  fputs("match\t", stdout);
  encode(suffix);
  putchar('\n');
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
    puts("cleared");
    return 0;
  }
  *tab++ = 0;
  if (!decode(tab)) return fail("invalid operand");
  key = tab;
  if (strcmp(line, "contains") == 0) {
    puts(critbit0_contains(tree, &key) ? "present" : "absent");
  } else if (strcmp(line, "insert") == 0) {
    result = critbit0_insert(tree, pool, &key);
    if (!result) return fail("insert failed");
    puts(result == 2 ? "inserted" : "exists");
  } else if (strcmp(line, "delete") == 0) {
    puts(critbit0_delete(tree, pool, &key) ? "deleted" : "missing");
  } else if (strcmp(line, "firstprefixed") == 0) {
    errno = 0;
    result = critbit0_firstprefixed(tree, pool, &suffix, &key);
    if (!result && errno) return fail("prefix lookup failed");
    if (result) {
      fputs("found\t", stdout);
      encode(suffix);
      putchar('\n');
      str0_free(&suffix, pool);
    } else puts("notfound");
  } else if (strcmp(line, "allprefixed") == 0) {
    result = critbit0_allprefixed(tree, pool, &suffix, &key, match);
    str0_free(&suffix, pool);
    if (result < 0) return fail("prefix lookup failed");
    if (output_failed || result == 0) return fail("output failed");
    puts("end");
  } else {
    return fail("invalid request");
  }
  return 0;
}

int main(void)
{
  critbit0_tree tree = { 0 };
  limitmalloc_pool pool = { LIMITMALLOC_MAX, 0, 0 };
  char *line = 0;
  size_t capacity = 0;
  ssize_t length;
  int status = 0;

  while ((length = getline(&line, &capacity, stdin)) != -1) {
    if (length == 0 || line[length - 1] != '\n') {
      status = fail("unterminated request");
      break;
    }
    line[length - 1] = 0;
    if (memchr(line, 0, (size_t) length - 1)) {
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
