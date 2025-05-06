"""
price_utils.py
------------------------
High‑level utilities for loading, cleaning and querying price data plus
helpers to evaluate crafting recipes (fabrication and average material
cost).

* All error/edge cases now return **np.nan** instead of silently falling
back to 0 so missing data is never mistaken for free.
* A dedicated ``parse_recipe`` helper validates **both** quantity and
name, so bad quantities are caught as well as bad names.
* Key logic is decomposed into small, reusable functions – easier to
test and maintain.
* Uses dataclasses and type hints throughout for clarity.

Main entry‑points
~~~~~~~~~~~~~~~~~
    load_price_data()              → curated **prices_df**
    compute_fabrication_price()    → cost for one recipe line
    compute_materials_avg_price()  → qty × avg_price for one recipe line
    add_fabrication_price_columns() → vectorised enrichment of a full
                                    recipe dataframe
"""

from __future__ import annotations

import json
import logging
import re
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Tuple

import numpy as np
import pandas as pd

from utils.query_utils import simplify

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
BASE_PRICE_FOLDER = Path("/src/data/resources/impure")
PRICE_EXCEL_FILE = Path("/src/data/resources/prices.xlsx")

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s: %(message)s")
logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
_RE_RECIPE = re.compile(r"x(\d+)\s*-\s*(.+)")  # strict ‑ dash mandatory

def parse_recipe(recipe: str) -> Optional[Tuple[int, str, str]]:
    """Return *(qty, name, name_key)* or **None** if the line is invalid.

    Quantity **must be positive**; leading/trailing spaces allowed.
    """
    if not recipe:
        return None

    m = _RE_RECIPE.fullmatch(recipe.strip())
    if not m:
        return None

    try:
        qty = int(m.group(1))
    except ValueError:
        return None

    if qty <= 0:
        return None

    name = m.group(2).strip()
    if not name:
        return None

    return qty, name, simplify(name)


def _clean_numeric_field(val: str | int | float) -> int:
    """Return **int** stripped of any non‑digit char; 0 when empty."""
    if val is None:
        return 0
    digits = re.sub(r"[^0-9]", "", str(val))
    return int(digits) if digits else 0


# ---------------------------------------------------------------------------
# Price‑file curation & loading
# ---------------------------------------------------------------------------
def curate_price_json_files(folder: Path) -> None:
    """Keep only the most recent JSON per *simplified name_key*."""
    if not folder.exists():
        logger.warning("Price folder does not exist: %s", folder)
        return

    latest: dict[str, tuple[Path, float]] = {}
    for p in folder.glob("*.json"):
        try:
            raw = json.loads(p.read_text("utf‑8"))
            name = raw.get("name", "").strip()
            if not name:
                continue
            key = simplify(name)
            mtime = p.stat().st_mtime
            # keep the newest file per key
            if key not in latest or mtime > latest[key][1]:
                latest[key] = (p, mtime)
        except Exception as exc:
            logger.error("Error reading %s: %s", p, exc)

    # delete any duplicates that are *not* the newest per key
    for p in folder.glob("*.json"):
        key = simplify(json.loads(p.read_text("utf‑8")).get("name", ""))
        if key in latest and p != latest[key][0]:
            try:
                p.unlink()
                logger.info("Deleted duplicate file: %s", p.name)
            except Exception as exc:
                logger.error("Failed deleting %s: %s", p, exc)


def _json_folder_to_dataframe(folder: Path) -> pd.DataFrame:
    """Read every *.json* in *folder* into a cleaned **DataFrame**."""
    records: list[dict] = []
    for p in folder.glob("*.json"):
        try:
            raw = json.loads(p.read_text("utf‑8"))
        except Exception as exc:
            logger.error("Could not parse %s: %s", p, exc)
            continue

        rec = {
            "category": raw.get("category", "").strip(),
            "name": raw.get("name", "").strip(),
            "pods": raw.get("pods", "").strip(),
            "avg_price": _clean_numeric_field(raw.get("avg_price", "")),
            "x1": _clean_numeric_field(raw.get("x1", "")),
            "x10": _clean_numeric_field(raw.get("x10", "")),
            "x100": _clean_numeric_field(raw.get("x100", "")),
        }
        records.append(rec)

    df = pd.DataFrame(records)
    if df.empty:
        return df

    df["name_key"] = df["name"].apply(simplify)
    numeric_cols = ["avg_price", "x1", "x10", "x100"]
    df[numeric_cols] = df[numeric_cols].astype(float)
    return df


def load_prices_from_folder(
    folder: Path | None = None,
    excel_file: Path | None = None,
    force_refresh: bool = False,
) -> pd.DataFrame:
    """Curate JSONs → DataFrame → save to Excel → return DataFrame."""
    folder = Path(folder or BASE_PRICE_FOLDER)
    excel_file = Path(excel_file or PRICE_EXCEL_FILE)

    if force_refresh:
        curate_price_json_files(folder)

    df = _json_folder_to_dataframe(folder)
    excel_file.parent.mkdir(parents=True, exist_ok=True)
    df.to_excel(excel_file, index=False)
    logger.info("Saved %d price rows → %s", len(df), excel_file)
    return df


def load_price_data(excel_file: Path | None = None, refresh_if_missing: bool = True) -> pd.DataFrame:
    """Load consolidated price DataFrame (adds *name_key* on the fly)."""
    excel_file = Path(excel_file or PRICE_EXCEL_FILE)
    if not excel_file.exists():
        if not refresh_if_missing:
            raise FileNotFoundError(excel_file)
        return load_prices_from_folder(excel_file=excel_file)

    df = pd.read_excel(excel_file, keep_default_na=False)
    if "name_key" not in df.columns:
        df["name_key"] = df["name"].apply(simplify)
    return df


# ---------------------------------------------------------------------------
# Cost computation primitives
# ---------------------------------------------------------------------------
def _effective_pack_prices(row: pd.Series) -> Tuple[float, float, float]:
    """Return best available (1, 10, 100)-pack prices for *row*.

    Fallback order as per game logic:

        100‑pack = x100  or x10*10  or x1*100  or avg*100
        10‑pack  = x10   or x100/10 or x1*10   or avg*10
        1‑pack   = x1    or x10/10  or x100/100 or avg
    """
    p1, p10, p100, avg = row[["x1", "x10", "x100", "avg_price"]]

    def _nz(v: float) -> float | None:
        return v if pd.notna(v) and v > 0 else None

    price100 = _nz(p100) or (_nz(p10) and _nz(p10) * 10) or (_nz(p1) and _nz(p1) * 100) or (_nz(avg) and _nz(avg) * 100) or np.nan
    price10 = _nz(p10) or (_nz(p100) and _nz(p100) / 10) or (_nz(p1) and _nz(p1) * 10) or (_nz(avg) and _nz(avg) * 10) or np.nan
    price1 = _nz(p1) or (_nz(p10) and _nz(p10) / 10) or (_nz(p100) and _nz(p100) / 100) or _nz(avg) or np.nan
    return price1, price10, price100


def compute_fabrication_price(recipe: str, price_df: pd.DataFrame) -> float:
    """Cost using best pack combination (np.nan if anything missing)."""
    parsed = parse_recipe(recipe)
    if parsed is None:
        return np.nan

    qty, _name, key = parsed
    row = price_df.loc[price_df["name_key"] == key]
    if row.empty:
        logger.warning("Price row missing for '%s'", _name)
        return np.nan

    price1, price10, price100 = _effective_pack_prices(row.iloc[0])
    if np.isnan(price1):  # all prices missing → cannot compute
        return np.nan

    cost = 0.0
    remaining = qty
    for size, unit_price in ((100, price100), (10, price10), (1, price1)):
        if np.isnan(unit_price):
            continue
        packs = remaining // size
        cost += packs * unit_price
        remaining -= packs * size
    # leftovers (if we ran out of pack sizes) cost = nan
    return cost if remaining == 0 else np.nan


def compute_materials_avg_price(recipe: str, price_df: pd.DataFrame) -> float:
    """Simply qty × avg_price (np.nan on any missing component)."""
    parsed = parse_recipe(recipe)
    if parsed is None:
        return np.nan

    qty, _name, key = parsed
    row = price_df.loc[price_df["name_key"] == key]
    if row.empty or pd.isna(row.iloc[0]["avg_price"]):
        logger.warning("Avg price missing for '%s'", _name)
        return np.nan

    return qty * float(row.iloc[0]["avg_price"])


# ---------------------------------------------------------------------------
# High‑level DataFrame utilities
# ---------------------------------------------------------------------------
def add_fabrication_price_columns(df: pd.DataFrame, price_df: pd.DataFrame) -> pd.DataFrame:
    """Return *df* plus columns: fabrication_price, avg_fabrication_price, error.

    • **fabrication_price** combines packs optimally
    • **avg_fabrication_price** = qty × avg_price (quick estimate)
    • **error** collects recipe items that had no price data or malformed lines
    """
    out = df.copy()

    if "name_key" not in price_df:
        price_df["name_key"] = price_df["name"].apply(simplify)

    recipe_cols = [c for c in out.columns if c.startswith("recipe:")]
    fabrication_totals: list[float] = []
    avg_totals: list[float] = []
    error_lists: list[str] = []

    for _, row in out[recipe_cols].iterrows():
        fab_total = 0.0
        avg_total = 0.0
        missing: List[str] = []

        for item in row.dropna():
            parsed = parse_recipe(item)
            if parsed is None:
                missing.append(item)
                continue

            qty, name, key = parsed
            price_row = price_df.loc[price_df["name_key"] == key]
            if price_row.empty:
                missing.append(name)
                continue

            fab_cost = compute_fabrication_price(item, price_df)
            avg_cost = compute_materials_avg_price(item, price_df)

            fab_total += 0.0 if np.isnan(fab_cost) else fab_cost
            avg_total += 0.0 if np.isnan(avg_cost) else avg_cost

        fabrication_totals.append(fab_total if fab_total else np.nan)
        avg_totals.append(avg_total if avg_total else np.nan)
        error_lists.append(", ".join(sorted(set(missing))))

    out["fabrication_price"] = fabrication_totals
    out["avg_fabrication_price"] = avg_totals
    out["error"] = error_lists
    return out

