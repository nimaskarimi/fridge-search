/* Exercises the C bridge from plain C, the same way Swift sees it. Compiling
   this file as C proves the header has no C++ in it. */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fridge_bridge.h"

static int failures = 0;
#define CHECK(cond)                                                  \
  do {                                                               \
    if (!(cond)) {                                                   \
      printf("    FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
      ++failures;                                                    \
    }                                                                \
  } while (0)

int main(void) {
  char error[256];

  FridgeEngine* missing = fridge_engine_create("/nonexistent.tsv", error, sizeof error);
  CHECK(missing == NULL);
  CHECK(strlen(error) > 0);

  FridgeEngine* engine = fridge_engine_create(SAMPLE_DATA, error, sizeof error);
  CHECK(engine != NULL);
  if (!engine) {
    printf("could not load sample data: %s\n", error);
    return 1;
  }
  CHECK(fridge_engine_recipe_count(engine) == 30);

  const char* names[8];
  size_t n = fridge_engine_suggest(engine, "Flo", names, 8);
  CHECK(n >= 1);
  CHECK(n >= 1 && strstr(names[0], "flour") != NULL);
  CHECK(fridge_engine_suggest(engine, "", names, 8) == 0);
  CHECK(fridge_engine_suggest(engine, "zzz", names, 8) == 0);

  const char* pantry[] = {"eggs", "butter", "flour", "brown sugar", "vanilla", "baking soda", "flor"};
  FridgeResults* results = fridge_search(engine, pantry, 7, 5, UINT32_MAX, true);
  CHECK(results != NULL);
  CHECK(fridge_results_count(results) == 5);
  CHECK(strcmp(fridge_result_name(results, 0), "Brown Butter Cookies") == 0);
  CHECK(fridge_result_have(results, 0) == fridge_result_total(results, 0));
  CHECK(fridge_results_unrecognized_count(results) == 1);
  CHECK(strcmp(fridge_results_unrecognized(results, 0), "flor") == 0);

  /* Result 2 is missing something; missing ingredients are listed first. */
  size_t count = fridge_result_ingredient_count(results, 2);
  CHECK(count == fridge_result_total(results, 2));
  CHECK(!fridge_result_ingredient_have(results, 2, 0));
  CHECK(fridge_result_ingredient_have(results, 2, count - 1));

  /* Out-of-range access is safe. */
  CHECK(strcmp(fridge_result_name(results, 99), "") == 0);
  CHECK(fridge_result_ingredient_count(results, 99) == 0);
  fridge_results_destroy(results);

  /* Lookup by id returns the same numbers as search, in the order asked,
     and skips ids that don't exist. */
  results = fridge_search(engine, pantry, 7, 3, UINT32_MAX, true);
  uint32_t ids[3] = {fridge_result_id(results, 2), 999999, fridge_result_id(results, 0)};
  FridgeResults* looked = fridge_lookup(engine, ids, 3, pantry, 7, true);
  CHECK(fridge_results_count(looked) == 2);
  CHECK(fridge_result_id(looked, 0) == fridge_result_id(results, 2));
  CHECK(fridge_result_id(looked, 1) == fridge_result_id(results, 0));
  CHECK(fridge_result_have(looked, 0) == fridge_result_have(results, 2));
  CHECK(fridge_result_score(looked, 0) == fridge_result_score(results, 2));
  CHECK(fridge_result_coverage(looked, 1) == fridge_result_coverage(results, 0));
  fridge_results_destroy(looked);
  fridge_results_destroy(results);

  /* max_missing = 0 returns only recipes you can make now. */
  results = fridge_search(engine, pantry, 7, 10, 0, true);
  for (size_t i = 0; i < fridge_results_count(results); ++i) {
    CHECK(fridge_result_have(results, i) == fridge_result_total(results, i));
  }
  fridge_results_destroy(results);

  fridge_engine_destroy(engine);
  printf("bridge_test: %d failed checks\n", failures);
  return failures == 0 ? 0 : 1;
}
