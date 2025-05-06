import pandas as pd
import re
from pathlib import Path
from typing import Union, List, Any, Optional
import unicodedata

# Path to the Excel file
EXCEL_FILE_OBJECTS = Path('/src/data/objects/objects_pure.xlsx')

# Columns for recipes
RECIPE_COLS = [f"recipe:{i}" for i in range(1, 9)]


def simplify(text: str) -> str:
    """
    Lowercase and de-accent Latin text, keep only alphanumeric characters.
    """
    # Normalize and strip diacritics
    nfkd = unicodedata.normalize('NFD', text)
    no_diacritics = ''.join(ch for ch in nfkd if not unicodedata.combining(ch))
    # Lowercase and filter alphanumeric
    return ''.join(ch for ch in no_diacritics.lower() if ch.isalnum())


def load_data_from_excel(path: Path = EXCEL_FILE_OBJECTS) -> pd.DataFrame:
    """
    Load the objects DataFrame from Excel, preserving empty strings.
    """
    return pd.read_excel(path, keep_default_na=False)


def filter_by_exact_recipe(df: pd.DataFrame, keyword: str) -> pd.DataFrame:
    """
    Return rows where any recipe column exactly matches the keyword.
    """
    mask = df[RECIPE_COLS].eq(keyword).any(axis=1)
    return df[mask]


def filter_by_recipe_contains(df: pd.DataFrame, keyword: str) -> pd.DataFrame:
    """
    Return rows where any recipe column’s simplified text contains
    the simplified keyword.
    """
    key = simplify(keyword)

    # Simplify all recipe columns (strip accents, lowercase, alnum-only)
    simplified = df[RECIPE_COLS].astype(str).apply(lambda col: col.map(simplify))

    # Build a mask: True if any simplified column contains `key`
    mask = (
        simplified
        .apply(lambda col: col.str.contains(key, na=False), axis=0)
        .any(axis=1)
    )

    return df[mask]


def filter_by_level(df: pd.DataFrame, level: str) -> pd.DataFrame:
    """
    Keep only rows where the numeric part of 'level' equals
    the numeric part of the supplied level string.

    E.g. 'Niv. 40' → '40', so passing '40' or 'Niv.40' will match
    only rows whose level number is exactly 40 (not 140, etc.).
    """
    # Extract digits from the lookup string
    m = re.search(r'(\d+)', level)
    if not m:
        # no digits in input → no rows
        return df.iloc[0:0]

    key = m.group(1)

    # Vectorized extract of the first group of digits from each level cell
    # .str.extract returns a DataFrame; grab column 0
    nums = df['level'].astype(str).str.extract(r'(\d+)')[0]

    # Build mask: exactly equal to our key
    mask = nums == key

    return df[mask]


def filter_by_category(df: pd.DataFrame, category: str) -> pd.DataFrame:
    """
    Return rows where the 'category' column exactly matches the given category string.
    """
    key = simplify(category)
    # apply simplify to every entry in 'category' and test for substring
    mask = (
        df['category']
        .astype(str)
        .apply(simplify)
        .str.contains(key, na=False)
    )
    return df[mask]


def filter_by_full_name(df: pd.DataFrame, full_name: str) -> pd.DataFrame:
    """
    Return rows where the simplified 'full_name' contains the simplified keyword.
    """
    key = simplify(full_name)
    # apply simplify to every entry in 'full_name' and test for substring
    mask = (
        df['full_name']
        .astype(str)
        .apply(simplify)
        .str.contains(key, na=False)
    )
    return df[mask]


def filter_by_name(df: pd.DataFrame, name: str) -> pd.DataFrame:
    """
    Return rows where the 'name' column exactly matches the given name string.
    """
    key = simplify(name)
    # apply simplify to every entry in 'name' and test for substring
    mask = (
        df['name']
        .astype(str)
        .apply(simplify)
        .str.contains(key, na=False)
    )
    return df[mask]

def filter_by_pods(df: pd.DataFrame,
                   min_pods: Optional[int] = None,
                   max_pods: Optional[int] = None) -> pd.DataFrame:
    """
    Return rows where the numeric part of 'pods' is between min_pods and max_pods (inclusive).
    If min_pods or max_pods is None, that bound is ignored.
    Non-numeric or missing pods → excluded.
    """
    # Extract the first run of digits from each 'pods' entry
    nums = (
        df['pods']
          .astype(str)
          .str.extract(r'(\d+)', expand=False)
          .dropna()
          .astype(int)
    )

    # Start with all False, then mark True where within bounds
    mask = pd.Series(False, index=df.index)

    if min_pods is None and max_pods is None:
        # no bounds → keep all rows with numeric pods
        mask.loc[nums.index] = True
    else:
        if min_pods is not None:
            mask.loc[nums.index] |= nums >= min_pods
        if max_pods is not None:
            mask.loc[nums.index] &= nums <= max_pods

    return df[mask]


def filter_by_price_beta(df: pd.DataFrame,
                         min_price: float = None,
                         max_price: float = None) -> pd.DataFrame:
    """
    Return rows where the numeric part of 'price_beta' is between
    min_price and max_price (inclusive). Strip out any commas or dots
    (no decimals in your data) before comparing.
    """
    # 1) Normalize: remove commas and dots, coerce to numeric (NaN if fails)
    prices = (
        df['price_beta']
          .astype(str)
          .str.replace(r'[.,]', '', regex=True)
    )
    nums = pd.to_numeric(prices, errors='coerce')

    # 2) Build mask: start all False, then mark True where within bounds
    mask = pd.Series(False, index=df.index)

    # If no bounds given, keep all rows with a valid numeric price
    if min_price is None and max_price is None:
        mask = ~nums.isna()
    else:
        if min_price is not None:
            mask |= nums >= min_price
        if max_price is not None:
            mask &= nums <= max_price

    return df[mask]

def apply_filters(
    df: pd.DataFrame,
    category: str = None,
    full_name: str = None,
    name: str = None,
    level: str = None,
    min_pods: int = None,
    max_pods: int = None,
    min_price_beta: float = None,
    max_price_beta: float = None,
    recipe_exact: str = None,
    recipe_contains: str = None
) -> pd.DataFrame:
    """
    Apply multiple filters in one call. Any argument set to None is ignored.
    """
    mask = pd.Series(True, index=df.index)

    if category is not None:
        mask &= df['category'] == category
    if full_name is not None:
        mask &= df['full_name'] == full_name
    if name is not None:
        mask &= df['name'] == name
    if level is not None:
        mask &= df['level'] == level
    if min_pods is not None:
        mask &= df['pods'] >= min_pods
    if max_pods is not None:
        mask &= df['pods'] <= max_pods
    if min_price_beta is not None:
        mask &= df['price_beta'] >= min_price_beta
    if max_price_beta is not None:
        mask &= df['price_beta'] <= max_price_beta
    if recipe_exact is not None:
        mask &= df[RECIPE_COLS].eq(recipe_exact).any(axis=1)
    if recipe_contains is not None:
        mask &= (
            df[RECIPE_COLS]
              .apply(lambda col: col.str.contains(recipe_contains, case=False, na=False))
              .any(axis=1)
        )

    return df[mask]
