// ----- rune_db.hpp
// rune_db.hpp  ------------------------------------------------------
#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <cctype>

struct Rune {
    std::string name;
    int   effect{};      // integer
    float weight{};      // now float
    std::string target;
};

struct RuneDB {
    std::vector<Rune> runes;
    explicit RuneDB(const std::string& path) { load(path); }

    /* very small JSON loader – expects an array of { ... } objects    */
    void load(const std::string& path) {
        runes.clear();

        std::ifstream f(path);
        if (!f) throw std::runtime_error("Cannot open rune file: " + path);

        std::string txt((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
        auto ws = [&](size_t& i) {
            while (i < txt.size() && std::isspace(static_cast<unsigned>(txt[i])))
                ++i;
        };

        size_t i = 0;
        ws(i);
        if (i >= txt.size() || txt[i] != '[') return;  // not a JSON array
        ++i;

        while (i < txt.size()) {
            ws(i);
            if (i >= txt.size() || txt[i] == ']') break;
            if (txt[i] != '{') { ++i; continue; }

            Rune r;
            ++i;  /* inside object */

            while (i < txt.size() && txt[i] != '}') {
                ws(i);

                /* -------- key -------- */
                std::string key;
                if (txt[i] == '"') {
                    ++i;
                    while (txt[i] != '"') key += txt[i++];
                    ++i;
                }
                ws(i);
                if (txt[i] == ':') ++i;
                ws(i);

                /* -------- value ------ */
                if (key == "nombre" || key == "target") {
                    std::string val;
                    ++i;                               // opening quote
                    while (txt[i] != '"') val += txt[i++];
                    ++i;                               // closing quote
                    (key == "nombre" ? r.name : r.target) = val;
                } else {                               // "efecto" or "peso"
                    std::string num;
                    bool dot_seen = false;
                    while (std::isdigit(static_cast<unsigned>(txt[i])) || (!dot_seen && txt[i] == '.')) {
                        if (txt[i] == '.') dot_seen = true;
                        num += txt[i++];
                    }
                    if (key == "efecto")
                        r.effect = std::stoi(num);
                    else
                        r.weight = std::stof(num);
                }

                ws(i);
                if (txt[i] == ',') ++i;
            }
            if (txt[i] == '}') ++i;

            runes.push_back(r);

            ws(i);
            if (txt[i] == ',') ++i;
        }
    }

    const Rune* find(const std::string& n) const {
        for (const auto& r : runes)
            if (r.name == n) return &r;
        return nullptr;
    }
};
