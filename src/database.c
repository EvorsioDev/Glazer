#include "database.h"

#include "dynarray.h"
#include "log.h"
#include <string.h>

DEFINE_VECTOR(db_handles, db_handle_t);

static Arena db_handles_arena = {0};
static bool list_initialized = false;
static db_handles_t fetchers;

void db_register_fetcher(db_handle_t *handle) {
  if (!list_initialized) {
    db_handles_init(&fetchers, &db_handles_arena);
    list_initialized = true;
  }
  if (handle == 0) {
    log_fatal("Failed to register handle: handle is null");
    abort();
  }
  db_handles_push(&fetchers, *handle);
  log_debug("Database type %s was successfully registered!", handle->typeName);
}

db_handle_t *db_match_type(const char *type) {
  for (int i = 0; i < (long)fetchers.N; i++) {
    if (strcmp(type, fetchers.array[i].typeName) == 0) {
      return fetchers.array + i;
    }
  }
  return 0;
}

static __attribute__((destructor)) void unregister_all(void) {
  if (list_initialized)
    arena_free(&db_handles_arena);
}
