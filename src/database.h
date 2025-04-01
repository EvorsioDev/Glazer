#pragma once
#include "arena.h"
#include "collections.h"
#include <stdbool.h>
#include <stdlib.h>

typedef struct {
  const char *user;
  const char *password;
} db_creds_t;

typedef struct {
  const char *id;
  bool primaryKey;
} db_attribute_t;

typedef struct {
  const char *name;
  db_attribute_t *attributes;
  size_t attributeN;
  stringvector_t foreign_tables;
} db_tbl_t;

typedef int(db_fetcher_func)(const char *uri, const db_creds_t *creds,
                             Arena *arena, db_tbl_t **tbl, int *tblC);

typedef struct {
  db_fetcher_func *fetcher;
  const char *typeName;
} db_handle_t;

void db_register_fetcher(db_handle_t *handle);

db_handle_t *db_match_type(const char *type);
