
//
// repl.c - A Sophia REPL
//
// Copyright (c) 2014 Stephen Mathieson <me@stephenmathieson.com>
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sophia.h>
#include "linenoise/linenoise.h"
#include "substr/substr.h"
#include "emitter/emitter.h"

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
 * Emitter.
 */

static emitter_t *emitter = NULL;

/**
 * Database path.
 */

static char *database_path = ".";

/**
 * Pre-declair command callbacks.
 */

static void on_help(void *);
static void on_exit(void *);
static void on_get(void *);
static void on_count(void *);
static void on_list(void *);


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

static int
setup(void) {
  int rc = -1;

  // setup sophia
  if (!(env = sp_env())) {
    fprintf(stderr, "Failed to create Sophia environment.\n");
    goto done;
  }

  if (-1 == sp_ctl(env, SPDIR, SPO_RDONLY, database_path)) {
    fprintf(stderr, "Failed to configurate Sophia environment.\n");
    fprintf(stderr, "%s\n", sp_error(env));
    goto done;
  }

  if (-1 == sp_ctl(env, SPGC, 1)) {
    fprintf(stderr, "Failed to setup Sophia garbage collector.\n");
    fprintf(stderr, "%s\n", sp_error(env));
    goto done;
  }

  if (!(db = sp_open(env))) {
    fprintf(stderr, "%s\n", sp_error(env));
    goto done;
  }

  // create emitter
  if (!(emitter = emitter_new())) goto done;

  // register callbacks (commands)
  if (-1 == emitter_on(emitter, "help", &on_help)) goto done;
  if (-1 == emitter_on(emitter, "get", &on_get)) goto done;
  if (-1 == emitter_on(emitter, "exit", &on_exit)) goto done;
  if (-1 == emitter_on(emitter, "count", &on_count)) goto done;
  if (-1 == emitter_on(emitter, "list", &on_list)) goto done;

  rc = 0;

done:
  return rc;
}

/**
 * Entry point.
 */

int
main(int argc, char **argv) {
  char *line = NULL;
  int rc = 1;

  if (2 == argc) database_path = argv[1];
  if (-1 == setup()) goto done;

  // setup autocomplete
  linenoiseSetCompletionCallback(auto_complete);
  // load history
  linenoiseHistoryLoad(history_file);

  // repl
  while ((line = linenoise("sophia> "))) {
    // empty line: noop
    if ('\0' == line[0]) goto next_line;

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
    } else {
      fprintf(stderr, "Unrecognized command '%s'\n", line);
    }

    // don't save exits
    if (!quit) {
      linenoiseHistoryAdd(line);
      linenoiseHistorySave(history_file);
    }

  next_line:
    free(line);
    if (quit) {
      printf("Goodbye :)\n");
      break;
    }
  }

  rc = 0;

done:
  if (env) sp_destroy(env);
  if (db) sp_destroy(db);
  if (emitter) emitter_destroy(emitter);
  return rc;
}


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
  printf("    count      Count all keys\n");
  printf("    exit       Close the database and exit\n");
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
  const char *value = NULL;
  void *ref = NULL;
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
    printf("%s => %s\n", key, value);
  } else {
    printf("%s => null\n", key);
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
    const char *key = sp_key(cursor);
    const char *value = sp_value(cursor);
    printf("%s => %s\n", key, value);
  }

  sp_destroy(cursor);
}
