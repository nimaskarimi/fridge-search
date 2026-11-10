// Command-line front end for the engine.
//
//   recipe_search data/sample_recipes.tsv
//   recipe_search data/sample_recipes.tsv --query "eggs, butter, flour, sugar"
//   recipe_search data/foodcom.tsv --k 5 --max-missing 2 --no-staples
//
// Without --query it starts an interactive prompt.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "recipe/index.h"
#include "recipe/normalize.h"
#include "recipe/search.h"

namespace {

void usage() {
  std::cerr << "usage: recipe_search <recipes.tsv> [--query \"a, b, c\"] [--k N]\n"
               "                     [--max-missing N] [--no-staples]\n";
}

void run_query(const recipe::RecipeIndex& index, recipe::Searcher& searcher,
               const std::string& line, const recipe::SearchOptions& options) {
  const recipe::ParsedPantry pantry = searcher.parse(recipe::split(line, ','));

  const auto start = std::chrono::steady_clock::now();
  const std::vector<recipe::Result> results = searcher.search(pantry.known, options);
  const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
                          std::chrono::steady_clock::now() - start)
                          .count();

  if (!pantry.unrecognized.empty()) {
    std::cout << "  (didn't recognize:";
    for (const auto& u : pantry.unrecognized) std::cout << " \"" << u << "\"";
    std::cout << ")\n";
  }
  if (results.empty()) {
    std::cout << "  No recipes found.\n";
  }
  int rank = 1;
  for (const recipe::Result& r : results) {
    const recipe::Recipe& rec = index.recipe(r.id);
    std::printf("%3d. %-40s score %.2f  have %u/%zu", rank++, rec.name.c_str(), r.score, r.have,
                rec.ingredients.size());
    if (rec.rating > 0) std::printf("  rating %.1f", rec.rating);
    std::printf("\n");
    if (r.missing_ingredients.empty()) {
      std::cout << "      you can make this now!\n";
    } else {
      std::cout << "      missing: ";
      for (size_t i = 0; i < r.missing_ingredients.size(); ++i) {
        std::cout << (i ? ", " : "") << index.ingredient_name(r.missing_ingredients[i]);
      }
      std::cout << "\n";
    }
  }
  std::cout << "  (" << micros << " us)\n";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    usage();
    return 1;
  }
  std::string path = argv[1];
  std::string query;
  recipe::SearchOptions options;
  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= argc) {
        usage();
        std::exit(1);
      }
      return argv[++i];
    };
    if (arg == "--query") {
      query = next();
    } else if (arg == "--k") {
      options.top_k = std::stoul(next());
    } else if (arg == "--max-missing") {
      options.max_missing = static_cast<uint32_t>(std::stoul(next()));
    } else if (arg == "--no-staples") {
      options.assume_staples = false;
    } else {
      usage();
      return 1;
    }
  }

  recipe::RecipeIndex index;
  try {
    const auto start = std::chrono::steady_clock::now();
    index = recipe::RecipeIndex::load_tsv(path);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();
    std::cerr << "Loaded " << index.num_recipes() << " recipes, " << index.num_ingredients()
              << " ingredients in " << ms << " ms\n";
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
  recipe::Searcher searcher(index);

  if (!query.empty()) {
    run_query(index, searcher, query, options);
    return 0;
  }
  std::cout << "Type what's in your fridge, comma-separated (empty line to quit).\n";
  std::string line;
  while (std::cout << "fridge> " << std::flush, std::getline(std::cin, line)) {
    if (line.empty()) break;
    run_query(index, searcher, line, options);
  }
  return 0;
}
