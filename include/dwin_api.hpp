/* dwin_api.hpp — simplified version, GPU-free (PrintWindow/BitBlt) */
#pragma once
#include "dconfig.hpp"
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <memory>
#include <shellscalingapi.h> // link to Shcore.lib
#include "dlog.hpp"      /* LOG_*    */

namespace dw {

/*────────────────────────────── misc helpers ───────────────────────────────*/
inline std::string last_error(DWORD code = ::GetLastError())
{
    LPSTR buf{};
    ::FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                     FORMAT_MESSAGE_IGNORE_INSERTS,
                     nullptr, code, 0, (LPSTR)&buf, 0, nullptr);
    std::string s = buf ? buf : "unknown";
    if (buf) ::LocalFree(buf);
    return s;
}
inline std::wstring to_wstring(std::string_view s)
{
    if (s.empty()) return {};
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}
inline std::string to_utf8(std::wstring_view w)
{
    if (w.empty()) return {};
    int n = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr,nullptr);
    std::string s(n, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr,nullptr);
    return s;
}

/*──────────────────────── window discovery helpers ─────────────────────────*/
namespace detail {
struct Search { std::wstring needle; bool partial; HWND result{nullptr}; };
inline BOOL CALLBACK enum_proc(HWND h, LPARAM p)
{
    auto* s = reinterpret_cast<Search*>(p);
    wchar_t buf[256]{}; ::GetWindowTextW(h, buf, 256);
    if ((s->partial && wcsstr(buf, s->needle.c_str())) || (!s->partial && s->needle == buf))
    { s->result = h; return FALSE; }  /* stop */
    return TRUE;                       /* continue */
}
}  // namespace detail

inline HWND find_window(std::wstring_view title, bool partial=false)
{ detail::Search s{std::wstring(title),partial}; ::EnumWindows(detail::enum_proc,(LPARAM)&s); return s.result; }
inline HWND find_window_utf8(std::string_view t,bool p=false){return find_window(to_wstring(t),p);}                
inline std::string get_window_title(HWND h){wchar_t buf[256]{};::GetWindowTextW(h,buf,256);return to_utf8(buf);}  

/*──────────────────────────── DPI helpers ──────────────────────────────────*/
inline double inv_scale(){static double v=1.0/CFG_DBL("screen_dpi_scale",1.0);return v;}
inline void   adjust_dpi(int&x,int&y){x=int(x*inv_scale());y=int(y*inv_scale());}

/*──────────────────── mouse / keyboard helpers ───────────────────────────*/
inline void mouse_down(HWND h, int x, int y){dw::adjust_dpi(x, y);LPARAM lp = MAKELPARAM(x, y);::PostMessage(h, WM_LBUTTONDOWN, MK_LBUTTON, lp);}
inline void mouse_up(HWND h, int x, int y){dw::adjust_dpi(x, y);LPARAM lp = MAKELPARAM(x, y);::PostMessage(h, WM_LBUTTONUP, 0, lp);}
inline void click(HWND h,int x,int y){adjust_dpi(x,y);LPARAM lp=MAKELPARAM(x,y);::PostMessage(h,WM_LBUTTONDOWN,MK_LBUTTON,lp);::PostMessage(h,WM_LBUTTONUP,0,lp);}    
inline void dbl_click(HWND h,int x,int y){click(h,x,y);::Sleep(60);click(h,x,y);}                                                
inline void move_cursor(HWND h,int x,int y){adjust_dpi(x,y);POINT p{x,y};::ClientToScreen(h,&p);::SetCursorPos(p.x,p.y);}          
inline void send_key(HWND h,WORD vk,bool ctrl=false){if(ctrl)::PostMessage(h,WM_KEYDOWN,VK_CONTROL,0);::PostMessage(h,WM_KEYDOWN,vk,0);::PostMessage(h,WM_KEYUP,vk,0);if(ctrl)::PostMessage(h,WM_KEYUP,VK_CONTROL,0);} 
inline void send_text(HWND h,std::string_view s,int d=35){for(char c:s){::PostMessage(h,WM_CHAR,(WPARAM)(unsigned char)c,0);::Sleep(d);} }
inline void send_text(HWND h,std::wstring_view s,int d=35){for(wchar_t c:s){::PostMessage(h,WM_CHAR,(WPARAM)c,0);::Sleep(d);} }
inline void send_vk(HWND hwnd, std::string_view key) {
    bool ctrl = false;
    WORD vk = 0;

    /* check CTRL+ */
    if(key.substr(0,5) == "CTRL+") {
        ctrl = true;
        key.remove_prefix(5);
    }

    /* named keys */
    if(key=="ENTER") vk = VK_RETURN;
    else if(key=="ESC") vk = VK_ESCAPE;
    else if(key=="TAB") vk = VK_TAB;
    else if(key=="UP") vk = VK_UP;
    else if(key=="DOWN") vk = VK_DOWN;
    else if(key=="LEFT") vk = VK_LEFT;
    else if(key=="RIGHT") vk = VK_RIGHT;
    else if(key.size()==1 && isalpha(key[0])) vk = toupper(key[0]);
    else if(key.size()>2 && key.substr(0,2)=="0x") {
        vk = static_cast<WORD>(std::stoi(std::string(key),nullptr,16));
    }
    else {
        LOG_WARN("unknown VK name: %.*s\n", (int)key.size(), key.data());
        return;
    }

    dw::send_key(hwnd, vk, ctrl);
}
inline void mouse_wheel(HWND hwnd, int x, int y, int delta)
{
    LOG_WARN("mouse_wheel does not seem to work ... \n");
    dw::adjust_dpi(x, y);

    /* move the real mouse pointer */
    POINT p{x, y};
    ::ClientToScreen(hwnd, &p);
    ::SetCursorPos(p.x, p.y);

    /* send global wheel event */
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = delta;
    ::SendInput(1, &input, sizeof(input));
}

/*──────────────────────── clipboard helpers ────────────────────────────────*/
inline void set_clipboard(std::wstring_view w){SIZE_T sz=(w.size()+1)*sizeof(wchar_t);HGLOBAL h=::GlobalAlloc(GMEM_MOVEABLE,sz);if(!h){LOG_ERROR("GlobalAlloc failed\n");return;}memcpy(::GlobalLock(h),w.data(),sz);::GlobalUnlock(h);if(::OpenClipboard(nullptr)){::EmptyClipboard();::SetClipboardData(CF_UNICODETEXT,h);::CloseClipboard();}else{::GlobalFree(h);LOG_WARN("OpenClipboard failed\n");}}
inline void paste(HWND h,std::wstring_view w){set_clipboard(w);send_key(h,'V',true);}                                           

/*──────────────────────── bitmap helpers ───────────────────────────────────*/
namespace detail {
inline bool save_bitmap(const uint8_t* data,size_t stride,int w,int h,const std::filesystem::path& file)
{
    BITMAPFILEHEADER bfh{}; BITMAPINFOHEADER bih{};
    bih.biSize=sizeof(BITMAPINFOHEADER); bih.biWidth=w; bih.biHeight=-h; bih.biPlanes=1; bih.biBitCount=32; bih.biCompression=BI_RGB;
    size_t img=w*h*4; bfh.bfType=0x4D42; bfh.bfOffBits=sizeof(bfh)+sizeof(bih); bfh.bfSize=DWORD(bfh.bfOffBits+img);
    FILE* fp=_wfopen(file.wstring().c_str(),L"wb"); if(!fp){LOG_ERROR("save_bitmap: %s",last_error().c_str());return false;}
    fwrite(&bfh,sizeof(bfh),1,fp); fwrite(&bih,sizeof(bih),1,fp); fwrite(data,img,1,fp); fclose(fp); return true;
}
} // namespace detail

/*──────────────────────────── PrintWindow capture ──────────────────────────*/
inline bool capture_window(HWND hwnd,const std::filesystem::path& file)
{
    if(!::IsWindow(hwnd)){LOG_ERROR("capture_window: invalid HWND\n");return false;}
    if(::IsIconic(hwnd)) ::ShowWindow(hwnd,SW_RESTORE);

    RECT rc{}; ::GetClientRect(hwnd,&rc);
    int w=rc.right, h=rc.bottom; if(w<=0||h<=0){LOG_ERROR("capture_window: zero-sized client\n");return false;}

    HDC wndDC = ::GetDC(hwnd);
    HDC memDC = ::CreateCompatibleDC(wndDC);
    HBITMAP hbm = ::CreateCompatibleBitmap(wndDC,w,h);
    ::SelectObject(memDC,hbm);

    BOOL ok = ::PrintWindow(hwnd, memDC, PW_CLIENTONLY);
    if(!ok){LOG_WARN("PrintWindow failed (%s), falling back to BitBlt\n",last_error().c_str());
        POINT p{0,0}; ::ClientToScreen(hwnd,&p);
        HDC scr = ::GetDC(nullptr);
        ::BitBlt(memDC,0,0,w,h,scr,p.x,p.y,SRCCOPY|CAPTUREBLT);
        ::ReleaseDC(nullptr,scr);
    }

    /* extract bits */
    BITMAPINFO bmi{}; bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); bmi.bmiHeader.biWidth=w; bmi.bmiHeader.biHeight=-h;
    bmi.bmiHeader.biPlanes=1; bmi.bmiHeader.biBitCount=32; bmi.bmiHeader.biCompression=BI_RGB;
    std::vector<uint8_t> buf(w*h*4);
    if(!::GetDIBits(memDC,hbm,0,h,buf.data(),&bmi,DIB_RGB_COLORS)){
        LOG_ERROR("GetDIBits failed: %s\n",last_error().c_str());
        ::DeleteObject(hbm);::DeleteDC(memDC);::ReleaseDC(hwnd,wndDC);
        return false;
    }

    bool saved = detail::save_bitmap(buf.data(), w*4, w, h, file);

    ::DeleteObject(hbm); ::DeleteDC(memDC); ::ReleaseDC(hwnd,wndDC);
    if(!saved) LOG_ERROR("capture_window: save failed\n");
    return saved;
}

/*──────────────────────────── tiny beep ───────────────────────────────────*/
inline void notify() { ::Beep(750, 300); ::Beep(1250, 300); ::Beep(350, 300); }

} // namespace dw