#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace recipe {

// Turns free-form ingredient text into a canonical key so that the index and
// user queries agree on spelling:
//
//   "2 cups All-Purpose Flour"  -> "all purpose flour"
//   "Eggs"                      -> "egg"
//   "Fresh Raspberries"         -> "fresh raspberry"
//
// Steps: lowercase, replace punctuation with spaces, drop leading quantities
// and units, collapse whitespace, singularize the last word.
// Returns "" if nothing meaningful is left.
std::string normalize_ingredient(std::string_view raw);

// Singularizes one lowercase English word with a few rules plus an exception
// list. Heuristic by design: it only has to be consistent, because the same
// function runs on both the recipe data and the query.
std::string singularize(std::string_view word);

// Splits on `delim` and trims whitespace from each piece. Empty pieces are
// dropped. Does not normalize.
std::vector<std::string> split(std::string_view text, char delim);

}  // namespace recipe
