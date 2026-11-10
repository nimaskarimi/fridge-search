#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "recipe/index.h"

namespace recipe {

struct SearchOptions {
  size_t top_k = 10;
  uint32_t max_missing = std::numeric_limits<uint32_t>::max();
  float missing_penalty = 0.04f;  // score lost per missing ingredient
  float rating_weight = 0.05f;    // bonus for a 5-star recipe
  bool assume_staples = true;     // treat salt, water, pepper as always on hand
};

struct Result {
  RecipeId id = 0;
  float score = 0.0f;
  float coverage = 0.0f;  // IDF-weighted fraction of the recipe you can cover, 0-1
  uint32_t have = 0;
  uint32_t missing = 0;
  std::vector<IngredientId> missing_ingredients;
};

struct ParsedPantry {
  std::vector<IngredientId> known;
  std::vector<std::string> unrecognized;
};

// Ranks recipes by how well a pantry covers them.
//
//   coverage = sum(weight of ingredients you have) / sum(weight of all ingredients)
//   score    = coverage - missing_penalty * missing + rating_weight * rating / 5
//
// search() only touches recipes that share at least one ingredient with the
// pantry (via the inverted index) and keeps the best k in a min-heap, so its
// cost grows with the posting lists of the pantry's ingredients, not with the
// size of the whole corpus. search_brute_force() scores every recipe; it is
// the reference implementation used by tests and the benchmark.
//
// Not thread-safe: a Searcher reuses scratch buffers between queries. Use one
// Searcher per thread; they can share one RecipeIndex.
class Searcher {
 public:
  explicit Searcher(const RecipeIndex& index);

  ParsedPantry parse(const std::vector<std::string>& pantry) const;

  std::vector<Result> search(const std::vector<std::string>& pantry,
                             const SearchOptions& options = {});
  std::vector<Result> search(std::vector<IngredientId> pantry, const SearchOptions& options = {});
  std::vector<Result> search_brute_force(std::vector<IngredientId> pantry,
                                         const SearchOptions& options = {}) const;

 private:
  struct Scored {
    RecipeId id;
    float score;
    float coverage;
    uint32_t have;
    uint32_t missing;
  };

  void prepare(std::vector<IngredientId>& pantry, const SearchOptions& options) const;
  Scored score(RecipeId id, uint32_t have, float have_weight, const SearchOptions& options) const;
  std::vector<Result> finish(std::vector<Scored> best, const std::vector<IngredientId>& pantry,
                             const SearchOptions& options) const;

  const RecipeIndex& index_;
  // Scratch accumulators indexed by RecipeId. Only entries listed in
  // touched_ are non-zero, and they are reset after every query.
  std::vector<uint32_t> have_count_;
  std::vector<float> have_weight_;
  std::vector<RecipeId> touched_;
};

}  // namespace recipe
