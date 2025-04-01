#include "database.h"
#include "generator.h"
#include "log.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

// glazer -t sqlite -u user -p password filename.db
void print_usage(const char *prog_name) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr,
          "  %s -t TYPE [-o OUTPUT] [-u USER -p PASSWORD] DATABASE_URI\n",
          prog_name);
  fprintf(stderr, "\nOptions:\n");
  fprintf(stderr, "  -t TYPE       Type of the database (required).\n");
  fprintf(stderr,
          "  -o OUTPUT     Output file (optional, defaults to stdout).\n");
  fprintf(stderr, "  -u USER       Database user (optional, required for "
                  "databases that need credentials).\n");
  fprintf(stderr, "  -p PASSWORD   Password for the database user (optional, "
                  "required for databases that need credentials).\n");
  fprintf(stderr, "\nArguments:\n");
  fprintf(stderr, "  DATABASE_URI  URI of the database (required).\n");
}

int main(int argc, char *argv[]) {
  char *db_type = NULL;
  char *output_file = NULL;
  char *user = NULL;
  char *password = NULL;
  char *database_uri = NULL;

  int opt;
  while ((opt = getopt(argc, argv, "t:o:u:p:")) != -1) {
    switch (opt) {
    case 't':
      db_type = optarg;
      break;
    case 'o':
      output_file = optarg;
      break;
    case 'u':
      user = optarg;
      break;
    case 'p':
      password = optarg;
      break;
    default:
      print_usage(argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if (!db_type) {
    log_error("Database type (-t) is required");
    print_usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  if (optind < argc) {
    database_uri = argv[optind];
  } else {
    log_error("Database URI is required");
    print_usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  db_handle_t *handle = db_match_type(db_type);
  if (handle == 0) {
    log_error("DB type %s was not found", db_type);
    return EXIT_FAILURE;
  }

  Arena a = {0};
  db_creds_t credentials = {0};
  int tblC;
  db_tbl_t *db_tables;
  int status =
      handle->fetcher(database_uri, &credentials, &a, &db_tables, &tblC);

  if (status != 0) {
    arena_free(&a);
    return EXIT_FAILURE;
  }

  FILE *out = fopen(output_file, "w");
  generator_gen2f(out, db_tables, tblC);
  fclose(out);

  arena_free(&a);

  return EXIT_SUCCESS;
}
