// Measures query latency for the indexed search vs. the brute-force baseline,
// and checks that both return identical results.
//
//   recipe_bench                         # synthetic corpus, 200K recipes
//   recipe_bench --recipes 50000 --queries 5000
//   recipe_bench --tsv data/foodcom.tsv  # real data

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "recipe/index.h"
#include "recipe/search.h"
#include "recipe/synthetic.h"

namespace {

using Clock = std::chrono::steady_clock;

struct Stats {
  double p50, p95, p99, mean;
};

Stats summarize(std::vector<double> micros) {
  std::sort(micros.begin(), micros.end());
  auto pct = [&](double p) { return micros[static_cast<size_t>(p * (micros.size() - 1))]; };
  double sum = 0;
  for (double m : micros) sum += m;
  return {pct(0.50), pct(0.95), pct(0.99), sum / micros.size()};
}

void print(const char* label, size_t n, const Stats& s) {
  std::printf("%-14s %6zu queries   p50 %9.1f us   p95 %9.1f us   p99 %9.1f us   mean %9.1f us\n",
              label, n, s.p50, s.p95, s.p99, s.mean);
}

bool same(const std::vector<recipe::Result>& a, const std::vector<recipe::Result>& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i].id != b[i].id || a[i].score != b[i].score) return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  size_t num_recipes = 200000;
  size_t num_queries = 10000;
  size_t brute_queries = 300;
  std::string tsv;
  for (int i = 1; i + 1 < argc; i += 2) {
    std::string arg = argv[i];
    if (arg == "--recipes") num_recipes = std::stoul(argv[i + 1]);
    else if (arg == "--queries") num_queries = std::stoul(argv[i + 1]);
    else if (arg == "--brute-queries") brute_queries = std::stoul(argv[i + 1]);
    else if (arg == "--tsv") tsv = argv[i + 1];
  }

  auto t0 = Clock::now();
  recipe::RecipeIndex index = tsv.empty() ? recipe::make_synthetic_index(num_recipes, 5000, 42)
                                          : recipe::RecipeIndex::load_tsv(tsv);
  const double build_ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
  std::printf("Corpus: %zu recipes, %zu ingredients (%s)\n", index.num_recipes(),
              index.num_ingredients(), tsv.empty() ? "synthetic, seed 42" : tsv.c_str());
  std::printf("Index build: %.0f ms\n\n", build_ms);

  std::mt19937_64 rng(7);
  std::vector<std::vector<recipe::IngredientId>> queries;
  for (size_t q = 0; q < num_queries; ++q) queries.push_back(recipe::random_pantry(index, rng));

  recipe::Searcher searcher(index);
  recipe::SearchOptions options;
  size_t checksum = 0;

  // Warm up caches before timing.
  for (size_t q = 0; q < std::min<size_t>(200, queries.size()); ++q) {
    checksum += searcher.search(queries[q], options).size();
  }

  std::vector<double> indexed;
  for (const auto& q : queries) {
    auto start = Clock::now();
    auto results = searcher.search(q, options);
    indexed.push_back(std::chrono::duration<double, std::micro>(Clock::now() - start).count());
    checksum += results.size();
  }

  std::vector<double> brute;
  size_t mismatches = 0;
  brute_queries = std::min(brute_queries, queries.size());
  for (size_t i = 0; i < brute_queries; ++i) {
    auto start = Clock::now();
    auto expected = searcher.search_brute_force(queries[i], options);
    brute.push_back(std::chrono::duration<double, std::micro>(Clock::now() - start).count());
    if (!same(expected, searcher.search(queries[i], options))) ++mismatches;
  }

  const Stats si = summarize(indexed);
  const Stats sb = summarize(brute);
  print("Indexed", indexed.size(), si);
  print("Brute force", brute.size(), sb);
  std::printf("\nSpeedup: %.1fx at p50, %.1fx at p99\n", sb.p50 / si.p50, sb.p99 / si.p99);
  std::printf("Correctness: %zu/%zu queries matched brute force exactly\n",
              brute_queries - mismatches, brute_queries);
  std::cerr << "(checksum " << checksum << ")\n";
  return mismatches == 0 ? 0 : 1;
}
