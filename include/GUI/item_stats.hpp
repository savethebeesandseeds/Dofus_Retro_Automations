// ----- item_stats.hpp
#pragma once
#include <string>
#include <vector>

struct Stat {
    std::string target;           // e.g. "Fuerza"
    int cur{}, mn{}, mx{};
};

struct ItemStats {
    std::string name;
    std::string category;
    std::vector<Stat> rows;

    void add_stat(const std::string& n,int cur,int mn,int mx)
    { rows.push_back({n,cur,mn,mx}); }
};
