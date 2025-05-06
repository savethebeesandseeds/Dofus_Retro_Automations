// ----- ui_draw.hpp
#pragma once
#include "ui_base.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>   // ← add this

/* ---------- column ratios (sum ≤ 1.0) ------------------------- */
static constexpr double ACTION_PCT = 0.14;   // 14 %  (was 20 %)
static constexpr double LOG_PCT    = 0.33;   // 33 %  (was 25 %)

/* ctor / dtor -----------------------------------------------------*/
inline UI::UI(const RuneDB& db):DB(db)
{
    initscr(); noecho(); cbreak(); keypad(stdscr,true); curs_set(0);
    start_color(); use_default_colors();
    init_pair(1,COLOR_BLACK,COLOR_CYAN);
    init_pair(2,COLOR_BLACK,COLOR_YELLOW);
    init_pair(3,COLOR_WHITE,-1);
    init_pair(4,COLOR_BLACK,238);
    init_pair(5,COLOR_GREEN,-1);
    init_pair(6,COLOR_CYAN,-1);
    init_pair(7,COLOR_BLACK,COLOR_WHITE);

    getmaxyx(stdscr,rows_,cols_);
    actions_w_ = int(cols_ * ACTION_PCT);
    logs_w_    = int(cols_ * LOG_PCT);
    matrix_w_  = cols_ - actions_w_ - logs_w_;

    header_=newwin(header_h_,cols_,0,0);
    footer_=newwin(footer_h_,cols_,rows_-footer_h_,0);
    winA_=newwin(rows_-header_h_-footer_h_,actions_w_,header_h_,0);
    winM_=newwin(rows_-header_h_-footer_h_,matrix_w_, header_h_,actions_w_);
    winL_=newwin(rows_-header_h_-footer_h_,logs_w_, header_h_,actions_w_+matrix_w_);
    
    keypad(winA_,true); keypad(winM_,true); keypad(winL_,true);

    /* load all saved objects on start */
    load_objects();
}
inline UI::~UI(){ endwin(); }

/* small shared util */
inline void UI::box_title(WINDOW*w,const std::string&t){ box(w,0,0); mvwprintw(w,0,2," %s ",t.c_str()); }

/* ------------ header / footer ---------------------------------- */
inline void UI::draw_header(){
    werase(header_); wbkgd(header_,COLOR_PAIR(1));
    std::ostringstream os;
    os<<"Forge Mage  ["<<(edit_mode_?"EDIT":"VIEW")<<"]";
    if(curr_obj() == nullptr) {
        os<<" — "<<"No object selected.";
    } else {
        if(!curr_obj()->name.empty())    os<<" — "<<curr_obj()->name;
        if(!curr_obj()->category.empty())os<<" | "<<curr_obj()->category;
    }
    mvwprintw(header_,1,2,"%s",os.str().c_str());
    wnoutrefresh(header_);
}
inline void UI::draw_footer()
{
    werase(footer_); wbkgd(footer_, COLOR_PAIR(2));
    if (edit_mode_)
        mvwprintw(footer_, 0, 2, "EDIT: arrows | + add | - del | r rename | ENTER edit | F2 save | ESC back   : %s", status_.c_str());
    else
        mvwprintw(footer_, 0, 2, "VIEW: TAB panes | ENTER select | ESC back/quit                                   : %s", status_.c_str());
    wnoutrefresh(footer_);
}

/* actions pane */
inline void UI::draw_actions(){
    werase(winA_); box_title(winA_,"Actions");
    for(size_t i=0;i<model::actions.size();++i){
        if(int(i)==selA_ && focus_==0) wattron(winA_,COLOR_PAIR(7)|A_BOLD);
        mvwprintw(winA_,i+1,2,"%s",model::actions[i].c_str());
        if(int(i)==selA_ && focus_==0) wattroff(winA_,COLOR_PAIR(7)|A_BOLD);
    }
    wnoutrefresh(winA_);
}

/* rune util */
inline std::vector<const Rune*> UI::runes_for(const std::string& stat) const{
    std::vector<const Rune*> v;
    for(const auto& r: DB.runes) if(r.target==stat) v.push_back(&r);
    std::sort(v.begin(),v.end(),[](auto a,auto b){return a->effect<b->effect;});
    if(v.size()>3) v.resize(3);
    return v;
}

/* ---------- matrix pane --------------------------------------- */
inline void UI::draw_matrix(){
    werase(winM_); box_title(winM_,"Item Stats");
    if(curr_obj() == nullptr) {
        mvwprintw(winM_,2,3,"(no object selected - use Select Object)");
        wnoutrefresh(winM_);
        return;
    } else if(curr_obj()->rows.empty()) {
        mvwprintw(winM_,2,3,"(no stats for this object — use Edit)");
        wnoutrefresh(winM_);
        return;
    }
    
    /* ---------- column layout ------------------------------------ */
    int inner = matrix_w_ - 2;
    const int stat_w = std::min(40, inner/3);   // big Stat column
    const int rune_w = 3;                       // slim grey slots
    const int num_w  = 10;                      // ← was 6  (now fits “1999999”)

    auto col_x = [&](int idx){
        if(idx==0)            return 1;
        else if(idx<=3)       return 1 + stat_w + (idx-1)*rune_w;
        else                  return 1 + stat_w + 3*rune_w + (idx-4)*num_w;
    };
    static const char* H[] = {"Stat","","","","Cur","Max","Min"};
    /* header */
    for(int c=0;c<model::COLS;++c)
        mvwprintw(winM_,1,col_x(c),"%s",H[c]);

    for(int r=0;r<int(curr_obj()->rows.size()) && 2+r<getmaxy(winM_)-1;++r){
        const Stat& st=curr_obj()->rows[r];

        /* column 0 highlight */
        bool stat_sel = (focus_==1 && r==selR_ && selC_==0);
        wbkgdset(winM_,COLOR_PAIR(3));
        if(stat_sel) wattron(winM_,COLOR_PAIR(5)|A_REVERSE);
        mvwprintw(winM_,2+r,1,"%s",st.target.c_str());
        if(stat_sel) wattroff(winM_,COLOR_PAIR(5)|A_REVERSE);

        /* remaining columns */
        auto runes=runes_for(st.target);
        for(int c=1;c<model::COLS;++c){
            bool sel=(focus_==1 && r==selR_ && c==selC_);
            wbkgdset(winM_, c<=3? COLOR_PAIR(4) : COLOR_PAIR(3));
            if(sel) wattron(winM_,COLOR_PAIR(5)|A_REVERSE);

            std::string txt;
            if(c<=3)            txt=(c-1)<(int)runes.size()? runes[c-1]->name : " ";
            else if(c==4)       txt=std::to_string(st.cur);
            else if(c==5)       txt=std::to_string(st.mx);
            else                txt=std::to_string(st.mn);

            mvwprintw(winM_,2+r,col_x(c),"%s",txt.c_str());
            if(sel) wattroff(winM_,COLOR_PAIR(5)|A_REVERSE);
        }
    }
    wbkgdset(winM_,COLOR_PAIR(3));
    wnoutrefresh(winM_);
}

/* log pane */
inline void UI::draw_log(){
    werase(winL_); box_title(winL_,"Log");
    int h=getmaxy(winL_)-2;
    int start=std::max(0,int(model::log.size())-h-log_ofs_);
    for(int i=start,row=1;i<int(model::log.size())&&row<=h;++i,++row){
        bool here=(focus_==2 && i==int(model::log.size())-1-log_ofs_);
        if(here) wattron(winL_,A_REVERSE);
        wattron(winL_,COLOR_PAIR(6));
        mvwprintw(winL_,row,1,"%s",model::log[i].c_str());
        wattroff(winL_,COLOR_PAIR(6));
        if(here) wattroff(winL_,A_REVERSE);
    }
    wnoutrefresh(winL_);
}

inline void UI::draw_picker()
{
    werase(winM_); box_title(winM_,"Select Object");

    if(library_.empty()){
        mvwprintw(winM_,2,3,"(no objects saved yet — use Add Object)");
        wnoutrefresh(winM_);
        return;
    }

    int h=getmaxy(winM_)-2;
    int start=std::max(0,selObj_-h/2);
    for(int i=start,row=1;i<int(library_.size())&&row<=h;++i,++row){
        bool here=(i==selObj_);
        if(here) wattron(winM_,COLOR_PAIR(5)|A_REVERSE);
        mvwprintw(winM_,row,2,"%-50s | %s",
                  library_[i].name.c_str(),
                  library_[i].category.c_str());
        if(here) wattroff(winM_,COLOR_PAIR(5)|A_REVERSE);
    }
    wnoutrefresh(winM_);
}


/* main compose now chooses which centre view */
inline void UI::redraw(){
    draw_header(); draw_footer(); draw_actions();
    if(select_mode_) {draw_picker();} else {draw_matrix();}
    draw_log(); doupdate();
}