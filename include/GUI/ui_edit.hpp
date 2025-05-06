// ---- ui_edit.hpp
#pragma once
#include "ui_base.hpp"
#include <fstream>
#include <cstring>

/* merge stub */
inline void UI::merge_rune(int row,int ix){
    if(curr_obj() == nullptr) { status_ = "ERROR: Unable to merge_rune, there is no object selected."; return; }
    auto ru=runes_for(curr_obj()->rows[row].target);
    if(ix>=(int)ru.size()){ status_="No rune for slot"; return; }
    status_="Would merge "+ru[ix]->name;
    model::push_log(status_);
}

/* load all existing lines – call once at startup */
inline void UI::load_objects(const std::string& file)
{
    library_.clear();
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line))
    {
        ItemStats it;

        /*-------- very small string parsing -------------------*/
        auto val = [&](const char* key) -> std::string {
            std::string k = std::string("\"") + key + "\":\"";
            size_t p = line.find(k);
            if (p == std::string::npos) return {};
            p += k.size();
            return line.substr(p, line.find('"', p) - p);
        };
        it.name     = val("name");
        it.category = val("category");

        /* parse stats array [{...},{...}] ----------------------*/
        size_t statsPos = line.find("\"stats\":[");
        if (statsPos != std::string::npos)
        {
            size_t p = statsPos + 9;               // after '['
            while (true)
            {
                size_t obj = line.find('{', p);
                if (obj == std::string::npos) break;
                size_t end = line.find('}', obj);
                if (end == std::string::npos) break;
                std::string chunk = line.substr(obj, end - obj + 1);

                std::string name  = val("stat");
                int cur  = std::stoi( val("cur" ).empty() ? "0":  val("cur" ));
                int mn   = std::stoi( val("min" ).empty() ? "0":  val("min" ));
                int mx   = std::stoi( val("max" ).empty() ? "0":  val("max" ));

                auto v = [&](const char* k){ size_t p=chunk.find(std::string("\"")+k+"\":");
                      if(p==std::string::npos) return 0;
                      p += strlen(k)+3; return std::atoi(chunk.c_str()+p); };
                name = [&]{
                    size_t p=chunk.find("\"stat\":\"");
                    if(p==std::string::npos) return std::string{};
                    p+=8; return chunk.substr(p, chunk.find('"',p)-p);
                }();
                cur=v("cur"); mn=v("min"); mx=v("max");
                it.rows.push_back({name,cur,mn,mx});
                p = end + 1;
            }
        }
        /*------------------------------------------------------*/
        if (!it.name.empty()) library_.push_back(std::move(it));
    }
}


/* save current item – append json‑line */
inline void UI::save_objects(const std::string& file){
    std::ofstream out(file, std::ios::trunc);  // overwrite the file
    if (!out) {
        status_ = "Cannot open " + file;
        return;
    }

    for (const auto& item : library_) {
        if (item.name.empty() || item.category.empty()) continue;

        out << '{';
        out << "\"name\":\"" << item.name << "\",";
        out << "\"category\":\"" << item.category << "\",";
        out << "\"stats\":[";
        for (size_t i = 0; i < item.rows.size(); ++i) {
            const Stat& s = item.rows[i];
            out << '{'
                << "\"stat\":\"" << s.target << "\","
                << "\"cur\":" << s.cur << ","
                << "\"min\":" << s.mn << ","
                << "\"max\":" << s.mx << '}';
            if (i + 1 < item.rows.size()) out << ',';
        }
        out << "]}\n";
    }

    if (!out) {
        status_ = "Write failed";
        return;
    }

    status_ = "Saved full library → " + file;
    model::push_log(status_);
}


/* prompt helper */
inline std::string UI::prompt_line(const char* msg){
    echo(); curs_set(1);
    mvprintw(LINES-1,0,"%s",msg); clrtoeol();
    char buf[128]; getnstr(buf,127);
    noecho(); curs_set(0);
    return buf;
}
/* prompt helpers */
inline int UI::prompt_int(const char* msg,int start){
    echo(); curs_set(1);
    mvprintw(LINES-1,0,"%s (%d): ",msg,start); clrtoeol();
    char buf[32]; getnstr(buf,31);
    noecho(); curs_set(0);
    return std::atoi(buf);
}

/* edit helpers */
inline void UI::add_stat_row(){ if(curr_obj() == nullptr) { status_ = "ERROR: Unable to add_stat_row, there is no object selected."; return; } curr_obj()->rows.push_back({"<type>",0,0,0}); selR_=curr_obj()->rows.size()-1; }
inline void UI::delete_stat_row(){ if(curr_obj() == nullptr) { status_ = "ERROR: Unable to delete_stat_row, there is no object selected."; return; } if(!curr_obj()->rows.empty()){ curr_obj()->rows.erase(curr_obj()->rows.begin()+selR_); selR_=std::max(0,selR_-1);} }
inline void UI::rename_object()
{
    if(curr_obj() == nullptr) { status_ = "ERROR: Unable to rename_object, there is no object selected."; return; }
    curr_obj()->name = prompt_line("New name : ");
    curr_obj()->category = prompt_line("New category  : ");
}
inline void UI::begin_add_object(){
    ItemStats new_item{};
    new_item.rows.push_back({"<type>",0,0,0});
    library_.push_back(new_item);
    selObj_ = library_.size() - 1;

    curr_obj()->name     = prompt_line("Item name : ");
    curr_obj()->category = prompt_line("Category  : ");
    curr_obj()->rows.clear();
    curr_obj()->rows.push_back({"<type>",0,0,0});
    edit_mode_ = true;
    selR_ = 0;            // ← fix (was 1)
    selC_ = 0;
    status_ = "Add mode";
}

inline void UI::begin_edit_object()
{
    if(curr_obj() == nullptr) { status_ = "ERROR: Unable to begin_edit_object, there is no object selected."; return; }
    if (curr_obj()->rows.empty())            // safety: ensure at least one row exists
        curr_obj()->rows.push_back({"<type>",0,0,0});

    edit_mode_ = true;
    selR_ = 0;                     // start on first row
    selC_ = 0;                     // …and column 0 (Stat) ← the key line
    focus_ = 1;
    status_ = "Edit mode";
}
inline void UI::begin_select_object(){
    load_objects();
    select_mode_=true; selObj_=0;
}
inline ItemStats* UI::curr_obj() {
    if(selObj_ == -1) {
        return nullptr;
    }
    return &library_[selObj_];
}