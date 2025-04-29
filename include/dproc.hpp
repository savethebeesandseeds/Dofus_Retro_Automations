/* dproc.hpp – tiny procedure interpreter
* ──────────────────────────────────────────────────────────────────────────*/
#pragma once
#include "dconfig.hpp"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <chrono>
#include <thread>
#include <cctype>
#include <iomanip>

#include "dlog.hpp"
#include "dutils.hpp"       // du::trim / trim_quotes / simplify
#include "dwin_api.hpp"     // dw::* helpers
#include "dscreen_ocr.hpp"  // so::read_region / locate_text

namespace dp {

/*──────────────────── runtime context ────────────────────*/
struct Context {
    HWND hwnd{};
    std::map<std::string, std::string> vars;
    cv::Mat prev;
};

/*──────────────────── function registry ──────────────────*/
using Fn = bool(*)(Context&, const std::vector<std::string>&);

#include "dproc_fn.hpp"     // all the available call_fn methods

/* A constexpr table → O(1) dispatch */
static const std::unordered_map<std::string, Fn> FN_TABLE = {
    {"click_next_item_in_line", &dp_fn::click_next_item_in_line},
    {"read_from_selected_item", &dp_fn::read_from_selected_item}
};

/*──────────────────── helpers ────────────────────────────*/
inline std::optional<RECT> find_phrase_bbox(HWND hwnd,
                                            const std::string& phrase,
                                            double conf = 60)
{
    auto hits = so::locate_text(hwnd, phrase, conf);
    if (hits.empty()) return std::nullopt;
    return hits.front();
}

inline std::optional<RECT> find_phrase_bbox(HWND hwnd,
                                            const RECT& roi,
                                            const std::string& phrase,
                                            double conf = 60)
{
    auto hits = so::locate_text(hwnd, roi, phrase, conf);
    if (hits.empty()) return std::nullopt;
    return hits.front();
}

/* expand $1 … $N ----------------------------------------------------------*/
inline std::string expand_args(std::string_view line,
    const std::vector<std::string>& args)
{
    if (args.empty()) return std::string(line);

    std::string out(line);
    for (size_t k = 0; k < args.size(); ++k) {
        std::string key = '$' + std::to_string(k + 1);
        std::string repl = (args[k] == "random") ? du::random_hex() : args[k];

        size_t pos = 0;
        while ((pos = out.find(key, pos)) != std::string::npos) {
            out.replace(pos, key.size(), repl);
            pos += repl.size();
        }
    }
    return out;
}

/*──────────────────── core interpreter ──────────────────*/
inline bool run_proc(Context& ctx,
                     const std::string&              name,
                     const std::vector<std::string>& args  = {},
                     int                              depth = 0)
{
    /*── entry ───────────────────────────────────────────*/
    LOG_EVENT("[run_proc] run_proc  name='%s'  depth=%d  argc=%zu\n",
              name.c_str(), depth, args.size());

    if (depth > 10) {
        LOG_ERROR("[run_proc] proc recursion too deep – aborting (%s)\n", name.c_str());
        return false;
    }

    std::filesystem::path folder = CFG_STR("procedure_folder", "./procedures");
    std::filesystem::path file   = folder / (name + ".proc");

    LOG_DEBUG("[run_proc] opening file: %s\n", file.string().c_str());
    std::ifstream in(file);
    if (!in) {
        LOG_ERROR("[run_proc] proc not found: %s\n", file.string().c_str());
        return false;
    }

    std::string raw; int lineno = 0;

    /*── main line-reader loop ───────────────────────────*/
    while (std::getline(in, raw))
    {
        ++lineno;
        LOG_DEBUG("[run_proc] [%s:%d] raw: \"%s\"\n",
                  name.c_str(), lineno, raw.c_str());

        /* strip comments / blanks */
        if (auto pos = raw.find('#'); pos != std::string::npos) raw.erase(pos);
        raw = du::trim(raw);
        if (raw.empty()) { LOG_DEBUG("[run_proc] …skipped (blank/comment)\n"); continue; }

        raw = expand_args(raw, args);          // substitute $1…$N
        std::istringstream ss(raw);
        std::string cmd; ss >> cmd;

        LOG_DEBUG("[run_proc] cmd='%s'  reset=\"%s\"\n", cmd.c_str(),
                  raw.substr(cmd.size()).c_str());

    /*──────────── flow / tasks ────────────────────*/
        if (cmd == "loop")
        {
            int times = 0; std::string sub; ss >> times >> sub;

            LOG_EVENT("[run_proc] loop  times=%d  sub='%s'\n", times, sub.c_str());
            if (sub.empty())
                throw std::runtime_error("loop: missing sub-procedure name");
            if (times <= 0) continue;          // nothing to do

            std::vector<std::string> sub_args;
            std::string tok;
            while (ss >> tok) sub_args.push_back(du::trim_quotes(tok));

            for (int i = 0; i < times; ++i) {
                LOG_DEBUG("[run_proc]   ↳ iteration %d/%d\n", i + 1, times);
                if (!run_proc(ctx, sub, sub_args, depth + 1))
                    break;
            }
        }
        else if (cmd == "call_proc") {
            std::string sub; ss >> sub;
            std::vector<std::string> sub_args;
            std::string tok; while (ss >> tok) sub_args.push_back(du::trim_quotes(tok));
            LOG_EVENT("[run_proc] call_proc  sub='%s'  argc=%zu\n", sub.c_str(), sub_args.size());
            if (!run_proc(ctx, sub, sub_args, depth + 1)) return false;
        } else if (cmd == "call_fn") {
            std::string fn;  ss >> fn;
        
            std::vector<std::string> fn_args;
            std::string tok;
            while (ss >> tok) fn_args.push_back(du::trim_quotes(tok));
        
            LOG_EVENT("[run_proc] call_fn  fn='%s'  argc=%zu\n",
                      fn.c_str(), fn_args.size());
        
            auto it = FN_TABLE.find(fn);
            if (it == FN_TABLE.end()) {
                LOG_ERROR("unknown fn: %s\n", fn.c_str());
                return false;
            }
        
            /* invoke; bubble failure up the stack */
            if (!(*it->second)(ctx, fn_args))
                return false;
        }
    /*──────────────── basic mouse / kbd ───────────────*/
        else if (cmd == "click")       { int x,y; ss>>x>>y; LOG_DEBUG("[run_proc] click (%d,%d)\n",x,y); dw::click(ctx.hwnd,x,y); }
        else if (cmd == "dblclick")    { int x,y; ss>>x>>y; LOG_DEBUG("[run_proc] dblclick (%d,%d)\n",x,y); dw::dbl_click(ctx.hwnd,x,y); }
        else if (cmd == "move")        { int x,y; ss>>x>>y; LOG_DEBUG("[run_proc] move (%d,%d)\n",x,y); dw::move_cursor(ctx.hwnd,x,y); }
        else if (cmd == "scroll")      { int x,y,d; ss>>x>>y>>d; LOG_DEBUG("[run_proc] scroll (%d,%d) d=%d\n",x,y,d); if(d) dw::mouse_wheel(ctx.hwnd,x,y,d); }
        else if (cmd == "hold_click")  {
            int x,y,dur; ss>>x>>y>>dur;
            LOG_EVENT("[run_proc] hold_click (%d,%d) dur=%dms\n",x,y,dur);
            if (dur == 0) continue;
            if (dur < 0)  throw std::runtime_error("hold_click duration must be >0");
            dw::mouse_down(ctx.hwnd,x,y); ::Sleep(dur); dw::mouse_up(ctx.hwnd,x,y);
        }
        else if (cmd == "type")        { std::string t; std::getline(ss,t); t=du::trim_quotes(du::trim(t)); LOG_DEBUG("[run_proc] type \"%s\"\n",t.c_str()); dw::send_text(ctx.hwnd,t); }
        else if (cmd == "key")         { std::string k; ss>>k; LOG_DEBUG("[run_proc] key \"%s\"\n",k.c_str()); dw::send_vk(ctx.hwnd,k); }
        else if (cmd == "paste")       { std::string t; std::getline(ss,t); t=du::trim_quotes(du::trim(t)); LOG_DEBUG("[run_proc] paste \"%s\"\n",t.c_str()); dw::paste(ctx.hwnd,dw::to_wstring(t)); }
        else if (cmd == "sleep")       { int ms; ss>>ms; LOG_EVENT("[run_proc] sleep %dms\n",ms); std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

    /*──────────────── CTX helpers ─────────────────────*/
        else if (cmd == "set_prev") {
            LOG_EVENT("[run_proc] set_prev (capture window)\n");
            ctx.prev = so::detail::capture(ctx.hwnd);
        }
        else if (cmd == "set_vars") {
            std::string var,value; ss>>var>>value;
            LOG_EVENT("[run_proc] set_vars  %s = \"%s\"\n",var.c_str(),value.c_str());
            ctx.vars[var]=value;
        }
    /*──────────────── OCR helpers ─────────────────────*/
        else if (cmd == "OCR") {
            int x,y,w,h; std::string _,var; ss>>x>>y>>w>>h>>_>>var;
            LOG_EVENT("[run_proc] OCR  (%d,%d,%d,%d) → %s\n",x,y,w,h,var.c_str());
            RECT rc{x,y,x+w,y+h}; ctx.vars[var]=so::read_region(ctx.hwnd,rc);
        }
        else if (cmd == "OCR_diff") {
            int x,y,w,h; std::string _,var; ss>>x>>y>>w>>h>>_>>var;
            LOG_EVENT("[run_proc] OCR_diff (%d,%d,%d,%d) → %s\n",x,y,w,h,var.c_str());
            RECT rc{x,y,x+w,y+h}; ctx.vars[var]=so::read_region(ctx.hwnd,ctx.prev,rc);
        }
        else if (cmd == "expect_ocr") {
            int x,y,w,h; std::string exp; ss>>x>>y>>w>>h; std::getline(ss,exp);
            exp=du::trim_quotes(du::trim(exp));
            LOG_EVENT("[run_proc] expect_ocr (%d,%d,%d,%d) exp=\"%s\"\n",x,y,w,h,exp.c_str());
            RECT rc{x,y,x+w,y+h};
            std::string txt = so::read_region(ctx.hwnd, rc);
            if (du::simplify(txt).find(du::simplify(exp)) == std::string::npos)
                throw std::runtime_error("EXPECT_OCR failed. exp='"+exp+"' got='"+txt+"'");
        }
        else if (cmd == "ocr_break") { /* …same pattern, shortened for brevity */ 
            int x,y,w,h; std::string exp; ss>>x>>y>>w>>h; std::getline(ss,exp);
            exp=du::trim_quotes(du::trim(exp));
            LOG_DEBUG("[run_proc] ocr_break (%d,%d,%d,%d) exp=\"%s\"\n",x,y,w,h,exp.c_str());
            RECT rc{x,y,x+w,y+h};
            std::string txt = so::read_region(ctx.hwnd, rc);
            if (du::simplify(txt).find(du::simplify(exp)) != std::string::npos){
                LOG_EVENT("[run_proc] ocr_break break!\n");
                return false;
            }
            LOG_EVENT("[run_proc] ocr_break NO break!\n");
        }
        else if (cmd == "ocr_stop") {
            int x,y,w,h; std::string exp; ss>>x>>y>>w>>h; std::getline(ss,exp);
            exp=du::trim_quotes(du::trim(exp));
            LOG_DEBUG("[run_proc] ocr_stop (%d,%d,%d,%d) exp=\"%s\"\n",x,y,w,h,exp.c_str());
            RECT rc{x,y,x+w,y+h};
            std::string txt = so::read_region(ctx.hwnd, rc);
            if (du::simplify(txt).find(du::simplify(exp)) != std::string::npos){
                LOG_EVENT("[run_proc] ocr_stop stop!\n");
                break;
            }
            LOG_EVENT("[run_proc] ocr_stop NO stop!\n");
        }

        /*──────────── diff-based break/stop ───────────*/
        else if (cmd == "break_if_no_diff") {
            int x,y,w,h; ss>>x>>y>>w>>h;
            RECT rc{x,y,x+w,y+h};
            double c = so::compare_imag(ctx.hwnd, ctx.prev, rc);
            LOG_DEBUG("[run_proc] break_if_no_diff cmp=%f\n", c);
            if (c > CFG_DBL("diff_comparison_humbral", 0.5)) {
                LOG_EVENT("[run_proc] break_if_no_diff break!\n");
                return false;
            }
            LOG_EVENT("[run_proc] break_if_no_diff NO break!\n");
        }
        else if (cmd == "stop_if_no_diff") {
            int x,y,w,h; ss>>x>>y>>w>>h;
            RECT rc{x,y,x+w,y+h};
            double c = so::compare_imag(ctx.hwnd, ctx.prev, rc);
            LOG_DEBUG("[run_proc] stop_if_no_diff cmp=%f\n", c);
            if (c > CFG_DBL("diff_comparison_humbral", 0.5)){
                LOG_EVENT("[run_proc] stop_if_no_diff stop!\n");
                break;
            }
            LOG_EVENT("[run_proc] stop_if_no_diff NO stop!\n");
        }
    /*──────── phrase helpers ───────────*/
        else if (cmd == "click_phrase") {
            std::string p; std::getline(ss,p); p=du::trim_quotes(du::trim(p));
            LOG_EVENT("[run_proc] click_phrase \"%s\"\n",p.c_str());
            if (auto rc = find_phrase_bbox(ctx.hwnd,p))
                dw::click(ctx.hwnd,(rc->left+rc->right)/2,(rc->top+rc->bottom)/2);
            else throw std::runtime_error("click_phrase: not found '"+p+"'");
        }
        else if (cmd == "wait_phrase") {
            std::string p; int to; ss>>std::quoted(p)>>to;
            LOG_EVENT("[run_proc] wait_phrase \"%s\"  timeout=%dms\n",p.c_str(),to);
            auto start = std::chrono::steady_clock::now();
            while (std::chrono::steady_clock::now()-start < std::chrono::milliseconds(to)) {
                if (find_phrase_bbox(ctx.hwnd,p)) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }
    /*──────── rect-aware phrase helpers ───────────*/
        else if (cmd == "click_phrase_rect") {
            int x,y,w,h; ss>>x>>y>>w>>h;
            std::string p; std::getline(ss,p); p=du::trim_quotes(du::trim(p));
            LOG_EVENT("[run_proc] click_phrase_rect \"%s\"  roi=(%d,%d,%d,%d)\n",
                      p.c_str(),x,y,w,h);
            RECT roi{x,y,x+w,y+h};
            if (auto rc = find_phrase_bbox(ctx.hwnd, roi, p))
                dw::click(ctx.hwnd,(rc->left+rc->right)/2,(rc->top+rc->bottom)/2);
            else throw std::runtime_error("click_phrase_rect: not found '"+p+"'");
        }
        else if (cmd == "wait_phrase_rect") {
            int x,y,w,h; ss>>x>>y>>w>>h;
            std::string p; int to; ss>>std::quoted(p)>>to;
            LOG_EVENT("[run_proc] wait_phrase_rect \"%s\" roi=(%d,%d,%d,%d) timeout=%dms\n",
                      p.c_str(),x,y,w,h,to);
            RECT roi{x,y,x+w,y+h};
            auto start = std::chrono::steady_clock::now();
            while (std::chrono::steady_clock::now()-start < std::chrono::milliseconds(to)) {
                if (find_phrase_bbox(ctx.hwnd, roi, p)) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }

    /*──────── save all vars ───────────*/
        else if (cmd == "save") {
            std::string fname; int reset = 0; ss>>std::quoted(fname); if(!(ss>>reset)) reset=0;
            LOG_EVENT("[run_proc] save  \"%s\"  reset=%d  vars=%zu\n", fname.c_str(), reset, ctx.vars.size());

            std::ofstream out(fname);
            if(!out) throw std::runtime_error("save: cannot open '"+fname+"'");
            out<<"{\n";
            size_t n=0, total=ctx.vars.size();
            for(auto&[k,v]:ctx.vars){
                out<<"  \""<<du::jesc(k)<<"\" : \""<<du::jesc(v)<<"\"";
                if(++n<total) out<<',';
                out<<"\n";
                /*─── NEW: also log each variable ───*/
                LOG_EVENT("[run_proc] saved  \"%s\" = \"%s\"\n", k.c_str(), v.c_str());
            }
            out<<"}\n";
            if(reset) ctx.vars.clear();
        }

    /*──────────── Default ────────────────────*/
        else {
            LOG_ERROR("[run_proc] unknown command @%d : %s\n", lineno, cmd.c_str());
            throw std::runtime_error("unknown command @" + std::to_string(lineno) + cmd + "\n");
            return false;
        }
    }

    LOG_EVENT("[run_proc] proc '%s' completed successfully\n", name.c_str());
    return true;
}

} // namespace dp
