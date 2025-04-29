/* dconfig.hpp */
#pragma once
/*─────────────────────────────────────────────────────────────────────────────
 *  tiny, zero-boilerplate runtime configuration loader
 *────────────────────────────────────────────────────────────────────────────*/
#define _WIN32_WINNT 0x0A00 /* Target Windows 10 */
#include <windows.h>                       /* (only for BOOL, etc.)           */
#include <unordered_map>
#include <string>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include "dlog.hpp"                        /* keep your own log_* helpers     */

namespace cfg {

// ────────────────────────────────────────────────────────────────────────────
//  Singleton loader
// ────────────────────────────────────────────────────────────────────────────
class Config {
public:
    static Config& get()
    {
        static Config inst;
        return inst;
    }

    /*----------------------------------------------------------------------*/
    /*  (re)load a file – default is "./.config"                            */
    /*----------------------------------------------------------------------*/
    void load(const std::string& file = "./.config")
    {
        {
            std::lock_guard<std::mutex> lock(mu_);
            map_.clear();
    
            std::ifstream in(file);
            if (!in) {
                LOG_ERROR("Cannot open config file: %s\n", file.c_str());
                throw std::runtime_error("Config file not found");
            }
    
            std::string line;
            while (std::getline(in, line)) {
                if (line.empty() || line[0] == '#' || line[0] == ';')
                    continue;
    
                auto eq = line.find('=');
                if (eq == std::string::npos) continue;
    
                auto trim = [](std::string s) {
                    const char* ws = " \t\r\n";
                    size_t b = s.find_first_not_of(ws);
                    size_t e = s.find_last_not_of(ws);
                    return (b == std::string::npos)
                           ? std::string{}
                           : s.substr(b, e - b + 1);
                };
    
                map_[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
            }
            path_ = file;
        }
        du::set_min_level(get("log_level", "info").c_str());

        LOG_INFO("Loaded %zu configuration entries from %s\n",
                 map_.size(), path_.c_str());
    }

    /*----------------------------------------------------------------------*/
    /*  Generic getters (with defaults)                                     */
    /*----------------------------------------------------------------------*/
    std::string get(const std::string& key,
                    const std::string& def = "") const
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = map_.find(key);
        return (it == map_.end()) ? def : it->second;
    }

    template <typename T>
    T get(const std::string& key, const T& def = T{}) const
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = map_.find(key);
        return (it == map_.end()) ? def : convert<T>(it->second, def);
    }

private:
    Config()              { load(); }
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    /*  type-specific string → value converters  */
    template <typename T>
    static T convert(const std::string& s, const T& def);

    std::unordered_map<std::string, std::string> map_;
    std::string path_;
    mutable std::mutex mu_;
};

/*────────────────────────────  specialisations  ────────────────────────────*/
template <>
inline int Config::convert<int>(const std::string& s, const int& def)
{
    try { return std::stoi(s); } catch (...) { return def; }
}

template <>
inline double Config::convert<double>(const std::string& s, const double& def)
{
    try { return std::stod(s); } catch (...) { return def; }
}

template <>
inline bool Config::convert<bool>(const std::string& s, const bool& def)
{
    std::string v; v.reserve(s.size());
    for (char c : s) v.push_back(static_cast<char>(::tolower(c)));

    if (v == "1"    || v == "true"  || v == "yes" || v == "on")  return true;
    if (v == "0"    || v == "false" || v == "no"  || v == "off") return false;
    return def;
}

/*────────────────────────────  convenience macros  ─────────────────────────*/
#define CFG_STR(key,       ...)  ::cfg::Config::get().get(       (key), __VA_ARGS__)
#define CFG_INT(key,       ...)  ::cfg::Config::get().get<int>(  (key), __VA_ARGS__)
#define CFG_DBL(key,       ...)  ::cfg::Config::get().get<double>((key), __VA_ARGS__)
#define CFG_BOOL(key,      ...)  ::cfg::Config::get().get<bool>( (key), __VA_ARGS__)

/*  Example
 *  -------
 *      std::string window     = CFG_STR ("window"              , "MainWnd");
 *      int         step_ms    = CFG_INT ("step_period"         , 30);
 *      double      dpi_scale  = CFG_DBL ("screen_dpi_scale"    , 1.0);
 *      bool        del_temp   = CFG_BOOL("delete_temp"         , false);
 *
 *  Adding / removing keys now requires **zero** extra code – just edit .config.
 */

} // namespace cfg
