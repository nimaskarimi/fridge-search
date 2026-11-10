#include "fridge_bridge.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "recipe/index.h"
#include "recipe/search.h"

struct FridgeEngine {
  recipe::RecipeIndex index;
  std::unique_ptr<recipe::Searcher> searcher;   // refers to index; engine is never moved
  std::vector<recipe::IngredientId> by_popularity;  // for autocomplete ranking
};

namespace fridge_detail {

struct Ingredient {
  std::string name;
  bool have;
};

struct Item {
  uint32_t id;
  std::string name;
  float score, coverage, rating;
  uint32_t have, total;
  std::vector<Ingredient> ingredients;
};

}  // namespace fridge_detail

namespace {

using fridge_detail::Ingredient;
using fridge_detail::Item;

void write_error(char* error, size_t capacity, const std::string& message) {
  if (error == nullptr || capacity == 0) return;
  const size_t n = std::min(capacity - 1, message.size());
  std::memcpy(error, message.data(), n);
  error[n] = '\0';
}

std::vector<std::string> to_strings(const char* const* strings, size_t count) {
  std::vector<std::string> out;
  for (size_t i = 0; i < count && strings; ++i) {
    if (strings[i]) out.emplace_back(strings[i]);
  }
  return out;
}

// Builds one result: every ingredient with a have/missing flag (missing
// first), plus have count, IDF-weighted coverage, and score computed exactly
// as recipe::Searcher does. `known` must be sorted.
Item make_item(const recipe::RecipeIndex& index, recipe::RecipeId id,
               const std::vector<recipe::IngredientId>& known,
               const recipe::SearchOptions& options) {
  const recipe::Recipe& rec = index.recipe(id);
  Item item{id, rec.name, 0.0f, 0.0f, rec.rating, 0,
            static_cast<uint32_t>(rec.ingredients.size()), {}};
  float have_weight = 0.0f;
  for (recipe::IngredientId ing : rec.ingredients) {
    const bool have = (options.assume_staples && index.is_staple(ing)) ||
                      std::binary_search(known.begin(), known.end(), ing);
    if (have) {
      ++item.have;
      have_weight += index.weight(ing);
    }
    item.ingredients.push_back({index.ingredient_name(ing), have});
  }
  const float total_weight = index.total_weight(id);
  item.coverage = total_weight > 0.0f ? have_weight / total_weight : 0.0f;
  item.score = item.coverage -
               options.missing_penalty * static_cast<float>(item.total - item.have) +
               options.rating_weight * (rec.rating / 5.0f);
  std::stable_partition(item.ingredients.begin(), item.ingredients.end(),
                        [](const Ingredient& g) { return !g.have; });
  return item;
}

}  // namespace

struct FridgeResults {
  std::vector<fridge_detail::Item> items;
  std::vector<std::string> unrecognized;
  double micros = 0;
};

extern "C" {

FridgeEngine* fridge_engine_create(const char* tsv_path, char* error, size_t error_capacity) {
  write_error(error, error_capacity, "");
  if (tsv_path == nullptr) {
    write_error(error, error_capacity, "no path given");
    return nullptr;
  }
  try {
    auto engine = std::make_unique<FridgeEngine>();
    engine->index = recipe::RecipeIndex::load_tsv(tsv_path);
    engine->searcher = std::make_unique<recipe::Searcher>(engine->index);

    const recipe::RecipeIndex& index = engine->index;
    engine->by_popularity.resize(index.num_ingredients());
    for (recipe::IngredientId i = 0; i < index.num_ingredients(); ++i) engine->by_popularity[i] = i;
    std::stable_sort(engine->by_popularity.begin(), engine->by_popularity.end(),
                     [&](recipe::IngredientId a, recipe::IngredientId b) {
                       return index.postings(a).size() > index.postings(b).size();
                     });
    return engine.release();
  } catch (const std::exception& e) {
    write_error(error, error_capacity, e.what());
    return nullptr;
  }
}

void fridge_engine_destroy(FridgeEngine* engine) { delete engine; }

size_t fridge_engine_recipe_count(const FridgeEngine* engine) {
  return engine ? engine->index.num_recipes() : 0;
}

size_t fridge_engine_ingredient_count(const FridgeEngine* engine) {
  return engine ? engine->index.num_ingredients() : 0;
}

size_t fridge_engine_suggest(const FridgeEngine* engine, const char* prefix,
                             const char** out_names, size_t max_results) {
  if (!engine || !prefix || !out_names || max_results == 0) return 0;
  std::string p;
  for (const char* c = prefix; *c; ++c) {
    p.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*c))));
  }
  while (!p.empty() && p.back() == ' ') p.pop_back();
  if (p.empty()) return 0;

  size_t count = 0;
  for (recipe::IngredientId id : engine->by_popularity) {
    const std::string& name = engine->index.ingredient_name(id);
    // Match at the start of the name or the start of any later word.
    const bool match = name.compare(0, p.size(), p) == 0 || name.find(" " + p) != std::string::npos;
    if (!match) continue;
    out_names[count++] = name.c_str();
    if (count == max_results) break;
  }
  return count;
}

FridgeResults* fridge_search(FridgeEngine* engine, const char* const* pantry, size_t pantry_count,
                             size_t top_k, uint32_t max_missing, bool assume_staples) {
  if (!engine) return nullptr;
  try {
    const std::vector<std::string> items = to_strings(pantry, pantry_count);
    recipe::SearchOptions options;
    options.top_k = top_k;
    options.max_missing = max_missing;
    options.assume_staples = assume_staples;

    auto out = std::make_unique<FridgeResults>();
    const auto start = std::chrono::steady_clock::now();
    recipe::ParsedPantry parsed = engine->searcher->parse(items);
    const std::vector<recipe::Result> results = engine->searcher->search(parsed.known, options);
    out->micros =
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
    out->unrecognized = std::move(parsed.unrecognized);

    std::sort(parsed.known.begin(), parsed.known.end());
    for (const recipe::Result& r : results) {
      Item item = make_item(engine->index, r.id, parsed.known, options);
      item.score = r.score;  // identical by construction; the engine is the source of truth
      out->items.push_back(std::move(item));
    }
    return out.release();
  } catch (const std::exception&) {
    return nullptr;
  }
}

FridgeResults* fridge_lookup(FridgeEngine* engine, const uint32_t* ids, size_t id_count,
                             const char* const* pantry, size_t pantry_count, bool assume_staples) {
  if (!engine) return nullptr;
  try {
    recipe::SearchOptions options;
    options.assume_staples = assume_staples;
    recipe::ParsedPantry parsed = engine->searcher->parse(to_strings(pantry, pantry_count));
    std::sort(parsed.known.begin(), parsed.known.end());

    auto out = std::make_unique<FridgeResults>();
    out->unrecognized = std::move(parsed.unrecognized);
    for (size_t i = 0; i < id_count && ids; ++i) {
      if (ids[i] >= engine->index.num_recipes()) continue;  // stale id from another dataset
      out->items.push_back(make_item(engine->index, ids[i], parsed.known, options));
    }
    return out.release();
  } catch (const std::exception&) {
    return nullptr;
  }
}

void fridge_results_destroy(FridgeResults* results) { delete results; }

double fridge_results_search_micros(const FridgeResults* results) {
  return results ? results->micros : 0;
}

size_t fridge_results_unrecognized_count(const FridgeResults* results) {
  return results ? results->unrecognized.size() : 0;
}

const char* fridge_results_unrecognized(const FridgeResults* results, size_t index) {
  if (!results || index >= results->unrecognized.size()) return "";
  return results->unrecognized[index].c_str();
}

size_t fridge_results_count(const FridgeResults* results) {
  return results ? results->items.size() : 0;
}

#define FRIDGE_ITEM_FIELD(fn, type, field, fallback)              \
  type fn(const FridgeResults* results, size_t i) {               \
    if (!results || i >= results->items.size()) return fallback;  \
    return results->items[i].field;                               \
  }

FRIDGE_ITEM_FIELD(fridge_result_id, uint32_t, id, 0)
FRIDGE_ITEM_FIELD(fridge_result_score, float, score, 0.0f)
FRIDGE_ITEM_FIELD(fridge_result_coverage, float, coverage, 0.0f)
FRIDGE_ITEM_FIELD(fridge_result_rating, float, rating, 0.0f)
FRIDGE_ITEM_FIELD(fridge_result_have, uint32_t, have, 0)
FRIDGE_ITEM_FIELD(fridge_result_total, uint32_t, total, 0)
#undef FRIDGE_ITEM_FIELD

const char* fridge_result_name(const FridgeResults* results, size_t i) {
  if (!results || i >= results->items.size()) return "";
  return results->items[i].name.c_str();
}

size_t fridge_result_ingredient_count(const FridgeResults* results, size_t i) {
  if (!results || i >= results->items.size()) return 0;
  return results->items[i].ingredients.size();
}

const char* fridge_result_ingredient(const FridgeResults* results, size_t i, size_t j) {
  if (!results || i >= results->items.size() || j >= results->items[i].ingredients.size()) return "";
  return results->items[i].ingredients[j].name.c_str();
}

bool fridge_result_ingredient_have(const FridgeResults* results, size_t i, size_t j) {
  if (!results || i >= results->items.size() || j >= results->items[i].ingredients.size()) {
    return false;
  }
  return results->items[i].ingredients[j].have;
}

}  // extern "C"
