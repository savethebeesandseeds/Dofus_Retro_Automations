// dlog.hpp
#pragma once
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <cctype>   // std::tolower

namespace du {

enum class Level { Debug, Event, Info, Warning, Error, Eureka };

inline const char* to_string(Level l) noexcept
{
    switch (l) {
        case Level::Debug   : return "\033[90mDEBUG\033[0m";    // Bright black (gray)
        case Level::Event   : return "\033[96mEVENT\033[0m";    // Bright cyan
        case Level::Info    : return "\033[92mINFO\033[0m";     // Bright green
        case Level::Warning : return "\033[93mWARN\033[0m";     // Bright yellow
        case Level::Error   : return "\033[91mERROR\033[0m";    // Bright red
        case Level::Eureka  : return "\033[95mEUREKA\033[0m";   // Bright magenta (a flash of discovery)
    }
    return "\033[97mUNKNOWN\033[0m";                            // Bright white
}

inline Level& min_level()
{
    static Level lvl = Level::Debug;
    return lvl;
}

/* simple ASCII-only, case-insensitive compare */
inline bool ieq(const char* a, const char* b) noexcept
{
    while (*a && *b) {
        if (std::tolower(static_cast<unsigned char>(*a)) !=
            std::tolower(static_cast<unsigned char>(*b)))
            return false;
        ++a; ++b;
    }
    return *a == *b;
}

inline Level parse_level(const char* s) noexcept
{
    if (ieq(s, "debug"))            return Level::Debug;
    if (ieq(s, "event"))            return Level::Event;
    if (ieq(s, "info"))             return Level::Info;
    if (ieq(s, "warn") || ieq(s,"warning"))
                                    return Level::Warning;
    if (ieq(s, "error"))            return Level::Error;
    if (ieq(s, "eureka"))           return Level::Eureka;
    return Level::Debug;            /* default fallback */
}

inline void set_min_level(const char* s) noexcept
{
    min_level() = parse_level(s);
}

inline void log(Level lvl, const char* fmt, ...)
{
    if (lvl < min_level()) return;

    char ts[20];
    std::time_t now = std::time(nullptr);
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    std::fprintf(stdout, "[%s] %s: ", ts, to_string(lvl));
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stdout, fmt, ap);
    va_end(ap);
    std::fflush(stdout);
}

} // namespace du

#define LOG_DEBUG(...)   ::du::log(::du::Level::Debug  , __VA_ARGS__)
#define LOG_EVENT(...)   ::du::log(::du::Level::Event  , __VA_ARGS__)
#define LOG_INFO(...)    ::du::log(::du::Level::Info   , __VA_ARGS__)
#define LOG_WARN(...)    ::du::log(::du::Level::Warning, __VA_ARGS__)
#define LOG_ERROR(...)   ::du::log(::du::Level::Error  , __VA_ARGS__)
#define LOG_EUREKA(...)  ::du::log(::du::Level::Eureka , __VA_ARGS__)
