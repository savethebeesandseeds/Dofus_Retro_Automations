// ---- model.hpp
#pragma once
#include <vector>
#include <string>

namespace model {

extern const std::vector<std::string> actions;
constexpr int COLS = 7;      // [Stat] + R1 R2 R3 Cur Max Min
extern std::vector<std::string> log;

/* helpers */
void push_log(const std::string& msg);

} // namespace model

// ---------------- implementation (header‑only) -------------------
namespace model {

const std::vector<std::string> actions = {
    "Select", "Add", "Edit (F1)", "Save (F2)", "Quit (q)"
};
    
std::vector<std::string> log;

inline void push_log(const std::string& msg) { log.emplace_back(msg); }

} // namespace model
