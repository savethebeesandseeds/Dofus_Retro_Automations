from utils.menus import *
from utils.data_body import *

# Example usage
# if __name__ == '__main__':
#     mgr = DataManager(EXCEL_FILE_OBJECTS)
#     (mgr.reset()
#         .filter_contains_recipe('Tejido coralino')
#         .filter_by_level('Niv.40')
#         .interactive_edit()
#         .save()
#     )
#     print('Done saving.')

if __name__ == "__main__":
    mgr = DataManager(Path(EXCEL_FILE_OBJECTS))
    run_menu(mgr)

