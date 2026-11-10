#include "recipe/search.h"

#include <algorithm>
#include <queue>
#include <stdexcept>

namespace recipe {
namespace {

// Higher score first; ties broken by lower recipe id so results are stable.
template <typename T>
bool better(const T& a, const T& b) {
  if (a.score != b.score) return a.score > b.score;
  return a.id < b.id;
}

}  // namespace

Searcher::Searcher(const RecipeIndex& index)
    : index_(index),
      have_count_(index.num_recipes(), 0),
      have_weight_(index.num_recipes(), 0.0f) {
  if (!index.finalized()) throw std::logic_error("Searcher requires a finalized RecipeIndex");
}

ParsedPantry Searcher::parse(const std::vector<std::string>& pantry) const {
  ParsedPantry parsed;
  for (const std::string& item : pantry) {
    std::vector<IngredientId> ids = index_.expand(item);
    if (!ids.empty()) {
      parsed.known.insert(parsed.known.end(), ids.begin(), ids.end());
    } else if (!item.empty()) {
      parsed.unrecognized.push_back(item);
    }
  }
  return parsed;
}

std::vector<Result> Searcher::search(const std::vector<std::string>& pantry,
                                     const SearchOptions& options) {
  return search(parse(pantry).known, options);
}

// Sorts and dedupes the pantry. When staples are assumed, they are removed
// from the pantry because score() credits them separately; this also keeps
// huge posting lists like "salt" out of the candidate scan.
void Searcher::prepare(std::vector<IngredientId>& pantry, const SearchOptions& options) const {
  std::sort(pantry.begin(), pantry.end());
  pantry.erase(std::unique(pantry.begin(), pantry.end()), pantry.end());
  if (options.assume_staples) {
    pantry.erase(std::remove_if(pantry.begin(), pantry.end(),
                                [&](IngredientId i) { return index_.is_staple(i); }),
                 pantry.end());
  }
}

Searcher::Scored Searcher::score(RecipeId id, uint32_t have, float have_weight,
                                 const SearchOptions& options) const {
  if (options.assume_staples) {
    have += index_.staple_count(id);
    have_weight += index_.staple_weight(id);
  }
  const Recipe& r = index_.recipe(id);
  const uint32_t total = static_cast<uint32_t>(r.ingredients.size());
  const uint32_t missing = total - have;
  const float total_weight = index_.total_weight(id);
  const float coverage = total_weight > 0.0f ? have_weight / total_weight : 0.0f;
  const float s = coverage - options.missing_penalty * static_cast<float>(missing) +
                  options.rating_weight * (r.rating / 5.0f);
  return {id, s, coverage, have, missing};
}

std::vector<Result> Searcher::search(std::vector<IngredientId> pantry,
                                     const SearchOptions& options) {
  prepare(pantry, options);
  if (options.top_k == 0) return {};

  // 1. Accumulate matches by walking each pantry ingredient's posting list.
  //    Pantry ids are ascending, so each recipe's weight is summed in the
  //    same order as in search_brute_force(), giving bit-identical floats.
  touched_.clear();
  for (IngredientId ing : pantry) {
    const float w = index_.weight(ing);
    for (RecipeId r : index_.postings(ing)) {
      if (have_count_[r] == 0) touched_.push_back(r);
      ++have_count_[r];
      have_weight_[r] += w;
    }
  }

  // 2. Score only the touched recipes; keep the best k in a min-heap whose
  //    top is the worst result kept so far.
  auto worse_on_top = [](const Scored& a, const Scored& b) { return better(a, b); };
  std::priority_queue<Scored, std::vector<Scored>, decltype(worse_on_top)> heap(worse_on_top);
  for (RecipeId r : touched_) {
    const Scored s = score(r, have_count_[r], have_weight_[r], options);
    have_count_[r] = 0;
    have_weight_[r] = 0.0f;
    if (s.missing > options.max_missing) continue;
    if (heap.size() < options.top_k) {
      heap.push(s);
    } else if (better(s, heap.top())) {
      heap.pop();
      heap.push(s);
    }
  }

  std::vector<Scored> best;
  best.reserve(heap.size());
  while (!heap.empty()) {
    best.push_back(heap.top());
    heap.pop();
  }
  return finish(std::move(best), pantry, options);
}

std::vector<Result> Searcher::search_brute_force(std::vector<IngredientId> pantry,
                                                 const SearchOptions& options) const {
  prepare(pantry, options);
  std::vector<Scored> all;
  for (RecipeId r = 0; r < index_.num_recipes(); ++r) {
    uint32_t have = 0;
    float have_weight = 0.0f;
    for (IngredientId ing : index_.recipe(r).ingredients) {
      if (std::binary_search(pantry.begin(), pantry.end(), ing)) {
        ++have;
        have_weight += index_.weight(ing);
      }
    }
    if (have == 0) continue;  // same candidate rule as the indexed search
    const Scored s = score(r, have, have_weight, options);
    if (s.missing <= options.max_missing) all.push_back(s);
  }
  std::sort(all.begin(), all.end(), [](const Scored& a, const Scored& b) { return better(a, b); });
  if (all.size() > options.top_k) all.resize(options.top_k);
  return finish(std::move(all), pantry, options);
}

// Sorts best-first and fills in the missing-ingredient list, which is only
// worth computing for the handful of results actually returned.
std::vector<Result> Searcher::finish(std::vector<Scored> best,
                                     const std::vector<IngredientId>& pantry,
                                     const SearchOptions& options) const {
  std::sort(best.begin(), best.end(), [](const Scored& a, const Scored& b) { return better(a, b); });
  std::vector<Result> results;
  results.reserve(best.size());
  for (const Scored& s : best) {
    Result res{s.id, s.score, s.coverage, s.have, s.missing, {}};
    for (IngredientId ing : index_.recipe(s.id).ingredients) {
      if (options.assume_staples && index_.is_staple(ing)) continue;
      if (!std::binary_search(pantry.begin(), pantry.end(), ing)) {
        res.missing_ingredients.push_back(ing);
      }
    }
    results.push_back(std::move(res));
  }
  return results;
}

}  // namespace recipe
