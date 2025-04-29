/* dutils.hpp */
#pragma once
/*─────────────────────────────────────────────────────────────────────────────
 *  tiny utilities: logger, timers, string helpers
 *────────────────────────────────────────────────────────────────────────────*/
#include "dconfig.hpp"
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <random>
#include <sstream>
#include <iomanip>
#include <limits>
#include "dlog.hpp"

#ifdef _OPENMP
#include <omp.h>
#else
inline int omp_get_thread_num() { return 0; }
#endif

namespace du {


/*───────────────────────────────  timers  ──────────────────────────────────*/
namespace detail {
using Clock = std::chrono::high_resolution_clock;
inline std::unordered_map<std::string, Clock::time_point>& timers()
{
    static std::unordered_map<std::string, Clock::time_point> t;
    static std::mutex mu;
    (void)mu;             // unused when not compiling with -fopenmp
    return t;
}
} // namespace detail

inline void tick(const std::string& label)
{
    detail::timers()[label] = detail::Clock::now();
}

inline void tock(const std::string& label)
{
    auto now = detail::Clock::now();
    auto& tm = detail::timers();
    auto it  = tm.find(label);
    if (it == tm.end()) return;

    std::chrono::duration<double> d = now - it->second;
    LOG_DEBUG("Elapsed [%s] : %.6f s\n", label.c_str(), d.count());
    tm.erase(it);
}

/* RAII stopwatch – simply declare inside a scope                  */
class StopWatch {
public:
    explicit StopWatch(std::string id)
        : id_(std::move(id)), start_(detail::Clock::now()) {}
    ~StopWatch()
    {
        std::chrono::duration<double> d = detail::Clock::now() - start_;
        LOG_DEBUG("Elapsed [%s] : %.6f s\n", id_.c_str(), d.count());
    }
private:
    std::string        id_;
    detail::Clock::time_point start_;
};

/*─────────────────────────  string helpers  ────────────────────────────────*/
template <class CharT, class Traits = std::char_traits<CharT>,
          class Alloc = std::allocator<CharT>>
double extract_decimal(const std::basic_string<CharT,Traits,Alloc>& s)
{
    std::basic_string<CharT,Traits,Alloc> num;
    bool dot = false;
    for (CharT c : s) {
        if (::isdigit(static_cast<unsigned char>(c)))
            num.push_back(c);
        else if (c == static_cast<CharT>('.') && !dot) {
            num.push_back(c);
            dot = true;
        }
    }
    if (num.empty()) return 0.0;

    if constexpr (std::is_same_v<CharT, char>)
        return std::stod(num);
    else {
        /* convert wide string to narrow for stod */
        std::string narrow(num.begin(), num.end());
        return std::stod(narrow);
    }
}

template <class Str>
Str remove_line_breaks(Str s)
{
    s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
    return s;
}


/* delete single file */
void DeleteSingleFile(const char *directoryPath, const char* fileName) {
    char filePath[MAX_PATH];
    snprintf(filePath, sizeof(filePath), "%s\\%s", directoryPath, fileName);
    /* Delete the file */
    if (!DeleteFile(filePath)) {
        LOG_ERROR("Failed to delete file %s (%lu)\n", filePath, GetLastError());
    } else {
        LOG_DEBUG("Deleted File: %s\n", filePath);
    }
}


/* Delete all files in a directory */
void DeleteFilesInDirectory(const char* directoryPath) {
    WIN32_FIND_DATA findFileData;
    HANDLE hFind = INVALID_HANDLE_VALUE;

    /* Create a search pattern */
    char searchPath[MAX_PATH];
    snprintf(searchPath, sizeof(searchPath), "%s\\*.*", directoryPath);

    /* Find the first file in the directory. */
    hFind = FindFirstFile(searchPath, &findFileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Unable to delete files in folder %s : error_nr (%lu)\n", directoryPath, GetLastError());
        return;
    }

    do {
        /* Skip directories, only delete files */
        if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            DeleteSingleFile(directoryPath, findFileData.cFileName);
        }
    } while (FindNextFile(hFind, &findFileData) != 0);

    if (GetLastError() != ERROR_NO_MORE_FILES) {
        LOG_ERROR("FindNextFile failed (%lu)\n", GetLastError());
    }

    FindClose(hFind);
}

inline std::string trim_quotes(std::string_view s) {
    if(s.size()>=2 && ((s.front()=='"' && s.back()=='"') || (s.front()=='\'' && s.back()=='\'')))
        return std::string(s.substr(1, s.size()-2));
    return std::string(s);
}

inline std::string trim(std::string_view s) {
    size_t start = 0, end = s.size();
    while(start < end && isspace((unsigned char)s[start])) ++start;
    while(end > start && isspace((unsigned char)s[end-1])) --end;
    return std::string(s.substr(start, end-start));
}

/*───────────────────────────────────────────────────────────────────────────
 *  simplify – lowercase & de-accent Latin text, keep only [a-z0-9]
 *──────────────────────────────────────────────────────────────────────────*/
inline std::string simplify(std::string_view input)
{
    std::string out;
    out.reserve(input.size());

    const unsigned char* s = reinterpret_cast<const unsigned char*>(input.data());
    size_t i = 0, n = input.size();

    auto push_lower_if_alnum = [&](char ch) {
        if (std::isalnum(static_cast<unsigned char>(ch)))
            out.push_back(std::tolower(static_cast<unsigned char>(ch)));
    };

    while (i < n)
    {
        unsigned char c = s[i];
        uint32_t cp   = 0;      // decoded code-point
        size_t   len  = 0;      // bytes consumed

        /* ───── decode one UTF-8 char ───── */
        if (c < 0x80) { cp = c; len = 1; }                        /* ASCII        */
        else if ((c & 0xE0) == 0xC0 && i + 1 < n) {               /* 2-byte       */
            cp  = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F);
            len = 2;
        }
        else if ((c & 0xF0) == 0xE0 && i + 2 < n) {               /* 3-byte       */
            cp =  ((c & 0x0F) << 12)
                | ((s[i + 1] & 0x3F) << 6)
                |  (s[i + 2] & 0x3F);
            len = 3;
        }
        else if ((c & 0xF8) == 0xF0 && i + 3 < n) {               /* 4-byte (rare)*/
            cp =  ((c & 0x07) << 18)
                | ((s[i + 1] & 0x3F) << 12)
                | ((s[i + 2] & 0x3F) << 6)
                |  (s[i + 3] & 0x3F);
            len = 4;
        }
        else { /* malformed byte – skip */ ++i; continue; }

        i += len;

        /* ───── map to ASCII skeleton ───── */
        switch (cp)
        {
            /* a / e / i / o / u with ANY accent or umlaut */
            case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3: case 0x00C4: case 0x00C5:
            case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3: case 0x00E4: case 0x00E5:
                out.push_back('a'); break;

            case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB:
            case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB:
                out.push_back('e'); break;

            case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF:
            case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF:
                out.push_back('i'); break;

            case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5: case 0x00D6: case 0x00D8:
            case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5: case 0x00F6: case 0x00F8:
                out.push_back('o'); break;

            case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC:
            case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC:
                out.push_back('u'); break;

            /* n with tilde */
            case 0x00D1: case 0x00F1:
                out.push_back('n'); break;

            /* c with cedilla */
            case 0x00C7: case 0x00E7:
                out.push_back('c'); break;

            /* ß → ss */
            case 0x00DF:
                out.append("ss"); break;

            /* æ / Æ */
            case 0x00E6: case 0x00C6:
                out.append("ae"); break;

            /* œ / Œ */
            case 0x0153: case 0x0152:
                out.append("oe"); break;

            default:
                if (cp < 128)                       /* any plain ASCII */
                    push_lower_if_alnum(static_cast<char>(cp));
                /* else: non-Latin → ignore */
                break;
        }
    }
    return out;
}

/* JSON escape – no deps */
inline std::string jesc(std::string_view s){
    std::string o; o.reserve(s.size()+4);
    for(unsigned char ch: s){
        switch(ch){
            case '\\':o+="\\\\";break; case '"':o+="\\\"";break;
            case '\b':o+="\\b";break; case '\f':o+="\\f";break;
            case '\n':o+="\\n";break; case '\r':o+="\\r";break;
            case '\t':o+="\\t";break;
            default:
                if(ch<32){o+="\\u00"; o+="0123456789ABCDEF"[ch>>4];
                                   o+="0123456789ABCDEF"[ch&15];}
                else o.push_back(char(ch));
        }}
    return o;
}

inline std::string random_hex(std::size_t nbytes = 8)  // 8 bytes → 16 hex chars
{
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dist(0, std::numeric_limits<uint64_t>::max());

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    uint64_t v = dist(rng);
    for (std::size_t i = 0; i < nbytes; ++i)
        oss << std::setw(2) << ((v >> (i * 8)) & 0xff);

    return oss.str();
}

} // namespace du

