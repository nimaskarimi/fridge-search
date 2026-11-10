// Plain C interface to the C++ engine, so Swift (or any language with a C
// FFI) can call it. Swift imports this header through the app's bridging
// header; no C++ interop mode is needed.
//
// Ownership rules:
//   - fridge_engine_create() returns an engine you must free with
//     fridge_engine_destroy().
//   - fridge_search() returns results you must free with
//     fridge_results_destroy(). Strings read from results stay valid until
//     the results are destroyed, so copy them before that.
//   - Strings from fridge_engine_suggest() stay valid while the engine lives.
//
// An engine is not thread-safe: call it from one thread at a time.

#ifndef FRIDGE_BRIDGE_H
#define FRIDGE_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FridgeEngine FridgeEngine;
typedef struct FridgeResults FridgeResults;

// Loads a recipes TSV file. On failure returns NULL and writes a message
// into error (if error is non-NULL and error_capacity > 0).
FridgeEngine* fridge_engine_create(const char* tsv_path, char* error, size_t error_capacity);
void fridge_engine_destroy(FridgeEngine* engine);

size_t fridge_engine_recipe_count(const FridgeEngine* engine);
size_t fridge_engine_ingredient_count(const FridgeEngine* engine);

// Autocomplete: up to max_results ingredient names where some word starts
// with prefix, most common ingredients first. Returns how many were written.
size_t fridge_engine_suggest(const FridgeEngine* engine, const char* prefix,
                             const char** out_names, size_t max_results);

// Searches with pantry_count ingredient strings. max_missing = UINT32_MAX
// means no limit. Returns NULL only on internal error.
FridgeResults* fridge_search(FridgeEngine* engine, const char* const* pantry, size_t pantry_count,
                             size_t top_k, uint32_t max_missing, bool assume_staples);

// Fetches specific recipes by id (e.g. saved ones), in the order given, with
// have/missing computed against the pantry. Ids that don't exist are skipped.
// Use the same result accessors as for fridge_search().
FridgeResults* fridge_lookup(FridgeEngine* engine, const uint32_t* ids, size_t id_count,
                             const char* const* pantry, size_t pantry_count, bool assume_staples);

void fridge_results_destroy(FridgeResults* results);

double fridge_results_search_micros(const FridgeResults* results);
size_t fridge_results_unrecognized_count(const FridgeResults* results);
const char* fridge_results_unrecognized(const FridgeResults* results, size_t index);

// Accessors for result i (0 = best). Out-of-range indexes return 0 or "".
size_t fridge_results_count(const FridgeResults* results);
uint32_t fridge_result_id(const FridgeResults* results, size_t i);
const char* fridge_result_name(const FridgeResults* results, size_t i);
float fridge_result_score(const FridgeResults* results, size_t i);
float fridge_result_coverage(const FridgeResults* results, size_t i);
float fridge_result_rating(const FridgeResults* results, size_t i);
uint32_t fridge_result_have(const FridgeResults* results, size_t i);
uint32_t fridge_result_total(const FridgeResults* results, size_t i);

// Every ingredient of result i, with whether the pantry covers it.
size_t fridge_result_ingredient_count(const FridgeResults* results, size_t i);
const char* fridge_result_ingredient(const FridgeResults* results, size_t i, size_t j);
bool fridge_result_ingredient_have(const FridgeResults* results, size_t i, size_t j);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // FRIDGE_BRIDGE_H
