// --- (main) forge_mage.cpp
#include "GUI/ui.hpp"
#include "rune_db.hpp"
#include "item_stats.hpp"

int main()
{
    RuneDB db("./data/runes/runes_weights.json");
    UI ui(db);

    clear();
    refresh();

    ui.redraw();                     // initial paint
    while (!ui.handle_input())       // then event loop
        ui.redraw();

    return 0;
}