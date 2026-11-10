# Fridge Search: a recipe search engine in C++

Type what's in your fridge; get back recipes you can make, ranked by how
much of each recipe you already have, plus exactly what you're missing.

```
fridge> eggs, butter, flour, brown sugar, vanilla, baking soda
  1. Brown Butter Cookies                     score 1.05  have 7/7  rating 4.9
      you can make this now!
  2. Hard Boiled Eggs                         score 1.04  have 2/2  rating 4.0
      you can make this now!
  3. Pound Cake                               score 0.83  have 5/6  rating 4.5
      missing: white sugar
```

## How it works

```
recipes.tsv ──► normalize ──► inverted index ──► search ──► top-k results
                ("2 cups Eggs"   ingredient →       touch only     min-heap,
                  → "egg")       [recipe ids]       candidate      missing list
                                 + IDF weights      recipes
```

**Normalization** (`src/normalize.cpp`): lowercase, strip punctuation and
leading quantities/units, singularize the head noun. It runs on both the data
and the query, so "2 cups All-Purpose Flour" and "all purpose flour" meet.

**Inverted index** (`src/index.cpp`): ingredient → sorted list of recipe ids.
Each ingredient gets an IDF weight, `log(1 + N / df)`, so rare ingredients
(saffron) matter more than common ones (butter). A second index maps words to
ingredients so "flour" finds "all purpose flour" when there's no exact match.

**Ranking** (`src/search.cpp`):

```
coverage = Σ weight(ingredients you have) / Σ weight(all recipe ingredients)
score    = coverage − 0.04 × missing_count + 0.05 × rating / 5
```

Staples (salt, water, pepper) are assumed to be in every kitchen.

**Search algorithm**: walk the posting list of each pantry ingredient and
accumulate per-recipe match counts and weights into reusable scratch arrays.
Only recipes sharing at least one ingredient are ever touched. Score those and
keep the best *k* in a min-heap: `O(P + T log k)`, where *P* is the total
posting-list length for the pantry and *T* the number of touched recipes,
instead of scanning all *N* recipes.

## Build and run

Requires a C++17 compiler and CMake 3.16+.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/recipe_tests
./build/recipe_search data/sample_recipes.tsv
./build/recipe_bench
```

CLI options: `--query "a, b, c"`, `--k N`, `--max-missing N`, `--no-staples`.

### Real data (Food.com, ~230K recipes)

1. Download *Food.com Recipes and Interactions* from Kaggle.
2. Put `RAW_recipes.csv` and `RAW_interactions.csv` in `data/`.
3. Convert and run:

```bash
python3 scripts/prepare_foodcom.py data/RAW_recipes.csv data/foodcom.tsv \
    --interactions data/RAW_interactions.csv
./build/recipe_search data/foodcom.tsv
./build/recipe_bench --tsv data/foodcom.tsv
```

## iOS app

`ios/` has a SwiftUI app that runs the engine on-device through a small C
interface (`bridge/fridge_bridge.h`). Autocomplete, pantry chips, ranked
results, and a detail screen per recipe. See [ios/README.md](ios/README.md)
to build it.

## Benchmark

Synthetic corpus: 200K recipes, 5K ingredients with Zipf-distributed
popularity, 10K realistic pantry queries (one machine, Release build):

| Method | p50 | p95 | p99 |
|---|---|---|---|
| Indexed search | 1.2 ms | 2.7 ms | 3.4 ms |
| Brute-force scan | 30.7 ms | 42.3 ms | 46.5 ms |

About **25× faster at p50**, with 300/300 queries returning results identical
to brute force. *Re-run on your machine and on the Food.com data and update
this table with your own numbers.*

## Testing

`tests/tests.cpp` has unit tests for normalization, indexing, and ranking,
a C test that drives the iOS bridge (`tests/bridge_test.c`), and a property
test that checks the optimized search against the
brute-force reference on 2,000 random queries with varied options. CI runs
on Linux and macOS for every push.

## Roadmap

- [ ] Optimize: bitset intersection for common ingredients, smaller posting
      lists (delta + varint encoding); re-benchmark after each change
- [ ] Ingredient substitution graph (buttermilk → milk + lemon) with graph search
- [ ] Typo tolerance ("flor" → "flour") with a trie + edit distance
- [ ] REST API server and Docker image
- [x] iOS app (SwiftUI) as the front end
- [ ] Switch tests to GoogleTest
