#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace recipe {

using IngredientId = uint32_t;
using RecipeId = uint32_t;

struct Recipe {
  std::string name;
  float rating = 0.0f;                    // 0-5; 0 means unrated
  std::vector<IngredientId> ingredients;  // sorted, unique
};

// Inverted index from ingredient -> recipes that use it.
//
// Usage:
//   RecipeIndex index;
//   index.add_recipe("Pancakes", {"flour", "eggs", "milk"}, 4.5f);
//   ...
//   index.finalize();   // computes weights; required before searching
//
// Each ingredient gets an IDF weight: log(1 + N / df). Rare ingredients
// ("saffron") weigh more than common ones ("butter"), so missing a rare
// ingredient hurts a recipe's score more than missing a common one.
class RecipeIndex {
 public:
  RecipeIndex();

  // Ingredient strings are normalized before interning. Returns the new id.
  RecipeId add_recipe(std::string name, const std::vector<std::string>& ingredients,
                      float rating = 0.0f);

  // Staples are ingredients every kitchen is assumed to have (salt, water...).
  // Must be set before finalize(). Defaults: salt, water, pepper, black pepper,
  // salt and pepper (a common single ingredient in Food.com data).
  void set_staples(const std::vector<std::string>& staples);

  void finalize();
  bool finalized() const { return finalized_; }

  // Loads a TSV file with lines: name<TAB>rating<TAB>ing1|ing2|ing3
  // Blank lines and lines starting with '#' are skipped. Returns a finalized
  // index. Throws std::runtime_error on I/O or format errors.
  static RecipeIndex load_tsv(const std::string& path);

  std::optional<IngredientId> lookup(std::string_view ingredient) const;  // normalizes

  // What a user most likely means by `ingredient`. An exact match wins;
  // otherwise every ingredient containing all of the typed words, so "flour"
  // finds "all purpose flour" and "vanilla" finds "vanilla extract".
  // Returns sorted ids; empty if nothing matches.
  std::vector<IngredientId> expand(std::string_view ingredient) const;

  size_t num_recipes() const { return recipes_.size(); }
  size_t num_ingredients() const { return names_.size(); }
  const Recipe& recipe(RecipeId id) const { return recipes_[id]; }
  const std::string& ingredient_name(IngredientId id) const { return names_[id]; }
  const std::vector<RecipeId>& postings(IngredientId id) const { return postings_[id]; }

  float weight(IngredientId id) const { return weights_[id]; }
  bool is_staple(IngredientId id) const { return is_staple_[id]; }
  float total_weight(RecipeId id) const { return total_weight_[id]; }
  float staple_weight(RecipeId id) const { return staple_weight_[id]; }
  uint32_t staple_count(RecipeId id) const { return staple_count_[id]; }

 private:
  IngredientId intern(const std::string& normalized);

  std::vector<Recipe> recipes_;
  std::vector<std::string> names_;                       // id -> name
  std::unordered_map<std::string, IngredientId> ids_;    // name -> id
  std::vector<std::vector<RecipeId>> postings_;          // id -> sorted recipe ids
  std::unordered_map<std::string, std::vector<IngredientId>> word_postings_;  // word -> ids
  std::vector<std::string> staple_names_;

  // Computed by finalize().
  std::vector<float> weights_;
  std::vector<bool> is_staple_;
  std::vector<float> total_weight_;
  std::vector<float> staple_weight_;
  std::vector<uint32_t> staple_count_;
  bool finalized_ = false;
};

}  // namespace recipe
