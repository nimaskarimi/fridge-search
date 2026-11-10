// Dependency-free test runner so the project builds anywhere (including
// Xcode) with no setup. Swapping to GoogleTest later is mechanical: each
// TEST(name) becomes TEST(Suite, name) and CHECK becomes EXPECT_TRUE.

#include <algorithm>
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <random>
#include <string>
#include <vector>

#include "recipe/index.h"
#include "recipe/normalize.h"
#include "recipe/search.h"
#include "recipe/synthetic.h"

namespace {

struct TestCase {
  const char* name;
  std::function<void()> fn;
};
std::vector<TestCase>& registry() {
  static std::vector<TestCase> tests;
  return tests;
}
int g_failures = 0;

struct Register {
  Register(const char* name, std::function<void()> fn) { registry().push_back({name, fn}); }
};

#define TEST(name)                                  \
  void test_##name();                               \
  Register register_##name(#name, test_##name);     \
  void test_##name()

#define CHECK(cond)                                                                \
  do {                                                                             \
    if (!(cond)) {                                                                 \
      std::printf("    FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);            \
      ++g_failures;                                                                \
    }                                                                              \
  } while (0)

#define CHECK_EQ(a, b)                                                             \
  do {                                                                             \
    auto va = (a);                                                                 \
    auto vb = (b);                                                                 \
    if (!(va == vb)) {                                                             \
      std::printf("    FAILED %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b);     \
      ++g_failures;                                                                \
    }                                                                              \
  } while (0)

using recipe::RecipeIndex;
using recipe::Searcher;
using recipe::SearchOptions;

std::string name_of(const RecipeIndex& index, const recipe::Result& r) {
  return index.recipe(r.id).name;
}

// ---------------------------------------------------------------- normalize

TEST(normalize_lowercases_and_singularizes) {
  CHECK_EQ(recipe::normalize_ingredient("Eggs"), std::string("egg"));
  CHECK_EQ(recipe::normalize_ingredient("  Large   Tomatoes "), std::string("tomato"));
  CHECK_EQ(recipe::normalize_ingredient("Fresh Raspberries"), std::string("fresh raspberry"));
  CHECK_EQ(recipe::normalize_ingredient("peaches"), std::string("peach"));
  CHECK_EQ(recipe::normalize_ingredient("Green Onions,"), std::string("green onion"));
}

TEST(normalize_strips_quantities_and_punctuation) {
  CHECK_EQ(recipe::normalize_ingredient("2 cups All-Purpose Flour"),
           std::string("all purpose flour"));
  CHECK_EQ(recipe::normalize_ingredient("1 can of black beans"), std::string("black bean"));
  CHECK_EQ(recipe::normalize_ingredient("1/2 tsp salt"), std::string("salt"));
  CHECK_EQ(recipe::normalize_ingredient("3 cups"), std::string(""));
}

TEST(normalize_handles_exceptions) {
  CHECK_EQ(recipe::normalize_ingredient("molasses"), std::string("molasses"));
  CHECK_EQ(recipe::normalize_ingredient("hummus"), std::string("hummus"));
  CHECK_EQ(recipe::normalize_ingredient("bay leaves"), std::string("bay leaf"));
  CHECK_EQ(recipe::normalize_ingredient("cookies"), std::string("cookie"));
  CHECK_EQ(recipe::normalize_ingredient("watercress"), std::string("watercress"));
}

TEST(split_trims_and_drops_empty) {
  auto parts = recipe::split(" eggs ,butter,, flour ", ',');
  CHECK_EQ(parts.size(), size_t{3});
  CHECK_EQ(parts[0], std::string("eggs"));
  CHECK_EQ(parts[2], std::string("flour"));
}

// ---------------------------------------------------------------- index

RecipeIndex small_index() {
  RecipeIndex index;
  index.add_recipe("Pancakes", {"flour", "eggs", "milk", "butter", "salt"}, 4.0f);
  index.add_recipe("Omelette", {"eggs", "butter", "cheese", "salt"}, 4.0f);
  index.add_recipe("Saffron Rice", {"rice", "saffron", "butter"}, 4.0f);
  index.add_recipe("Buttered Rice", {"rice", "butter", "salt"}, 4.0f);
  index.add_recipe("Lemonade", {"lemons", "sugar", "water"}, 4.0f);
  index.finalize();
  return index;
}

TEST(index_builds_sorted_postings_and_weights) {
  RecipeIndex index = small_index();
  CHECK_EQ(index.num_recipes(), size_t{5});
  auto butter = index.lookup("Butter");
  auto saffron = index.lookup("saffron");
  CHECK(butter.has_value());
  CHECK(saffron.has_value());
  CHECK_EQ(index.postings(*butter).size(), size_t{4});
  CHECK(std::is_sorted(index.postings(*butter).begin(), index.postings(*butter).end()));
  // Rare ingredients weigh more than common ones.
  CHECK(index.weight(*saffron) > index.weight(*butter));
  CHECK(!index.lookup("unicorn").has_value());
}

TEST(index_dedupes_ingredients_within_recipe) {
  RecipeIndex index;
  index.add_recipe("Double Egg", {"egg", "Eggs", "2 eggs"});
  index.finalize();
  CHECK_EQ(index.recipe(0).ingredients.size(), size_t{1});
}

TEST(index_expand_matches_partial_names) {
  RecipeIndex index;
  index.add_recipe("A", {"all-purpose flour", "vanilla extract", "bread flour", "eggs"});
  index.finalize();
  CHECK_EQ(index.expand("flour").size(), size_t{2});          // both flours
  CHECK_EQ(index.expand("2 cups flour").size(), size_t{2});   // quantity stripped first
  CHECK_EQ(index.expand("bread flour").size(), size_t{1});    // exact match wins
  CHECK_EQ(index.expand("vanilla").size(), size_t{1});
  CHECK(index.expand("almond flour").empty());                // every word must match
  CHECK(index.expand("dragonfruit").empty());
}

TEST(synthetic_corpus_has_full_vocabulary) {
  RecipeIndex index = recipe::make_synthetic_index(2000, 300, 1);
  CHECK(index.num_ingredients() > 250);  // names must survive normalization
}

TEST(index_loads_sample_tsv) {
  RecipeIndex index = RecipeIndex::load_tsv(SAMPLE_DATA);
  CHECK(index.num_recipes() >= 25);
  CHECK(index.lookup("all purpose flour").has_value());
}

TEST(index_rejects_malformed_tsv) {
  bool threw = false;
  try {
    RecipeIndex::load_tsv("/nonexistent/path.tsv");
  } catch (const std::runtime_error&) {
    threw = true;
  }
  CHECK(threw);
}

// ---------------------------------------------------------------- search

TEST(search_ranks_complete_match_first) {
  RecipeIndex index = small_index();
  Searcher searcher(index);
  auto results = searcher.search(std::vector<std::string>{"eggs", "butter", "cheese"});
  CHECK(!results.empty());
  CHECK_EQ(name_of(index, results[0]), std::string("Omelette"));
  CHECK_EQ(results[0].missing, uint32_t{0});
  CHECK(results[0].missing_ingredients.empty());
}

TEST(search_reports_missing_ingredients) {
  RecipeIndex index = small_index();
  Searcher searcher(index);
  auto results = searcher.search(std::vector<std::string>{"flour", "eggs", "butter"});
  CHECK_EQ(name_of(index, results[0]), std::string("Pancakes"));
  CHECK_EQ(results[0].missing_ingredients.size(), size_t{1});
  CHECK_EQ(index.ingredient_name(results[0].missing_ingredients[0]), std::string("milk"));
}

TEST(search_assumes_staples_unless_disabled) {
  RecipeIndex index = small_index();
  Searcher searcher(index);
  std::vector<std::string> pantry = {"rice", "butter"};

  auto with = searcher.search(pantry);
  CHECK_EQ(name_of(index, with[0]), std::string("Buttered Rice"));
  CHECK_EQ(with[0].missing, uint32_t{0});  // salt is a staple

  SearchOptions no_staples;
  no_staples.assume_staples = false;
  auto without = searcher.search(pantry, no_staples);
  for (const auto& r : without) {
    if (name_of(index, r) == "Buttered Rice") CHECK_EQ(r.missing, uint32_t{1});
  }
}

TEST(search_penalizes_missing_rare_ingredient_more) {
  RecipeIndex index = small_index();
  Searcher searcher(index);
  // With staples off, each rice dish misses exactly one ingredient: salt
  // (common) for Buttered Rice, saffron (rare) for Saffron Rice. Missing the
  // rare one should cost more coverage.
  SearchOptions opts;
  opts.assume_staples = false;
  auto results = searcher.search(std::vector<std::string>{"rice", "butter"}, opts);
  float saffron_cov = -1, buttered_cov = -1;
  for (const auto& r : results) {
    if (name_of(index, r) == "Saffron Rice") saffron_cov = r.coverage;
    if (name_of(index, r) == "Buttered Rice") buttered_cov = r.coverage;
  }
  CHECK(saffron_cov > 0);
  CHECK(buttered_cov > saffron_cov);
}

TEST(search_respects_top_k_and_max_missing) {
  RecipeIndex index = small_index();
  Searcher searcher(index);
  SearchOptions opts;
  opts.top_k = 2;
  CHECK_EQ(searcher.search(std::vector<std::string>{"butter"}, opts).size(), size_t{2});

  opts.top_k = 10;
  opts.max_missing = 0;
  for (const auto& r : searcher.search(std::vector<std::string>{"eggs", "butter", "cheese"}, opts)) {
    CHECK_EQ(r.missing, uint32_t{0});
  }
  opts.top_k = 0;
  CHECK(searcher.search(std::vector<std::string>{"butter"}, opts).empty());
}

TEST(search_handles_empty_and_unknown_pantry) {
  RecipeIndex index = small_index();
  Searcher searcher(index);
  CHECK(searcher.search(std::vector<std::string>{}).empty());
  auto parsed = searcher.parse({"eggs", "dragonfruit"});
  CHECK_EQ(parsed.known.size(), size_t{1});
  CHECK_EQ(parsed.unrecognized.size(), size_t{1});
}

TEST(search_is_repeatable_scratch_buffers_reset) {
  RecipeIndex index = small_index();
  Searcher searcher(index);
  auto a = searcher.search(std::vector<std::string>{"eggs", "butter"});
  searcher.search(std::vector<std::string>{"rice", "saffron"});
  auto b = searcher.search(std::vector<std::string>{"eggs", "butter"});
  CHECK_EQ(a.size(), b.size());
  for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
    CHECK_EQ(a[i].id, b[i].id);
    CHECK_EQ(a[i].score, b[i].score);
  }
}

// Property test: on thousands of random queries over a random corpus, the
// optimized search must return exactly what the brute-force scan returns.
TEST(search_matches_brute_force_on_random_queries) {
  RecipeIndex index = recipe::make_synthetic_index(5000, 400, 123);
  Searcher searcher(index);
  std::mt19937_64 rng(99);
  const SearchOptions variants[] = {
      {},
      {5, 2, 0.04f, 0.05f, true},
      {20, 100, 0.1f, 0.0f, false},
  };
  int mismatches = 0;
  for (int q = 0; q < 2000; ++q) {
    auto pantry = recipe::random_pantry(index, rng);
    const SearchOptions& opts = variants[q % 3];
    auto fast = searcher.search(pantry, opts);
    auto slow = searcher.search_brute_force(pantry, opts);
    bool ok = fast.size() == slow.size();
    for (size_t i = 0; ok && i < fast.size(); ++i) {
      ok = fast[i].id == slow[i].id && fast[i].score == slow[i].score &&
           fast[i].missing_ingredients == slow[i].missing_ingredients;
    }
    if (!ok) ++mismatches;
  }
  CHECK_EQ(mismatches, 0);
}

}  // namespace

int main() {
  for (const TestCase& t : registry()) {
    const int before = g_failures;
    t.fn();
    std::printf("[%s] %s\n", g_failures == before ? " OK " : "FAIL", t.name);
  }
  std::printf("\n%zu tests, %d failed checks\n", registry().size(), g_failures);
  return g_failures == 0 ? 0 : 1;
}
