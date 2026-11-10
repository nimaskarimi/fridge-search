#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "recipe/index.h"

namespace recipe {

// Builds a finalized index of fake recipes whose ingredient popularity follows
// a Zipf distribution (a few ingredients are everywhere, most are rare), like
// real recipe data. Ingredient 0 is "salt" and 1 is "water" so staples get
// exercised. Used by tests and the benchmark when no real dataset is given.
RecipeIndex make_synthetic_index(size_t num_recipes, size_t vocab_size, uint64_t seed);

// A realistic pantry: part of a random recipe's ingredients plus a few
// popular extras (picked by sampling ingredients from random recipes, which
// is proportional to how common each ingredient is).
std::vector<IngredientId> random_pantry(const RecipeIndex& index, std::mt19937_64& rng);

}  // namespace recipe
