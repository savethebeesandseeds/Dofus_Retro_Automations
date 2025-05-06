// ----- ui_input.hpp
#pragma once
#include "ui_base.hpp"
#include "ui_draw.hpp"
#include "ui_edit.hpp"

#pragma once
#include "ui_base.hpp"
#include "ui_draw.hpp"
#include "ui_edit.hpp"
#include <algorithm>

/* ---------- helpers ------------------------------------------- */
namespace {

bool global_keys(UI& ui, int ch)
{
    if (ch == 27) {                     // ESC
        if (ui.select_mode_) {          // leave picker
            ui.select_mode_ = false;
            return true;
        }
        if (ui.edit_mode_) {            // leave edit
            ui.edit_mode_ = false;
            ui.focus_ = 0;              // back to Actions
            ui.selC_ = 1; 
            return true;
        }
        if (ui.focus_ != 0) {           // anywhere else → Actions pane
            ui.focus_ = 0;
            return true;
        }
        /* already on Actions → treat ESC same as q = quit */
        return true;                    // request quit (main will exit)
    }

    if (ch == 'q')                      // explicit quit
        return true;

    if (ch == KEY_F(1)) { 
        if (ui.curr_obj() == nullptr) { ui.begin_select_object(); } 
        else { ui.begin_edit_object(); } 
    }

    if (ch == KEY_F(2)) {
        ui.save_objects();
        return false;
    }

    /* ───────────── TAB : cycle panes, tidy edit/view state ───────── */
    if (ch == '\t')
    {
        /* 1 . If we were editing the matrix, quit edit‑mode first.   */
        if (ui.edit_mode_) {
            ui.edit_mode_ = false;
        }

        /* 2 . Advance focus (0‑>1‑>2‑>0).                            */
        ui.focus_ = (ui.focus_ + 1) % 3;

        /* 3 . Cursor inside matrix pane */
        if (ui.focus_ == 1) {
            ui.select_mode_ = false;
            ui.edit_mode_   = false;
            ui.focus_       = 1;                   // matrix pane
            ui.selR_ = 0;
            ui.selC_ = ui.edit_mode_ ? 0 : 1;   // ← keep for edit/view …
        }
        return false;                       // key handled
    }
    return false;
}
    

/* -------------------------------------------------------------- */
bool picker_keys(UI& ui, int ch)
{
    if (!ui.select_mode_) return false;
    if (ch==KEY_UP   && ui.selObj_>0)                         --ui.selObj_;
    else if (ch==KEY_DOWN && ui.selObj_<int(ui.library_.size())-1) ++ui.selObj_;
    else if (ch=='\n' && !ui.library_.empty()) {
        ui.selObj_      = ui.selObj_;          // keep chosen index
        ui.select_mode_ = false;
        ui.edit_mode_   = false;
        ui.focus_       = 1;                   // matrix pane
        ui.selR_ = 0;
        ui.selC_ = 1;                          // start on first rune column
        ui.status_ = "Loaded " + ui.library_[ui.selObj_].name;
    }  else if (ch==27) ui.select_mode_ = false;               // cancel
    return true;
}

/* -------------------------------------------------------------- */
bool navigation_keys(UI& ui, int ch)
{
    if (!(ch==KEY_LEFT||ch==KEY_RIGHT||ch==KEY_UP||ch==KEY_DOWN||
          ch==KEY_PPAGE||ch==KEY_NPAGE))
        return false;

    auto clampRow = [&](int r){ return std::clamp(r,0,ui.curr_obj()? int(ui.curr_obj()->rows.size())-1 : 0);};
    // auto clampColView=[&](int c){ return std::clamp(c,0,model::COLS-1); };
    // auto clampColEdit=[&](int c){ return (c<=0)?0: (c<4)?4: std::min(c,6); };

    /* horizontal ------------------------------------------------*/
    if (ch==KEY_LEFT || ch==KEY_RIGHT){
        if (ui.focus_==1){
            if (!ui.edit_mode_) {        // --- VIEW mode (rune cols 1‑3 only)
                int next = ui.selC_ + (ch==KEY_RIGHT ? 1 : -1);
    
                /* wrap inside 1‑3 range */
                if (next < 1) next = 3;
                if (next > 3) next = 1;
    
                ui.selC_ = next;
            } else {
                /* cycle 0 <-> 4‑5‑6 */
                if (ch==KEY_RIGHT)
                    ui.selC_ = ui.selC_==0 ? 4 : ui.selC_==6 ? 0 : ui.selC_+1;
                else
                    ui.selC_ = ui.selC_==0 ? 6 : ui.selC_==4 ? 0 : ui.selC_-1;
            }
        }
    }
    /* vertical --------------------------------------------------*/
    else if (ch==KEY_UP || ch==KEY_DOWN){
        int dir = (ch==KEY_DOWN)? 1 : -1;
        if (ui.focus_==0){
            ui.selA_ = std::clamp(ui.selA_ + dir, 0, int(model::actions.size())-1);
        }else if (ui.focus_==1){
            if (!ui.curr_obj()) {
                ui.focus_ += (ch==KEY_RIGHT? 1 : -1);
                ui.focus_ = std::clamp(ui.focus_,0,2);
            } else {
                ui.selR_ = clampRow(ui.selR_ + dir);
            }
        }else if (ui.focus_==2){
            ui.log_ofs_ = std::clamp(ui.log_ofs_ - dir,
                                      0, std::max(0,int(model::log.size())-1));
        }
    }
    /* page‑up/down on log --------------------------------------*/
    else if (ui.focus_==2){
        if (ch==KEY_PPAGE) ui.log_ofs_ =
            std::min<int>(model::log.size()-1, ui.log_ofs_+5);
        else               ui.log_ofs_ = std::max(0, ui.log_ofs_-5);
    }
    return true;
}

/* -------------------------------------------------------------- */
bool edit_keys(UI& ui, int ch)
{
    if (!ui.edit_mode_) return false;

    switch (ch){
    case '+':  ui.add_stat_row(); return true;
    case '-':  ui.delete_stat_row(); return true;
    case 'r':  if(ui.focus_==1 && ui.selC_==0) ui.rename_object(); return true;
    case KEY_F(2): ui.save_objects(); return true;

    case '\n':
        if (ui.focus_==1 && ui.curr_obj() != nullptr){
            auto& st = ui.curr_obj()->rows[ui.selR_];
            if (ui.selC_==0) st.target = ui.prompt_line("Stat: ");
            else if (ui.selC_>=4){
                int* fld = ui.selC_==4 ? &st.cur : ui.selC_==5 ? &st.mx : &st.mn;
                *fld = ui.prompt_int("Value", *fld);
            }
        }
        return false;
    }
    return false;
}

/* -------------------------------------------------------------- */
bool view_keys(UI& ui, int ch)
{
    if (ui.edit_mode_ || ui.select_mode_) return false;

    if (ui.focus_==0 && ch=='\n'){             // ENTER on menu
        const std::string& a = model::actions[ui.selA_];

        if      (a=="Select") ui.begin_select_object();
        else if (a=="Add")    ui.begin_add_object();
        else if (a=="Edit (F1)") { if (ui.curr_obj() == nullptr) { ui.begin_select_object(); } else { ui.begin_edit_object();} }
        else if (a=="Save (F2)")   ui.save_objects();
        else if (a=="Quit (q)")          return true;   // ← only here we quit
        return false;                                 // handled, but not quit
    }
    if (ui.focus_==1 && ch=='\n' && ui.selC_<=3){     // merge cell
        ui.merge_rune(ui.selR_, ui.selC_-1);
        return false;
    }
    return false;
}


} // anonymous namespace
/* ---------- top‑level ------------------------------------------ */
inline bool UI::handle_input()
{
    int ch = wgetch(stdscr);
    status_.clear();

    if (global_keys(*this,ch))          return true;   // quit handled inside

    if (picker_keys(*this,ch))          return false;
    if (navigation_keys(*this,ch))      return false;
    if (edit_keys(*this,ch))            return false;
    if (view_keys(*this,ch))            return true;    // may return quit

    return false;
}
