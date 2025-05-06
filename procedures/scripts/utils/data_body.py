import pandas as pd
from pathlib import Path
from typing import Union, Optional

from utils.query_utils import *
from utils.file_utils import *
from utils.price_utils import *
from utils.menus import *

class DataManager:
    """
    A handy class to load, query, interactively edit, and save object data.

    Usage:
        mgr = DataManager('/path/to/file.xlsx')
        mgr.filter_contains_recipe('tejido coralino') \
           .filter_by_level('Niv. 40')   \
           .interactive_edit()            \
           .save()
    """
    def __init__(self, path: Union[str, Path]):
        self.path = Path(path)
        # Load full DataFrame and remember original columns
        self.full_df, _ = load_data(self.path)
        self._base_columns = list(self.full_df.columns)
        # Arrange columns
        self.full_df = self.full_df[['full_name'] + [col for col in self._base_columns if col != 'full_name']]
        # Start with the full data as subset
        self.subset_df = self.full_df.copy()
        # Add fabrcation price
        self.update_fabrication_prices()


    def reset(self) -> 'DataManager':
        """Reset the subset to the full DataFrame."""
        self.subset_df = self.full_df.copy()
        return self

    def filter_exact_recipe(self, keyword: str) -> 'DataManager':
        """Keep only rows where any recipe column exactly matches keyword."""
        self.subset_df = filter_by_exact_recipe(self.subset_df, keyword)
        return self

    def filter_contains_recipe(self, keyword: str) -> 'DataManager':
        """Keep only rows where any recipe column contains keyword (case-insensitive)."""
        self.subset_df = filter_by_recipe_contains(self.subset_df, keyword)
        return self

    def filter_by_level(self, level: str) -> 'DataManager':
        """Keep only rows where 'level' == level."""
        self.subset_df = filter_by_level(self.subset_df, level)
        return self

    def filter_by_category(self, category: str) -> 'DataManager':
        """Keep only rows where 'category' == category."""
        self.subset_df = filter_by_category(self.subset_df, category)
        return self

    def filter_by_full_name(self, full_name: str) -> 'DataManager':
        """Keep only rows where 'full_name' == full_name."""
        self.subset_df = filter_by_full_name(self.subset_df, full_name)
        return self

    def filter_by_name(self, name: str) -> 'DataManager':
        """Keep only rows where 'name' == name."""
        self.subset_df = filter_by_name(self.subset_df, name)
        return self

    def filter_by_pods(self, min_pods: Optional[int] = None, max_pods: Optional[int] = None) -> 'DataManager':
        """Keep only rows where 'pods' is between min_pods and max_pods."""
        self.subset_df = filter_by_pods(self.subset_df, min_pods, max_pods)
        return self

    def filter_by_price(self, min_price: Optional[float] = None, max_price: Optional[float] = None) -> 'DataManager':
        """Keep only rows where 'price_beta' is between min_price and max_price."""
        self.subset_df = filter_by_price_beta(self.subset_df, min_price, max_price)
        return self

    def apply_filters(self, **kwargs) -> 'DataManager':
        """
        Apply multiple filters at once on the current subset. Any arg not provided is ignored.
        """
        self.subset_df = apply_filters(self.subset_df, **kwargs)
        return self

    def interactive_edit(self) -> 'DataManager':
        """
        Launch the curses-based editor on the current subset.
        After editing, merges changes back into full_df.
        """
        interactive_edit(self, self.path)
        # Merge edits into the full DataFrame by index
        self.full_df.update(self.subset_df)
        return self
    
    def update_fabrication_prices(self) -> 'DataManager':
        """Add a 'fabrication_price' column based on recipe ingredients."""
        price_df = load_price_data()
        self.full_df = add_fabrication_price_columns(self.full_df, price_df)
        self.subset_df = add_fabrication_price_columns(self.subset_df, price_df)
        return self

    def save(self) -> None:
        """Save the full DataFrame (with any edits) back to the original file,
        excluding any temporary columns like 'fabrication_price'."""
        # Save only the original columns to avoid writing temporary fields
        self.full_df.update(self.subset_df)
        to_save = self.full_df[self._base_columns]
        save_data(to_save, self.path)

    def show(self) -> pd.DataFrame:
        """Return the current subset DataFrame."""
        return self.subset_df

