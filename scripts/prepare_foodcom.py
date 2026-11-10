#!/usr/bin/env python3
"""Converts the Food.com Kaggle dataset into the engine's TSV format.

Download "Food.com Recipes and Interactions" from Kaggle
(kaggle.com/datasets/shuyangli94/food-com-recipes-and-user-interactions)
and put RAW_recipes.csv (and optionally RAW_interactions.csv) in data/. Then:

    python3 scripts/prepare_foodcom.py data/RAW_recipes.csv data/foodcom.tsv \
        --interactions data/RAW_interactions.csv

Output lines: name<TAB>average_rating<TAB>ingredient1|ingredient2|...
Ingredient normalization happens in C++, so this script only cleans fields.
"""

import argparse
import ast
import csv
import sys
from collections import defaultdict


def load_ratings(path):
    """Average star rating per recipe id. Food.com uses 0 for 'review without
    a rating', so zeros are skipped."""
    totals = defaultdict(lambda: [0, 0])
    with open(path, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            try:
                rating = int(row["rating"])
            except (KeyError, ValueError):
                continue
            if rating > 0:
                t = totals[row["recipe_id"]]
                t[0] += rating
                t[1] += 1
    return {rid: s / n for rid, (s, n) in totals.items()}


def clean(text):
    return " ".join(text.replace("\t", " ").replace("|", " ").split())


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("recipes_csv")
    parser.add_argument("output_tsv")
    parser.add_argument("--interactions", help="RAW_interactions.csv for ratings")
    parser.add_argument("--limit", type=int, default=0, help="only convert the first N recipes")
    args = parser.parse_args()

    csv.field_size_limit(sys.maxsize)
    ratings = load_ratings(args.interactions) if args.interactions else {}

    written = skipped = 0
    with open(args.recipes_csv, newline="", encoding="utf-8") as src, \
         open(args.output_tsv, "w", encoding="utf-8") as out:
        out.write("# name\trating\tingredients\n")
        for row in csv.DictReader(src):
            try:
                ingredients = ast.literal_eval(row["ingredients"])  # stored as a Python list
            except (KeyError, ValueError, SyntaxError):
                skipped += 1
                continue
            name = clean(row.get("name", "")).title()
            ingredients = [clean(i) for i in ingredients if clean(i)]
            if not name or not ingredients:
                skipped += 1
                continue
            rating = ratings.get(row.get("id", ""), 0.0)
            out.write(f"{name}\t{rating:.2f}\t{'|'.join(ingredients)}\n")
            written += 1
            if args.limit and written >= args.limit:
                break

    print(f"wrote {written} recipes to {args.output_tsv} (skipped {skipped})")


if __name__ == "__main__":
    main()
