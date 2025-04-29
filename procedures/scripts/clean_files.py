import pandas as pd
import json
import os
import re
import secrets
from pathlib import Path
import json, secrets, sys
from pathlib import Path
import pandas as pd
import numpy as np


base_data_folder = '/src/data/objects/pure'
excel_file = '/src/procedures/scripts/objects_pure.xlsx'


def load_data_from_folder():
    folder = Path(base_data_folder)
    data = []

    for json_file in folder.glob('*.json'):
        with open(json_file, 'r', encoding='utf-8') as f:
            raw = json.load(f)

            # Clean up keys
            clean_item = {k.strip('"'): v for k, v in raw.items()}
            data.append(clean_item)

    # Create DataFrame
    df = pd.DataFrame(data)

    df.to_excel(excel_file, index=False)
    return df

def load_data_from_excel():
    return pd.read_excel(excel_file, keep_default_na=False)

def dump_rows_to_files(df, output_folder):
    output_folder = Path(output_folder)
    output_folder.mkdir(parents=True, exist_ok=True)

    for idx, row in df.iterrows():
        row_dict = row.to_dict()
        random_hex = secrets.token_hex(8)  # 8 bytes → 16 hex characters
        output_path = output_folder / f"{random_hex}.json"

        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(row_dict, f, ensure_ascii=False, indent=2)

    print(f"✅ {len(df)} rows dumped into {output_folder}")

def fix_full_name(df):
    mask = df['full_name'] == 'COSES'

    df.loc[mask, 'full_name'] = df.loc[mask].apply(
        lambda row: f"{row['name']} ({row['level']})", axis=1
    )

    def fix(name):
        name = str(name)  # Ensure it's a string
        if name and name[-1].isdigit() and not name.endswith(')'):
            return name + ')'
        return name

    df['full_name'] = df['full_name'].apply(fix)
    return df

def stats(df):
    num_duplicates = df.duplicated().sum()
    print(f"Total duplicate rows: {num_duplicates}")
    # df = df.drop_duplicates()
    
    num_full_name_duplicates = df.duplicated(subset="full_name").sum()
    print(f"Total duplicate 'full_name': {num_full_name_duplicates}")
    # df = df.drop_duplicates(subset="full_name")

    num_name_duplicates = df.duplicated(subset="name").sum()
    print(f"Total duplicate 'name': {num_name_duplicates}")
    # df = df.drop_duplicates(subset="full_name")

    # Step 1: Count occurrences
    counts = df['full_name'].value_counts()

    # Step 2: Filter to only those appearing more than 2 times
    filtered = counts[counts > 2]

    # Step 3: Print nicely
    for name, count in filtered.items():
        print(f"{count:3} × {name}")

    # Step 1: Collect all recipe columns
    recipe_cols = [col for col in df.columns if col.startswith('recipe:')]

    # Step 2: Stack all recipe values into a single big Series
    recipes = df[recipe_cols].values.ravel()

    # Step 3: Clean: Remove empty or weird entries
    recipes = pd.Series(recipes)
    recipes = recipes[recipes.notna()]              # Remove NaN if any
    recipes = recipes[recipes.str.strip() != '']     # Remove empty strings
    recipes = recipes[recipes != 'Y']                # Remove 'Y' if you want

    # Step 4: Count occurrences
    recipe_counts = recipes.value_counts()

    # Step 5: Filter and print nicely (e.g., only those appearing more than once)
    filtered_recipes = recipe_counts[recipe_counts > 10]

    # for ingredient, count in filtered_recipes.items():
    #     print(f"{count:3} × ...{ingredient}...")

def print_invalid_full_names(df):
    pattern = r'^.+ \(Niv\. \d+\)$'  # Matches "(Niv. 182)" at the END

    invalid_mask = ~df['full_name'].str.match(pattern)
    invalid_rows = df[invalid_mask]

    if invalid_rows.empty:
        print("✅ All 'full_name' entries are correctly formatted.")
    else:
        print(f"⚠️ Found {len(invalid_rows)} invalid 'full_name' entries:\n")
        for idx, row in invalid_rows.iterrows():
            print(f"Row {idx}: {row['full_name']}")

    return invalid_rows

def print_invalid_categories(df):
    pattern_parenthesis = r'.*\(Categoría : [\wÀ-ÿ\s]+\)$'
    pattern_simple = r'^Categoría : [\wÀ-ÿ\s]+$'

    valid_mask = df['category'].str.match(pattern_parenthesis) | df['category'].str.match(pattern_simple)
    invalid_mask = ~valid_mask

    invalid_rows = df[invalid_mask]

    if invalid_rows.empty:
        print("✅ All 'category' entries are correctly formatted.")
    else:
        print(f"⚠️ Found {len(invalid_rows)} invalid 'category' entries:\n")
        for idx, row in invalid_rows.iterrows():
            print(f"Row {idx}: {row['category']}")

    return invalid_rows


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _norm(v):
    """unify values before comparing: NaN → '', strip strings."""
    if isinstance(v, float) and np.isnan(v):   # fast path for NaN
        return ""
    if pd.isna(v):
        return ""
    if isinstance(v, str):
        return v.strip()
    return v


def print_group_differences(group):
    """
    group = list of (idx, row) tuples coming from one ‘full_name’.
    Shows ONLY the columns whose (normalised) values differ.
    """
    if len(group) < 2:
        return

    idxs, rows = zip(*group)
    cols = rows[0].index

    for col in cols:
        vals = [_norm(r[col]) for r in rows]
        if not all(v == vals[0] for v in vals):
            print(f"  ✏️  Difference in «{col}»")
            for idx, v in zip(idxs, vals):
                print(f"     Row {idx}: {v}")
    print()  # spacer


# ---------------------------------------------------------------------------
# 1)  Audit duplicates and show where they differ
# ---------------------------------------------------------------------------

def print_ordered_full_name_duplicates(df):
    """
    Groups duplicates by 'full_name', prints where columns differ.
    Returns the slice containing only duplicate rows (for further use).
    """
    dup_mask = df.duplicated('full_name', keep=False)
    dups      = df[dup_mask].sort_values('full_name')

    if dups.empty:
        print("✅  No duplicate «full_name» entries found.")
        return dups

    print(f"⚠️  {len(dups)} rows share a duplicate «full_name»:\n")

    current_name, group = None, []
    for idx, row in dups.iterrows():
        if row['full_name'] != current_name:
            if group:                      # flush previous block
                print_group_differences(group)
                group.clear()

            current_name = row['full_name']
            print(f"🔹 {current_name}")
        group.append((idx, row))

    if group:                              # flush last
        print_group_differences(group)

    return dups


# ---------------------------------------------------------------------------
# 2)  Flag-keeping column & interactive resolver
# ---------------------------------------------------------------------------

def ensure_keep_column(df, col='keep'):
    """Add a boolean column defaulting to True if it doesn’t exist yet."""
    if col not in df.columns:
        df[col] = True
    return col


def resolve_duplicates_interactive(df, col='keep'):
    """
    Walk through each duplicate pair and ask the user:
    1 = keep first   |  2 = keep second   |  q = keep both
    Anything else = skip / leave as is.
    Works best from a terminal / ipython console.
    """
    col = ensure_keep_column(df, col)
    dup_groups = df[df.duplicated('full_name', keep=False)].groupby('full_name')

    for name, grp in dup_groups:
        if len(grp) != 2:
            # For groups with >2 duplicates we leave untouched (or extend logic here)
            continue

        i1, i2 = grp.index.tolist()
        print(f"\n🔸 {name}")
        print_group_differences([(i1, df.loc[i1]), (i2, df.loc[i2])])

        while True:
            ans = input("Keep 1 / 2 / q (both) ?  ").strip().lower()
            if ans in {'1', '2', 'q', ''}:
                break
            print("Please answer 1, 2, q, or <Enter> to skip.")

        if ans == '1':
            df.loc[i2, col] = False
        elif ans == '2':
            df.loc[i1, col] = False
        elif ans == 'q':
            pass      # keep both – already True
        # else empty input: leave as-is (still both True)

    print("\n✅  Interactive pass finished.")
    return df


# ---------------------------------------------------------------------------
# 3)  Dump only the rows flagged True — omitting the flag column
# ---------------------------------------------------------------------------

def dump_rows_to_files_extra(df, output_folder, col='keep'):
    """
    Writes every row whose `col` is True to a randomly-named .json file.
    The flag column itself is NOT included in the output.
    """
    output = Path(output_folder)
    output.mkdir(parents=True, exist_ok=True)

    kept = df[df[col] == True]          # noqa: E712  (True is a literal here)
    for _, row in kept.iterrows():
        row_dict = row.drop(labels=[col]).to_dict()
        fname    = f"{secrets.token_hex(8)}.json"
        with open(output / fname, "w", encoding="utf-8") as fh:
            json.dump(row_dict, fh, ensure_ascii=False, indent=2)

    print(f"📦  Dumped {len(kept)} rows to «{output}».")


# ---------------------------------------------------------------------------
# helper: treat "", NaN, and the stray "Y" placeholder as empty -------------
# ---------------------------------------------------------------------------
def _empty(v):
    if pd.isna(v):
        return True
    if isinstance(v, str):
        return v.strip() == "" or v.strip().lower() == "y"
    return False


# ---------------------------------------------------------------------------
# continuity checker ---------------------------------------------------------
# ---------------------------------------------------------------------------
def print_recipe_discontinuities(df, prefix="recipe:", count=8):
    """
    For each row, verifies that the recipe columns form a solid prefix
    of non-empty entries (length 0-count).  Gaps ⇒ reported.

    returns a DataFrame slice containing only the faulty rows
    """
    cols = [f"{prefix}{i}" for i in range(1, count + 1)]
    bad_idx = []

    for idx, row in df.iterrows():
        flags = [_empty(row[c]) for c in cols]      # True = empty
        try:
            first_empty = flags.index(True)         # where emptiness starts
        except ValueError:                          # no empties at all ⇒ ok
            continue

        # if any later column is non-empty → discontinuity
        if any(not f for f in flags[first_empty + 1:]) or first_empty == 0 and not all(flags):
            bad_idx.append(idx)
            pattern = "".join("■" if not f else "·" for f in flags)
            print(f"❌  row {idx} : [{pattern}] : {row['full_name']} ")

    if not bad_idx:
        print("✅  All rows have continuous recipe sequences.")
        return pd.DataFrame(columns=df.columns)

    print(f"\n⚠️  {len(bad_idx)} rows need attention.\n")
    return df.loc[bad_idx]

# ---------------------------------------------------------------------------
# EXAMPLE WORKFLOW
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    # Example usage:
    # print(df)

    # df = load_data_from_folder()
    df = load_data_from_excel()
    # df = df.drop_duplicates()
    # df = fix_full_name(df)

    # dump_rows_to_files(df, "/src/data/objects/puree/")

    
    
    
    print(".....")


    stats(df)



    # 1. inspect 
    print_invalid_full_names(df)
    print_invalid_categories(df)
    duplicates = print_ordered_full_name_duplicates(df)
    print_recipe_discontinuities(df)

    # 2. mark what you want to keep
    # resolve_duplicates_interactive(df)

    # 3. after review, dump the survivors
    # dump_rows_to_files_extra(df, "/src/data/objects/puree/")
    


