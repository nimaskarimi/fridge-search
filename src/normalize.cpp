#include "recipe/normalize.h"

#include <cctype>
#include <unordered_map>
#include <unordered_set>

namespace recipe {
namespace {

bool ends_with(std::string_view s, std::string_view suffix) {
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Words that look plural but are not, or whose plural is irregular.
const std::unordered_map<std::string_view, std::string_view>& irregular() {
  static const std::unordered_map<std::string_view, std::string_view> m = {
      {"molasses", "molasses"}, {"hummus", "hummus"},     {"couscous", "couscous"},
      {"asparagus", "asparagus"}, {"citrus", "citrus"},   {"swiss", "swiss"},
      {"schnapps", "schnapps"}, {"bitters", "bitters"},   {"grits", "grits"},
      {"leaves", "leaf"},       {"halves", "half"},       {"loaves", "loaf"},
      {"cookies", "cookie"},    {"brownies", "brownie"},  {"chilies", "chili"},
      {"chillies", "chilli"},   {"smoothies", "smoothie"}, {"pierogies", "pierogi"},
  };
  return m;
}

const std::unordered_set<std::string_view>& units() {
  static const std::unordered_set<std::string_view> u = {
      "cup",   "cups",   "c",          "tbsp",        "tbs",      "tablespoon",
      "tablespoons",     "tsp",        "teaspoon",    "teaspoons", "oz",
      "ounce", "ounces", "lb",         "lbs",         "pound",    "pounds",
      "g",     "gram",   "grams",      "kg",          "ml",       "l",
      "liter", "liters", "pinch",      "dash",        "can",      "cans",
      "package",         "packages",   "pkg",         "stick",    "sticks",
      "of",    "a",      "an",         "large",       "medium",   "small",
  };
  return u;
}

bool is_quantity(std::string_view word) {
  for (char c : word) {
    if (std::isdigit(static_cast<unsigned char>(c))) return true;
  }
  return false;
}

}  // namespace

std::string singularize(std::string_view word) {
  if (auto it = irregular().find(word); it != irregular().end()) {
    return std::string(it->second);
  }
  if (word.size() <= 3) return std::string(word);  // "gas", "peas" edge cases aside
  if (ends_with(word, "ies")) {                      // berries -> berry
    return std::string(word.substr(0, word.size() - 3)) + "y";
  }
  if (ends_with(word, "ches") || ends_with(word, "shes") ||  // peaches -> peach
      ends_with(word, "xes") || ends_with(word, "oes")) {    // tomatoes -> tomato
    return std::string(word.substr(0, word.size() - 2));
  }
  if (ends_with(word, "ss") || ends_with(word, "us") || ends_with(word, "is")) {
    return std::string(word);  // "watercress", "octopus", "anis"
  }
  if (word.back() == 's') return std::string(word.substr(0, word.size() - 1));
  return std::string(word);
}

std::string normalize_ingredient(std::string_view raw) {
  // 1. Lowercase and split into alphanumeric words.
  std::vector<std::string> words;
  std::string current;
  for (char c : raw) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (std::isalnum(uc)) {
      current.push_back(static_cast<char>(std::tolower(uc)));
    } else if (!current.empty()) {
      words.push_back(std::move(current));
      current.clear();
    }
  }
  if (!current.empty()) words.push_back(std::move(current));

  // 2. Drop leading quantities and units ("2 cups", "1 can of").
  size_t start = 0;
  while (start < words.size() &&
         (is_quantity(words[start]) || units().count(words[start]) > 0)) {
    ++start;
  }
  if (start == words.size()) return "";

  // 3. Singularize the head noun (last word) and join.
  words.back() = singularize(words.back());
  std::string out;
  for (size_t i = start; i < words.size(); ++i) {
    if (!out.empty()) out.push_back(' ');
    out += words[i];
  }
  return out;
}

std::vector<std::string> split(std::string_view text, char delim) {
  std::vector<std::string> parts;
  size_t pos = 0;
  while (pos <= text.size()) {
    size_t end = text.find(delim, pos);
    if (end == std::string_view::npos) end = text.size();
    std::string_view piece = text.substr(pos, end - pos);
    while (!piece.empty() && std::isspace(static_cast<unsigned char>(piece.front()))) {
      piece.remove_prefix(1);
    }
    while (!piece.empty() && std::isspace(static_cast<unsigned char>(piece.back()))) {
      piece.remove_suffix(1);
    }
    if (!piece.empty()) parts.emplace_back(piece);
    pos = end + 1;
  }
  return parts;
}

}  // namespace recipe
