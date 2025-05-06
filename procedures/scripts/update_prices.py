from utils.price_utils import *

if __name__ == '__main__':
    # Standard curation: load from JSON folder, clean, and save to Excel
    price_df = load_prices_from_folder()

    print(f'Curation complete: {len(price_df)} items loaded and saved to {PRICE_EXCEL_FILE}')