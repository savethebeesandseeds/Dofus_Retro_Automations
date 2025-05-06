// ---- ui_base.hpp
#pragma once
#include "platform.hpp"
#include "model.hpp"
#include "item_stats.hpp"
#include "rune_db.hpp"
#include <string>

/* shared UI data --------------------------------------------------*/
class UI {
public:
    UI(const RuneDB& db);
    ~UI();

    void redraw();              // draw everything
    bool handle_input();        // true ⇒ quit
    ItemStats* curr_obj();
    
public:
    /* data --------------------------------------------------------*/
    const RuneDB& DB;
    std::vector<ItemStats> library_;   // all loaded objects
    bool select_mode_ = false;         // true ⇒ showing picker
    int  selObj_ = -1;                 // object selected index
    
    /* panes / geometry -------------------------------------------*/
    int rows_, cols_, header_h_=3, footer_h_=2,
    actions_w_, logs_w_, matrix_w_;
    WINDOW *header_, *footer_, *winA_, *winM_, *winL_;
    
    /* state ------------------------------------------------------*/
    int  focus_=0, selA_=0, selR_=1, selC_=1, log_ofs_=0;
    bool edit_mode_=false;
    std::string status_;
    
    /* helpers implemented elsewhere */
    static void box_title(WINDOW*, const std::string&);
    void draw_header(), draw_footer(), draw_actions(),
    draw_matrix(), draw_log();
    void merge_rune(int row,int col);
    void load_objects(const std::string& file = OBJECTS_PATH);
    void save_objects(const std::string& file = OBJECTS_PATH);
    
    /* edit helpers */
    void begin_add_object();
    void begin_edit_object();
    void begin_select_object();
    void add_stat_row();
    void delete_stat_row();
    void rename_object();
    void draw_picker();
    
    /* rune util */
    std::vector<const Rune*> runes_for(const std::string& stat) const;

    /* prompt */
    static std::string prompt_line(const char* msg);
    inline int prompt_int(const char* msg,int start);
};
