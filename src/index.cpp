#include "recipe/index.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>

#include "recipe/normalize.h"

namespace recipe {

RecipeIndex::RecipeIndex() {
  set_staples({"salt", "water", "pepper", "black pepper", "salt and pepper"});
}

RecipeId RecipeIndex::add_recipe(std::string name, const std::vector<std::string>& ingredients,
                                 float rating) {
  if (finalized_) throw std::logic_error("add_recipe() called after finalize()");

  const RecipeId id = static_cast<RecipeId>(recipes_.size());
  Recipe r;
  r.name = std::move(name);
  r.rating = rating;
  for (const std::string& raw : ingredients) {
    std::string key = normalize_ingredient(raw);
    if (!key.empty()) r.ingredients.push_back(intern(key));
  }
  std::sort(r.ingredients.begin(), r.ingredients.end());
  r.ingredients.erase(std::unique(r.ingredients.begin(), r.ingredients.end()),
                      r.ingredients.end());

  // Recipe ids increase monotonically, so every posting list stays sorted.
  for (IngredientId ing : r.ingredients) postings_[ing].push_back(id);
  recipes_.push_back(std::move(r));
  return id;
}

void RecipeIndex::set_staples(const std::vector<std::string>& staples) {
  if (finalized_) throw std::logic_error("set_staples() called after finalize()");
  staple_names_.clear();
  for (const std::string& s : staples) {
    std::string key = normalize_ingredient(s);
    if (!key.empty()) staple_names_.push_back(key);
  }
}

void RecipeIndex::finalize() {
  const double n = static_cast<double>(recipes_.size());

  weights_.assign(names_.size(), 0.0f);
  for (IngredientId i = 0; i < names_.size(); ++i) {
    const double df = static_cast<double>(postings_[i].size());
    weights_[i] = static_cast<float>(std::log(1.0 + n / std::max(df, 1.0)));
  }

  is_staple_.assign(names_.size(), false);
  for (const std::string& s : staple_names_) {
    if (auto it = ids_.find(s); it != ids_.end()) is_staple_[it->second] = true;
  }

  total_weight_.assign(recipes_.size(), 0.0f);
  staple_weight_.assign(recipes_.size(), 0.0f);
  staple_count_.assign(recipes_.size(), 0);
  for (RecipeId r = 0; r < recipes_.size(); ++r) {
    for (IngredientId ing : recipes_[r].ingredients) {
      total_weight_[r] += weights_[ing];
      if (is_staple_[ing]) {
        staple_weight_[r] += weights_[ing];
        ++staple_count_[r];
      }
    }
  }
  finalized_ = true;
}

RecipeIndex RecipeIndex::load_tsv(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open " + path);

  RecipeIndex index;
  std::string line;
  size_t line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;

    const size_t t1 = line.find('\t');
    const size_t t2 = t1 == std::string::npos ? t1 : line.find('\t', t1 + 1);
    if (t2 == std::string::npos) {
      throw std::runtime_error(path + ":" + std::to_string(line_no) +
                               ": expected name<TAB>rating<TAB>ingredients");
    }
    float rating = 0.0f;
    try {
      rating = std::stof(line.substr(t1 + 1, t2 - t1 - 1));
    } catch (const std::exception&) {
      throw std::runtime_error(path + ":" + std::to_string(line_no) + ": bad rating");
    }
    index.add_recipe(line.substr(0, t1), split(std::string_view(line).substr(t2 + 1), '|'),
                     rating);
  }
  index.finalize();
  return index;
}

std::optional<IngredientId> RecipeIndex::lookup(std::string_view ingredient) const {
  auto it = ids_.find(normalize_ingredient(ingredient));
  if (it == ids_.end()) return std::nullopt;
  return it->second;
}

IngredientId RecipeIndex::intern(const std::string& normalized) {
  if (auto it = ids_.find(normalized); it != ids_.end()) return it->second;
  const IngredientId id = static_cast<IngredientId>(names_.size());
  names_.push_back(normalized);
  ids_.emplace(normalized, id);
  postings_.emplace_back();
  // Ids are assigned in increasing order, so word posting lists stay sorted.
  std::vector<std::string> words = split(normalized, ' ');
  std::sort(words.begin(), words.end());
  words.erase(std::unique(words.begin(), words.end()), words.end());
  for (const std::string& w : words) word_postings_[w].push_back(id);
  return id;
}

std::vector<IngredientId> RecipeIndex::expand(std::string_view ingredient) const {
  const std::string key = normalize_ingredient(ingredient);
  if (key.empty()) return {};
  if (auto it = ids_.find(key); it != ids_.end()) return {it->second};

  // Intersect the word posting lists of every word in the query.
  std::vector<IngredientId> result;
  bool first = true;
  for (const std::string& w : split(key, ' ')) {
    auto it = word_postings_.find(w);
    if (it == word_postings_.end()) return {};
    if (first) {
      result = it->second;
      first = false;
    } else {
      std::vector<IngredientId> merged;
      std::set_intersection(result.begin(), result.end(), it->second.begin(), it->second.end(),
                            std::back_inserter(merged));
      result = std::move(merged);
    }
    if (result.empty()) return {};
  }
  return result;
}

}  // namespace recipe
