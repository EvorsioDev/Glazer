#include "generator.h"
#include "collections.h"
#include "log.h"
#include "zhash.h"
#include <string.h>

void generator_gen2f(FILE *out, db_tbl_t *table, int tblC) {
  Arena arena = {0};

  struct ZHashTable *table_names = zcreate_hash_table(&arena);
  intvector_t *adjlists = arena_calloc(&arena, tblC, sizeof(intvector_t));
  for (int i = 0; i < tblC; i++) {
    intvector_init(&adjlists[i], &arena);
    zhash_set(table_names, table[i].name, i);
  }

  for (int i = 0; i < tblC; i++) {
    for (int j = 0; j < table[i].foreign_tables.N;j++) {
      intvector_push(&adjlists[i], zhash_get(table_names, table[i].foreign_tables.array[j]));
    }
  }

  for (int i = 0; i < tblC; i++) {
    fprintf(out, "entity %s \"%s\"\n", table[i].name, table[i].name);
  }

  for (int i = 0; i < tblC; i++) {
    for (int j = 0; j < adjlists[i].N; j++) {
      long v2 = adjlists[i].array[j];
      fprintf(out, "link %s and %s\n", table[i].name, table[v2].name);
    }
  }

  arena_free(&arena);
}
