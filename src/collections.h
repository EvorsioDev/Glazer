#pragma once

#include "dynarray.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  long long *array;
  size_t N;
  size_t cap;
  Arena *arena;
} intvector_t;

#define VECTOREND(vector) ((vector).array[(vector).N - 1])

void intvector_init(intvector_t *vec, Arena *a);

void intvector_push(intvector_t *vec, long long el);

long long intvector_popback(intvector_t *vec);

void intvector_clear(intvector_t *vec);

typedef struct {
  const char **array;
  size_t N;
  size_t cap;
  Arena *arena;
} stringvector_t;

void stringvector_init(stringvector_t *vec, Arena *a);

void stringvector_push(stringvector_t *vec, const char* el);

const char* stringvector_popback(stringvector_t *vec);

void stringvector_clear(stringvector_t *vec);

typedef struct bitset_t {
  uint64_t *bitset;
  size_t blocks;
} bitset_t;

void bitset_init(bitset_t *set, size_t N, Arena *arena);

void bitset_add(bitset_t *set, uint32_t val);

bool bitset_has(bitset_t *set, uint32_t val);

void bitset_remove(bitset_t *set, uint32_t val);
