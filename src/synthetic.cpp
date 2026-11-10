#include "recipe/synthetic.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_set>

namespace recipe {

RecipeIndex make_synthetic_index(size_t num_recipes, size_t vocab_size, uint64_t seed) {
  std::mt19937_64 rng(seed);

  std::vector<std::string> vocab;
  vocab.reserve(vocab_size);
  vocab.push_back("salt");
  vocab.push_back("water");
  // Letters only ("ing ab", "ing ac", ...): the normalizer drops words with
  // digits, so numeric names would all collapse into one ingredient.
  for (size_t i = vocab.size(); i < vocab_size; ++i) {
    std::string word;
    for (size_t n = i; ; n /= 26) {
      word.insert(word.begin(), static_cast<char>('a' + n % 26));
      if (n < 26) break;
    }
    vocab.push_back("ing " + word + "q");  // trailing q keeps singularize() from changing it
  }

  // Zipf CDF: P(i) proportional to 1 / (i + 1).
  std::vector<double> cdf(vocab_size);
  double sum = 0.0;
  for (size_t i = 0; i < vocab_size; ++i) {
    sum += 1.0 / static_cast<double>(i + 1);
    cdf[i] = sum;
  }
  std::uniform_real_distribution<double> uniform(0.0, sum);
  std::uniform_int_distribution<int> recipe_size(4, 14);
  std::uniform_int_distribution<int> stars(0, 10);

  RecipeIndex index;
  for (size_t r = 0; r < num_recipes; ++r) {
    const size_t want = std::min<size_t>(static_cast<size_t>(recipe_size(rng)), vocab_size);
    std::unordered_set<size_t> chosen;
    while (chosen.size() < want) {
      chosen.insert(static_cast<size_t>(
          std::lower_bound(cdf.begin(), cdf.end(), uniform(rng)) - cdf.begin()));
    }
    std::vector<std::string> ingredients;
    for (size_t i : chosen) ingredients.push_back(vocab[std::min(i, vocab_size - 1)]);
    index.add_recipe("Recipe " + std::to_string(r), ingredients,
                     static_cast<float>(stars(rng)) / 2.0f);
  }
  index.finalize();
  return index;
}

std::vector<IngredientId> random_pantry(const RecipeIndex& index, std::mt19937_64& rng) {
  std::uniform_int_distribution<size_t> pick_recipe(0, index.num_recipes() - 1);
  std::vector<IngredientId> pantry;

  // Most of one recipe, so the query has a realistic "almost there" match.
  std::vector<IngredientId> base = index.recipe(static_cast<RecipeId>(pick_recipe(rng))).ingredients;
  std::shuffle(base.begin(), base.end(), rng);
  base.resize(std::min(base.size(), base.size() / 2 + 1));
  pantry = base;

  // Plus 2-6 extras weighted by popularity.
  std::uniform_int_distribution<int> extras(2, 6);
  for (int e = extras(rng); e > 0; --e) {
    const auto& ings = index.recipe(static_cast<RecipeId>(pick_recipe(rng))).ingredients;
    if (ings.empty()) continue;
    std::uniform_int_distribution<size_t> pick(0, ings.size() - 1);
    pantry.push_back(ings[pick(rng)]);
  }
  return pantry;
}

}  // namespace recipe
