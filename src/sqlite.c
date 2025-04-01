#include "collections.h"
#include "database.h"
#include "log.h"
#include <dlfcn.h>
#include <sqlite3.h>

static const char *SQLITE_BIN_NAME = "libsqlite3.so";
static void *handle;

static db_fetcher_func fetcher_func;

int (*sqlite3_open_v2_ptr)(const char *filename, sqlite3 **ppDb, int flags,
                           const char *zVfs);
const char *(*sqlite3_errmsg_ptr)(sqlite3 *db);
SQLITE_API int (*sqlite3_close_ptr)(sqlite3 *);

SQLITE_API int (*sqlite3_prepare_v2_ptr)(
    sqlite3 *db,           /* Database handle */
    const char *zSql,      /* SQL statement, UTF-8 encoded */
    int nByte,             /* Maximum length of zSql in bytes. */
    sqlite3_stmt **ppStmt, /* OUT: Statement handle */
    const char **pzTail    /* OUT: Pointer to unused portion of zSql */
);

SQLITE_API const unsigned char *(*sqlite3_column_text_ptr)(sqlite3_stmt *,
                                                           int iCol);

SQLITE_API int (*sqlite3_step_ptr)(sqlite3_stmt *);

SQLITE_API int (*sqlite3_finalize_ptr)(sqlite3_stmt *pStmt);

SQLITE_API int (*sqlite3_column_int_ptr)(sqlite3_stmt *, int iCol);

DEFINE_VECTOR(attribute_list, db_attribute_t)

static void __attribute__((constructor)) register_sqlite_module(void) {
  handle = dlopen(SQLITE_BIN_NAME, RTLD_LAZY);
  if (!handle) {
    fprintf(stderr, "%s\n", dlerror());
    return;
  }

  *(void **)(&sqlite3_open_v2_ptr) = dlsym(handle, "sqlite3_open_v2");
  *(void **)(&sqlite3_errmsg_ptr) = dlsym(handle, "sqlite3_errmsg");
  *(void **)(&sqlite3_close_ptr) = dlsym(handle, "sqlite3_close");
  *(void **)(&sqlite3_prepare_v2_ptr) = dlsym(handle, "sqlite3_prepare_v2");
  *(void **)(&sqlite3_step_ptr) = dlsym(handle, "sqlite3_step");
  *(void **)(&sqlite3_column_text_ptr) = dlsym(handle, "sqlite3_column_text");
  *(void **)(&sqlite3_column_int_ptr) = dlsym(handle, "sqlite3_column_int");
  *(void **)(&sqlite3_finalize_ptr) = dlsym(handle, "sqlite3_finalize");

  db_handle_t db_handle = {fetcher_func, "sqlite"};
  db_register_fetcher(&db_handle);
}

static void __attribute__((destructor)) unload(void) { dlclose(handle); }

static int fetcher_func(const char *uri, const db_creds_t *creds, Arena *arena,
                        db_tbl_t **tbl, int *tblC) {
  if (creds->user || creds->password) {
    log_warn("Sqlite 3 does not support credentials");
    return -1;
  }

  sqlite3 *dbP;
  int status;
  sqlite3_stmt *res;
  Arena fetch_arena = {0};

  status =
      sqlite3_open_v2(uri, &dbP, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, 0);

  if (status != SQLITE_OK) {
    log_error("Error while fetching data: %s", sqlite3_errmsg(dbP));
    goto handle_error;
  }
  status = sqlite3_prepare_v2(
      dbP, "select name from sqlite_schema where type='table';", -1, &res, 0);

  if (status != SQLITE_OK) {
    log_error("Error while fetching data: %s", sqlite3_errmsg(dbP));
    goto handle_error;
  }

  stringvector_t tbl_names;
  stringvector_init(&tbl_names, &fetch_arena);

  status = sqlite3_step(res);
  while (status == SQLITE_ROW) {
    const unsigned char *tblName = sqlite3_column_text(res, 0);
    stringvector_push(&tbl_names, (char *)tblName);
    status = sqlite3_step(res);
  }

  status = sqlite3_finalize(res);

  *tbl = arena_calloc(arena, tbl_names.N, sizeof(db_tbl_t));
  *tblC = tbl_names.N;
  for (int i = 0; i < tbl_names.N; i++) {
    (*tbl)[i].attributeN = 0;
    (*tbl)[i].name = arena_strdup(arena, tbl_names.array[i]);
    stringvector_init(&(*tbl)[i].foreign_tables, arena);
  }

  for (int i = 0; i < *tblC; i++) {
    char buf[8 * BUFSIZ];
    sprintf(buf, "PRAGMA table_info(%s)", tbl_names.array[i]);
    status = sqlite3_prepare_v2(dbP, buf, -1, &res, 0);
    if (status != SQLITE_OK) {
      log_error("Error while fetching data: %s", sqlite3_errmsg(dbP));
      status = sqlite3_finalize(res);
      goto handle_error;
    }

    attribute_list_t al;
    attribute_list_init(&al, arena);

    status = sqlite3_step(res);
    while (status == SQLITE_ROW) {
      db_attribute_t attr = {0};
      const char *name = (char *)sqlite3_column_text(res, 1);
      attr.id = arena_strdup(arena, name);
      attr.primaryKey = sqlite3_column_int(res, 5);
      attribute_list_push(&al, attr);
      status = sqlite3_step(res);
    }

    (*tbl)[i].attributeN = al.N;
    (*tbl)[i].attributes = al.array;

    status = sqlite3_finalize(res);
    if (status != SQLITE_OK) {
      log_error("Error while fetching data: %s", sqlite3_errmsg(dbP));
      goto handle_error;
    }
  }

  for (int i = 0; i < *tblC; i++) {
    char buf[8 * BUFSIZ];
    sprintf(buf, "PRAGMA foreign_key_list(%s)", tbl_names.array[i]);
    status = sqlite3_prepare_v2(dbP, buf, -1, &res, 0);
    if (status != SQLITE_OK) {
      log_error("Error while fetching data: %s", sqlite3_errmsg(dbP));
      status = sqlite3_finalize(res);
      goto handle_error;
    }

    attribute_list_t al;
    attribute_list_init(&al, arena);

    status = sqlite3_step(res);
    while (status == SQLITE_ROW) {
      stringvector_push(&(*tbl)[i].foreign_tables,
                        arena_strdup(arena, sqlite3_column_text(res, 2)));
      status = sqlite3_step(res);
    }

    status = sqlite3_finalize(res);
    if (status != SQLITE_OK) {
      log_error("Error while fetching data: %s", sqlite3_errmsg(dbP));
      goto handle_error;
    }
  }

  if (status != SQLITE_OK) {
  handle_error:;
    sqlite3_close(dbP);
    arena_free(&fetch_arena);
    return 1;
  }

  sqlite3_close(dbP);
  arena_free(&fetch_arena);

  return 0;
}

SQLITE_API const char *sqlite3_errmsg(sqlite3 *db) {
  return sqlite3_errmsg_ptr(db);
}

SQLITE_API int sqlite3_open_v2(const char *filename, sqlite3 **ppDb, int flags,
                               const char *zVfs) {
  return sqlite3_open_v2_ptr(filename, ppDb, flags, zVfs);
}

SQLITE_API const unsigned char *sqlite3_column_text(sqlite3_stmt *stmt,
                                                    int iCol) {
  return sqlite3_column_text_ptr(stmt, iCol);
}

SQLITE_API int sqlite3_column_int(sqlite3_stmt *stmt, int iCol) {
  return sqlite3_column_int_ptr(stmt, iCol);
}

SQLITE_API int sqlite3_close(sqlite3 *db) { return sqlite3_close_ptr(db); }

SQLITE_API int sqlite3_prepare_v2(
    sqlite3 *db,           /* Database handle */
    const char *zSql,      /* SQL statement, UTF-8 encoded */
    int nByte,             /* Maximum length of zSql in bytes. */
    sqlite3_stmt **ppStmt, /* OUT: Statement handle */
    const char **pzTail    /* OUT: Pointer to unused portion of zSql */
) {
  return sqlite3_prepare_v2_ptr(db, zSql, nByte, ppStmt, pzTail);
}

SQLITE_API int sqlite3_step(sqlite3_stmt *stmt) {
  return sqlite3_step_ptr(stmt);
}

SQLITE_API int sqlite3_finalize(sqlite3_stmt *pStmt) {
  return sqlite3_finalize_ptr(pStmt);
}
