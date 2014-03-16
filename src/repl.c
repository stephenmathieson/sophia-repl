
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sophia.h>
#include "linenoise/linenoise.h"
#include "substr/substr.h"
#include "emitter.c/emitter.h"

/**
 * History file.
 */

static char *history_file = ".sprepl_history";

/**
 * Quit flag.
 */

static int quit = 0;

/**
 * Sophia pointers.
 */

static void *env = NULL;
static void *db = NULL;

/**
 * Help command.
 */

static void
on_help(void *data) {
  printf("\n");
  printf("  Commands:\n");
  printf("\n");
  printf("    help       Display list of commands\n");
  printf("    get <key>  Get the value of `key`\n");
  printf("    list       List all keys\n");
  printf("\n");
}

/**
 * Exit command.
 */

static void
on_exit(void *data) {
  // TODO close database
  quit = 1;
}

/**
 * Get command.
 */

static void
on_get(void *data) {
  char *line = (char *) data;
  char *key = NULL;
  char *value = NULL;
  void *ref = NULL;
  size_t keysize = 0;
  size_t valuesize = 0;

  if (!(key = substr(line, 4, -1))) {
    printf("  Key required.\n");
    return;
  }

  // TODO with updated Sophia, this isn't necessary
  if (-1 == sp_get(db, key, strlen(key) + 1, &ref, &valuesize)) {
    fprintf(stderr, "%s\n", sp_error(db));
    return;
  }

  value = (char *) ref;

  if (value) {
    printf("  %s => %s\n", key, value);
  } else {
    printf("  %s => null\n", key);
  }

  free(key);
}

/**
 * Count command.
 */

static void
on_count(void *data) {
  void *cursor = NULL;
  int count = 0;

  if (!(cursor = sp_cursor(db, SPGT, NULL, 0))) {
    fprintf(stderr, "Failed to create Sophia cursor.\n");
    fprintf(stderr, "%s\n", sp_error(db));
    return;
  }

  while (sp_fetch(cursor)) {
    if (sp_key(cursor)) {
      count++;
    }
  }

  sp_destroy(cursor);
  printf("%d\n", count);
}

/**
 * List command.
 */

static void
on_list(void *data) {
  void *cursor = NULL;

  // TODO support starting at a specific key
  // TODO support iterating in reverse
  if (!(cursor = sp_cursor(db, SPGT, NULL, 0))) {
    fprintf(stderr, "Failed to create Sophia cursor.\n");
    fprintf(stderr, "%s\n", sp_error(db));
    return;
  }

  while (sp_fetch(cursor)) {
    char *key = sp_key(cursor);
    size_t keysize = sp_keysize(cursor) + 1;
    char *value = sp_value(cursor);
    printf("  %s => %s\n", key, value);
  }

  sp_destroy(cursor);
}

/**
 * Setup autocomplete for commands.
 */

void
auto_complete(const char *buf, linenoiseCompletions *lc) {
  switch (buf[0]) {
    case 'h':
      linenoiseAddCompletion(lc, "help");
      break;
    case 'g':
      linenoiseAddCompletion(lc, "get");
      break;
    case 'l':
      linenoiseAddCompletion(lc, "list");
      break;
    case 'q':
    case 'e':
      linenoiseAddCompletion(lc, "exit");
      break;
    case 'c':
      linenoiseAddCompletion(lc, "count");
      break;
  }
}

/**
 * Entry point.
 */

int
main(int argc, char **argv) {
  char *line = NULL;
  emitter_t *emitter = NULL;
  int rc = 0;

  char *database_path = ".";
  if (2 == argc) database_path = argv[1];

  // setup sophia
  if (!(env = sp_env())) {
    fprintf(stderr, "Failed to create Sophia environment.\n");
    goto error;
  }

  // TODO readonly
  if (-1 == sp_ctl(env, SPDIR, SPO_CREAT|SPO_RDWR, database_path)) {
    fprintf(stderr, "Failed to configurate Sophia environment.\n");
    fprintf(stderr, "%s\n", sp_error(env));
    goto error;
  }

  if (-1 == sp_ctl(env, SPGC, 1)) {
    fprintf(stderr, "Failed to setup Sophia garbage collector.\n");
    fprintf(stderr, "%s\n", sp_error(env));
    goto error;
  }

  if (!(db = sp_open(env))) {
    fprintf(stderr, "%s\n", sp_error(env));
    goto error;
  }

  // create emitter
  if (!(emitter = emitter_new())) goto error;

  // register callbacks (commands)
  if (-1 == emitter_on(emitter, "help", &on_help)) goto error;
  if (-1 == emitter_on(emitter, "get", &on_get)) goto error;
  if (-1 == emitter_on(emitter, "exit", &on_exit)) goto error;
  if (-1 == emitter_on(emitter, "count", &on_count)) goto error;
  if (-1 == emitter_on(emitter, "list", &on_list)) goto error;

  // setup autocomplete
  linenoiseSetCompletionCallback(auto_complete);
  // load history
  linenoiseHistoryLoad(history_file);

  // repl
  while ((line = linenoise("sophia> "))) {
    // empty line: noop
    if ('\0' == line[0]) continue;

    if (0 == strncmp("help", line, 4)) {
      emitter_emit(emitter, "help", NULL);
    } else if (0 == strncmp("get", line, 3)) {
      emitter_emit(emitter, "get", line);
    } else if (0 == strncmp("exit", line, 4)) {
      emitter_emit(emitter, "exit", NULL);
    } else if (0 == strncmp("list", line, 4)) {
      emitter_emit(emitter, "list", NULL);
    } else if (0 == strncmp("count", line, 5)) {
      emitter_emit(emitter, "count", NULL);
    }

    // don't save exits
    if (!quit) {
      linenoiseHistoryAdd(line);
      linenoiseHistorySave(history_file);
    }

    free(line);
    if (quit) {
      printf("Goodbye :)\n");
      break;
    }
  }

error:
  rc = 1;

done:
  if (env) sp_destroy(env);
  if (db) sp_destroy(db);
  if (emitter) emitter_destroy(emitter);
  return rc;
}
