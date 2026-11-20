// RocketDock Modern 2.0 - lanceur d'applications (dock) pour Windows 10/11.
// Win32 + GDI+, un seul .exe sans dépendance. Compatible avec le Settings.ini de RocketDock.
//
// Architecture :
//   - fenêtre "layered" (alpha par pixel) redessinée à chaque image via UpdateLayeredWindow ;
//   - dock_math.h : calcul du zoom / de la disposition (testé séparément) ;
//   - tout est piloté par un timer unique (Tick) : survol, zoom, masquage auto, animations.
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <algorithm>
#include <initializer_list>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <commoncontrols.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <mmsystem.h>

#include "dock_math.h"

using namespace Gdiplus;

// ---------------------------------------------------------------------------------------------
// Textes (français / anglais selon la langue de l'interface Windows)
// ---------------------------------------------------------------------------------------------
static bool g_fr = false;
#define T(fr, en) (g_fr ? (fr) : (en))

static const wchar_t* kAppName = L"RocketDock Modern";
static const wchar_t* kClass = L"RocketDockModernWnd";
static const wchar_t* kSecMain = L"Software\\RocketDock";
static const wchar_t* kSecIcons = L"Software\\RocketDock\\Icons";

enum Side { SIDE_TOP = 0, SIDE_LEFT = 1, SIDE_BOTTOM = 2, SIDE_RIGHT = 3 };

// ---------------------------------------------------------------------------------------------
// Utilitaires
// ---------------------------------------------------------------------------------------------
static std::wstring ExpandEnv(const std::wstring& s) {
    if (s.find(L'%') == std::wstring::npos) return s;
    wchar_t buf[4096];
    DWORD n = ExpandEnvironmentStringsW(s.c_str(), buf, 4096);
    return (n > 0 && n < 4096) ? std::wstring(buf) : s;
}
static std::wstring Lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
    return s;
}
static std::wstring Trim(const std::wstring& s) {
    size_t a = s.find_first_not_of(L" \t\r\n");
    if (a == std::wstring::npos) return L"";
    size_t b = s.find_last_not_of(L" \t\r\n");
    return s.substr(a, b - a + 1);
}
static bool StartsWith(const std::wstring& s, const wchar_t* p) { return s.rfind(p, 0) == 0; }
static std::wstring ExeDir() {
    wchar_t b[MAX_PATH * 2] = {};
    GetModuleFileNameW(nullptr, b, MAX_PATH * 2);
    PathRemoveFileSpecW(b);
    return b;
}
static std::wstring ExtOf(const std::wstring& path) {
    const wchar_t* e = PathFindExtensionW(path.c_str());
    return Lower(e ? e : L"");
}
static std::wstring ResolvePath(const std::wstring& raw) {
    std::wstring p = ExpandEnv(Trim(raw));
    if (p.empty() || p[0] == L'[' || StartsWith(Lower(p), L"shell:")) return p;
    if (PathIsRelativeW(p.c_str())) {
        wchar_t out[MAX_PATH * 2];
        PathCombineW(out, ExeDir().c_str(), p.c_str());
        return out;
    }
    return p;
}
static unsigned HexColor(const std::wstring& s, unsigned def) {
    if (s.size() != 6) return def;
    wchar_t* end = nullptr;
    unsigned v = (unsigned)wcstoul(s.c_str(), &end, 16);
    return (end && *end == 0) ? v : def;
}

// ---------------------------------------------------------------------------------------------
// Réglages et fichier INI (même format que RocketDock)
// ---------------------------------------------------------------------------------------------
struct Settings {
    int side = SIDE_BOTTOM;
    int offset = 0;
    int monitor = 0;
    int iconMin = 44;
    int iconMax = 80;
    int zoomWidth = 3;
    bool autoHide = false;
    int autoHideDelay = 400;
    bool hideLabels = false;
    bool lockIcons = false;
    bool showRunning = true;
    bool openRunning = true;
    int themeOpacity = 100;
    int iconOpacity = 100;
    std::wstring theme = L"Dark";
    std::wstring hotkey = L"Control+Alt+R";
    std::wstring fontName = L"Segoe UI";
};

static std::wstring g_ini;

static std::wstring IniGet(const wchar_t* sec, const wchar_t* key, const wchar_t* def) {
    static wchar_t buf[8192];
    GetPrivateProfileStringW(sec, key, def, buf, 8192, g_ini.c_str());
    return buf;
}
static int IniInt(const wchar_t* sec, const wchar_t* key, int def) {
    return (int)GetPrivateProfileIntW(sec, key, def, g_ini.c_str());
}
static void IniPut(const wchar_t* sec, const wchar_t* key, const std::wstring& val) {
    WritePrivateProfileStringW(sec, key, val.c_str(), g_ini.c_str());
}
static void IniPutInt(const wchar_t* sec, const wchar_t* key, int val) { IniPut(sec, key, std::to_wstring(val)); }

static const wchar_t* SideName(int s) {
    switch (s) {
        case SIDE_TOP: return L"Top";
        case SIDE_LEFT: return L"Left";
        case SIDE_RIGHT: return L"Right";
        default: return L"Bottom";
    }
}
static int SideFromName(const std::wstring& n) {
    std::wstring l = Lower(n);
    if (l == L"top") return SIDE_TOP;
    if (l == L"left") return SIDE_LEFT;
    if (l == L"right") return SIDE_RIGHT;
    return SIDE_BOTTOM;
}

// ---------------------------------------------------------------------------------------------
// Thèmes
// ---------------------------------------------------------------------------------------------
struct Theme {
    Color barTop, barBot, border1, border2, sheen, accent, labelBg, labelFg, labelBorder, hl, divider;
    BYTE shadowA = 70;
    bool dark = true;
};
static Theme MakeTheme(const std::wstring& name) {
    Theme t;
    std::wstring n = Lower(name);
    if (n == L"light") {
        t.dark = false;
        t.barTop = Color(222, 255, 255, 255);
        t.barBot = Color(210, 232, 236, 245);
        t.border1 = Color(190, 255, 255, 255);
        t.border2 = Color(70, 120, 130, 150);
        t.sheen = Color(70, 255, 255, 255);
        t.accent = Color(255, 0, 110, 210);
        t.labelBg = Color(240, 252, 252, 254);
        t.labelFg = Color(255, 28, 30, 36);
        t.labelBorder = Color(60, 0, 0, 0);
        t.hl = Color(46, 0, 0, 0);
        t.divider = Color(70, 0, 0, 0);
        t.shadowA = 52;
    } else if (n == L"aurora") {
        t.barTop = Color(214, 70, 56, 128);
        t.barBot = Color(224, 22, 28, 78);
        t.border1 = Color(110, 200, 190, 255);
        t.border2 = Color(30, 160, 170, 255);
        t.sheen = Color(40, 255, 255, 255);
        t.accent = Color(255, 255, 150, 210);
        t.labelBg = Color(238, 26, 22, 62);
        t.labelFg = Color(255, 255, 255, 255);
        t.labelBorder = Color(70, 200, 190, 255);
        t.hl = Color(40, 255, 255, 255);
        t.divider = Color(80, 255, 255, 255);
        t.shadowA = 78;
    } else {  // Dark
        t.barTop = Color(210, 46, 49, 62);
        t.barBot = Color(220, 22, 24, 34);
        t.border1 = Color(100, 255, 255, 255);
        t.border2 = Color(22, 255, 255, 255);
        t.sheen = Color(34, 255, 255, 255);
        t.accent = Color(255, 110, 175, 255);
        t.labelBg = Color(238, 20, 22, 30);
        t.labelFg = Color(255, 255, 255, 255);
        t.labelBorder = Color(60, 255, 255, 255);
        t.hl = Color(34, 255, 255, 255);
        t.divider = Color(70, 255, 255, 255);
        t.shadowA = 76;
    }
    return t;
}

// ---------------------------------------------------------------------------------------------
// Éléments du dock
// ---------------------------------------------------------------------------------------------
struct Item {
    std::wstring title, file, file2, cmd, args, wd;
    int showCmd = 0;
    bool sep = false, ctx = false, openRunning = false;
    std::vector<Bitmap*> lv, lv2;  // pyramides d'images (256, 128, 64)
    bool running = false;
    HWND runWnd = nullptr;
    float size = 0, bounceT = -1.0f, hl = 0.0f;
};

static const wchar_t* kRecycle = L"[RecycleBin]";

static void FreeLevels(std::vector<Bitmap*>& v) {
    for (Bitmap* b : v) delete b;
    v.clear();
}
static void FreeItem(Item& it) {
    FreeLevels(it.lv);
    FreeLevels(it.lv2);
}

// ---------------------------------------------------------------------------------------------
// Chargement d'icônes
// ---------------------------------------------------------------------------------------------
static Bitmap* NewPARGB(int w, int h) { return new Bitmap(w, h, PixelFormat32bppPARGB); }

// Copie une région de 'src' dans un carré 256x256 (ratio conservé, centré).
static Bitmap* Master(Image* src, float sx, float sy, float sw, float sh) {
    Bitmap* m = NewPARGB(256, 256);
    Graphics g(m);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    float k = std::min(256.0f / sw, 256.0f / sh);
    float dw = sw * k, dh = sh * k;
    ImageAttributes ia;
    ia.SetWrapMode(WrapModeTileFlipXY);
    g.DrawImage(src, RectF((256 - dw) / 2, (256 - dh) / 2, dw, dh), sx, sy, sw, sh, UnitPixel, &ia);
    return m;
}

static Bitmap* ScaledCopy(Image* src, int w, int h) {
    Bitmap* b = NewPARGB(w, h);
    Graphics g(b);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    ImageAttributes ia;
    ia.SetWrapMode(WrapModeTileFlipXY);
    g.DrawImage(src, RectF(0, 0, (REAL)w, (REAL)h), 0, 0, (REAL)src->GetWidth(), (REAL)src->GetHeight(),
                UnitPixel, &ia);
    return b;
}

static std::vector<Bitmap*> Pyramid(Bitmap* master) {
    std::vector<Bitmap*> v;
    if (!master) return v;
    v.push_back(master);
    v.push_back(ScaledCopy(master, 128, 128));
    v.push_back(ScaledCopy(v.back(), 64, 64));
    return v;
}

// HICON -> bitmap 256 (gère les icônes sans canal alpha via le masque), recadre si l'icône est petite.
static Bitmap* HiconToMaster(HICON h) {
    if (!h) return nullptr;
    ICONINFO ii = {};
    if (!GetIconInfo(h, &ii)) return nullptr;
    Bitmap* res = nullptr;
    BITMAP bm = {};
    if (ii.hbmColor && GetObjectW(ii.hbmColor, sizeof(bm), &bm) && bm.bmWidth > 0 && bm.bmHeight > 0) {
        int w = bm.bmWidth, hh = bm.bmHeight;
        std::vector<uint32_t> px((size_t)w * hh), mask((size_t)w * hh);
        HDC dc = GetDC(nullptr);
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = -hh;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        GetDIBits(dc, ii.hbmColor, 0, hh, px.data(), &bi, DIB_RGB_COLORS);
        bool hasAlpha = false;
        for (uint32_t p : px) {
            if (p >> 24) { hasAlpha = true; break; }
        }
        if (!hasAlpha && ii.hbmMask) {
            BITMAPINFO bm2 = bi;
            GetDIBits(dc, ii.hbmMask, 0, hh, mask.data(), &bm2, DIB_RGB_COLORS);
            for (size_t i = 0; i < px.size(); ++i)
                px[i] = (px[i] & 0x00FFFFFF) | ((mask[i] & 0x00FFFFFF) ? 0u : 0xFF000000u);
        }
        ReleaseDC(nullptr, dc);
        // Boîte englobante du contenu visible
        int minx = w, miny = hh, maxx = -1, maxy = -1;
        for (int y = 0; y < hh; ++y)
            for (int x = 0; x < w; ++x)
                if ((px[(size_t)y * w + x] >> 24) > 12) {
                    minx = std::min(minx, x); maxx = std::max(maxx, x);
                    miny = std::min(miny, y); maxy = std::max(maxy, y);
                }
        if (maxx >= minx && maxy >= miny) {
            Bitmap tmp(w, hh, w * 4, PixelFormat32bppARGB, (BYTE*)px.data());
            int cw = maxx - minx + 1, ch = maxy - miny + 1;
            // On ne recadre que si le contenu occupe nettement moins que l'image (icône 32 px dans un cadre 256)
            if (cw < w * 0.8f && ch < hh * 0.8f) res = Master(&tmp, (float)minx, (float)miny, (float)cw, (float)ch);
            else res = Master(&tmp, 0, 0, (float)w, (float)hh);
        }
    }
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    return res;
}

typedef HRESULT(WINAPI* PFN_SHGetImageList)(int, REFIID, void**);
// IID_IImageList défini localement (évite une dépendance à une bibliothèque d'IID)
static const GUID kIID_IImageList = {0x46EB5926, 0x582E, 0x4017, {0x9F, 0xDF, 0xE8, 0x99, 0x8D, 0xAA, 0x09, 0x50}};

static Bitmap* ShellIcon(const std::wstring& path) {
    SHFILEINFOW sfi = {};
    if (!SHGetFileInfoW(path.c_str(), 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX)) return nullptr;
    static PFN_SHGetImageList fn =
        (PFN_SHGetImageList)GetProcAddress(GetModuleHandleW(L"shell32.dll"), MAKEINTRESOURCEA(727));
    Bitmap* res = nullptr;
    if (fn) {
        const int sizes[] = {4 /*SHIL_JUMBO*/, 2 /*SHIL_EXTRALARGE*/};
        for (int s : sizes) {
            IImageList* il = nullptr;
            if (SUCCEEDED(fn(s, kIID_IImageList, (void**)&il)) && il) {
                HICON hi = nullptr;
                if (SUCCEEDED(il->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hi)) && hi) {
                    res = HiconToMaster(hi);
                    DestroyIcon(hi);
                }
                il->Release();
            }
            if (res) break;
        }
    }
    if (!res) {
        SHFILEINFOW s2 = {};
        if (SHGetFileInfoW(path.c_str(), 0, &s2, sizeof(s2), SHGFI_ICON | SHGFI_LARGEICON) && s2.hIcon) {
            res = HiconToMaster(s2.hIcon);
            DestroyIcon(s2.hIcon);
        }
    }
    return res;
}

static bool FontAvailable(const wchar_t* name) {
    FontFamily ff(name);
    return ff.IsAvailable();
}

static void RRPath(GraphicsPath& p, const RectF& r, float rad) {
    float d = std::min(rad * 2.0f, std::min(r.Width, r.Height));
    if (d <= 0.5f) { p.AddRectangle(r); return; }
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    p.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    p.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    p.CloseFigure();
}

// Tuile moderne : carré arrondi dégradé + glyphe (police d'icônes Segoe) ou lettre.
static Bitmap* MakeTile(const std::wstring& glyph, bool badge, unsigned c1, unsigned c2, const wchar_t* fontName,
                        bool bold) {
    Bitmap* b = NewPARGB(256, 256);
    Graphics g(b);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);
    RectF r(10, 10, 236, 236);
    GraphicsPath path;
    RRPath(path, r, 56);
    LinearGradientBrush grad(PointF(10, 10), PointF(246, 246),
                             Color(255, (c1 >> 16) & 255, (c1 >> 8) & 255, c1 & 255),
                             Color(255, (c2 >> 16) & 255, (c2 >> 8) & 255, c2 & 255));
    g.FillPath(&grad, &path);
    // reflet haut
    g.SetClip(&path);
    LinearGradientBrush sheen(RectF(10, 10, 236, 120), Color(70, 255, 255, 255), Color(0, 255, 255, 255),
                              LinearGradientModeVertical);
    g.FillRectangle(&sheen, RectF(10, 10, 236, 120));
    g.ResetClip();
    Pen edge(Color(70, 255, 255, 255), 3.0f);
    g.DrawPath(&edge, &path);
    // glyphe
    FontFamily ff(fontName);
    if (ff.IsAvailable()) {
        Font f(&ff, 128, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);
        RectF rr(10, 14, 236, 236);
        SolidBrush shadow(Color(60, 0, 0, 0));
        g.DrawString(glyph.c_str(), -1, &f, RectF(rr.X, rr.Y + 5, rr.Width, rr.Height), &sf, &shadow);
        SolidBrush white(Color(255, 255, 255, 255));
        g.DrawString(glyph.c_str(), -1, &f, rr, &sf, &white);
    }
    if (badge) {
        SolidBrush red(Color(255, 255, 90, 70));
        g.FillEllipse(&red, RectF(172, 22, 56, 56));
        Pen ring(Color(255, 255, 255, 255), 5.0f);
        g.DrawEllipse(&ring, RectF(172, 22, 56, 56));
    }
    return b;
}

static Bitmap* LetterTile(const std::wstring& title) {
    static const unsigned pal[][2] = {{0x4F8DF7, 0x2B55D6}, {0xF2994A, 0xE0573A}, {0x34C38F, 0x1B8F70},
                                      {0xB36CF0, 0x7A3FD1}, {0xEB5A8C, 0xC2336A}, {0x4CC3E6, 0x2A8CB8}};
    wchar_t ch = title.empty() ? L'?' : (wchar_t)towupper(title[0]);
    size_t hsh = 0;
    for (wchar_t c : title) hsh = hsh * 31 + (size_t)c;
    const unsigned* p = pal[hsh % 6];
    std::wstring s(1, ch);
    return MakeTile(s, false, p[0], p[1], L"Segoe UI", true);
}

// glyph:E8B7[:RRGGBB[:RRGGBB]][!]
static Bitmap* GlyphTile(const std::wstring& specIn) {
    std::wstring spec = specIn.substr(6);
    bool badge = false;
    if (!spec.empty() && spec.back() == L'!') { badge = true; spec.pop_back(); }
    std::vector<std::wstring> parts;
    size_t st = 0;
    for (;;) {
        size_t c = spec.find(L':', st);
        parts.push_back(spec.substr(st, c == std::wstring::npos ? c : c - st));
        if (c == std::wstring::npos) break;
        st = c + 1;
    }
    unsigned code = (unsigned)wcstoul(parts[0].c_str(), nullptr, 16);
    unsigned c1 = parts.size() > 1 ? HexColor(parts[1], 0x4F8DF7) : 0x4F8DF7;
    unsigned c2 = parts.size() > 2 ? HexColor(parts[2], 0x2B55D6) : 0x2B55D6;
    static const wchar_t* font = FontAvailable(L"Segoe Fluent Icons") ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets";
    std::wstring g(1, (wchar_t)code);
    if (!FontAvailable(font)) return LetterTile(L"?");
    return MakeTile(g, badge, c1, c2, font, false);
}

static std::vector<Bitmap*> LoadImageSpec(const std::wstring& specIn, const std::wstring& cmd,
                                          const std::wstring& title) {
    std::wstring spec = Trim(specIn);
    Bitmap* m = nullptr;
    if (StartsWith(Lower(spec), L"glyph:")) {
        m = GlyphTile(spec);
    } else {
        std::wstring path = spec.empty() ? ResolvePath(cmd) : ResolvePath(spec);
        if (spec.empty() && cmd == kRecycle) {
            m = GlyphTile(L"glyph:E74D:7C8BA3:4B5870");
        } else if (!path.empty() && path[0] != L'[' && !StartsWith(Lower(path), L"shell:") && PathFileExistsW(path.c_str())) {
            std::wstring ext = ExtOf(path);
            if (ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".bmp" || ext == L".gif" ||
                ext == L".tif" || ext == L".tiff") {
                Bitmap src(path.c_str());
                if (src.GetLastStatus() == Ok) m = Master(&src, 0, 0, (float)src.GetWidth(), (float)src.GetHeight());
            } else if (ext == L".ico") {
                HICON hi = (HICON)LoadImageW(nullptr, path.c_str(), IMAGE_ICON, 256, 256, LR_LOADFROMFILE);
                if (hi) { m = HiconToMaster(hi); DestroyIcon(hi); }
            }
            if (!m) m = ShellIcon(path);
        }
    }
    if (!m) m = LetterTile(title.empty() ? cmd : title);
    return Pyramid(m);
}

static void LoadItemImages(Item& it) {
    FreeItem(it);
    if (it.sep) return;
    it.lv = LoadImageSpec(it.file, it.cmd, it.title);
    if (!Trim(it.file2).empty())
        it.lv2 = LoadImageSpec(it.file2, it.cmd, it.title);
    else if (it.cmd == kRecycle && Trim(it.file).empty())
        it.lv2 = LoadImageSpec(L"glyph:E74D:F2A33A:E07B0F!", it.cmd, it.title);
}

// ---------------------------------------------------------------------------------------------
// État global de l'application
// ---------------------------------------------------------------------------------------------
struct Geo {
    float S = 1, s0 = 44, sMax = 80, gap = 7, sepW = 12, padSide = 14, padTop = 9, padBottom = 14;
    float edgeGap = 10, barH = 70, hideDist = 100, margin = 24;
    int W = 600, H = 200, X = 0, Y = 0;
    bool vertical = false;
    float L = 600;  // longueur de la fenêtre le long de l'axe du dock
    dm::Layout base;
    float baseStart = 0;
    RECT mon = {};
};

struct App {
    HINSTANCE hInst = nullptr;
    HWND hwnd = nullptr;
    Settings set;
    std::vector<Item> items;
    Theme th;
    Geo g;
    // surface de dessin
    HDC mem = nullptr;
    HBITMAP dib = nullptr, oldBmp = nullptr;
    void* bits = nullptr;
    Bitmap* surf = nullptr;
    Graphics* gfx = nullptr;
    int dibW = 0, dibH = 0;
    FontFamily* fam = nullptr;
    Font* font = nullptr;
    ImageAttributes* ia = nullptr;
    // dynamique
    float hoverW = 0, hide = 1.0f, idleT = 0, labelA = 0, realStart = 0;
    bool hotPrev = false, forceHidden = false, menuOpen = false, dirty = true;
    dm::Layout real;
    int hoverIdx = -1, labelIdx = -1;
    LARGE_INTEGER freq = {}, last = {};
    int timerMs = 15;
    // glisser
    int pressIdx = -1;
    POINT pressPt = {};
    bool dragging = false;
    // divers
    DWORD lastRunning = 0, lastBin = 0, lastTop = 0;
    bool binFull = false;
    ULONG_PTR gdipToken = 0;
};
static App A;

// ---------------------------------------------------------------------------------------------
// Géométrie
// ---------------------------------------------------------------------------------------------
static float SystemScale() {
    typedef UINT(WINAPI * PFN)();
    PFN fn = (PFN)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForSystem");
    if (fn) return fn() / 96.0f;
    HDC dc = GetDC(nullptr);
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return dpi / 96.0f;
}

static BOOL CALLBACK MonCb(HMONITOR hm, HDC, LPRECT, LPARAM lp) {
    auto* v = (std::vector<MONITORINFO>*)lp;
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hm, &mi)) {
        if (mi.dwFlags & MONITORINFOF_PRIMARY) v->insert(v->begin(), mi);
        else v->push_back(mi);
    }
    return TRUE;
}

static void ToUV(float x, float y, float& u, float& v) {
    switch (A.set.side) {
        case SIDE_TOP: u = x; v = y; break;
        case SIDE_LEFT: u = y; v = x; break;
        case SIDE_RIGHT: u = y; v = (float)A.g.W - x; break;
        default: u = x; v = (float)A.g.H - y; break;
    }
}
static RectF MapRect(float u0, float u1, float v0, float v1) {
    float W = (float)A.g.W, H = (float)A.g.H;
    switch (A.set.side) {
        case SIDE_TOP: return RectF(u0, v0, u1 - u0, v1 - v0);
        case SIDE_LEFT: return RectF(v0, u0, v1 - v0, u1 - u0);
        case SIDE_RIGHT: return RectF(W - v1, u0, v1 - v0, u1 - u0);
        default: return RectF(u0, H - v1, u1 - u0, v1 - v0);
    }
}

static void EnsureSurface() {
    if (A.dib && A.dibW == A.g.W && A.dibH == A.g.H) return;
    delete A.gfx; A.gfx = nullptr;
    delete A.surf; A.surf = nullptr;
    if (A.mem && A.oldBmp) SelectObject(A.mem, A.oldBmp);
    if (A.dib) DeleteObject(A.dib);
    if (!A.mem) A.mem = CreateCompatibleDC(nullptr);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = A.g.W;
    bi.bmiHeader.biHeight = -A.g.H;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    A.dib = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &A.bits, nullptr, 0);
    A.oldBmp = (HBITMAP)SelectObject(A.mem, A.dib);
    A.dibW = A.g.W;
    A.dibH = A.g.H;
    A.surf = new Bitmap(A.g.W, A.g.H, A.g.W * 4, PixelFormat32bppPARGB, (BYTE*)A.bits);
    A.gfx = new Graphics(A.surf);
}

static void BuildFonts() {
    delete A.font; delete A.fam;
    A.fam = new FontFamily(A.set.fontName.c_str());
    if (!A.fam->IsAvailable()) { delete A.fam; A.fam = new FontFamily(L"Segoe UI"); }
    A.font = new Font(A.fam, 13.0f * A.g.S, FontStyleRegular, UnitPixel);
}

static std::vector<bool> SepVec() {
    std::vector<bool> v;
    for (auto& it : A.items) v.push_back(it.sep);
    return v;
}

static void RecalcGeometry() {
    Geo& g = A.g;
    float S = g.S;
    g.vertical = (A.set.side == SIDE_LEFT || A.set.side == SIDE_RIGHT);
    // Moniteur
    std::vector<MONITORINFO> mons;
    EnumDisplayMonitors(nullptr, nullptr, MonCb, (LPARAM)&mons);
    if (mons.empty()) {
        MONITORINFO mi = {};
        mi.cbSize = sizeof(mi);
        mi.rcMonitor = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
        mi.rcWork = mi.rcMonitor;
        mons.push_back(mi);
    }
    const MONITORINFO& mi = mons[(size_t)std::min<int>(std::max(A.set.monitor, 0), (int)mons.size() - 1)];
    g.mon = A.set.autoHide ? mi.rcMonitor : mi.rcWork;
    float monLen = (float)(g.vertical ? (g.mon.bottom - g.mon.top) : (g.mon.right - g.mon.left));
    float monThick = (float)(g.vertical ? (g.mon.right - g.mon.left) : (g.mon.bottom - g.mon.top));

    g.gap = 7 * S; g.sepW = 12 * S; g.padSide = 14 * S; g.padTop = 9 * S; g.padBottom = 14 * S;
    g.edgeGap = 10 * S; g.margin = 24 * S;
    float s0 = (float)A.set.iconMin * S;
    float sMax = std::max((float)A.set.iconMax * S, s0);
    std::vector<bool> sep = SepVec();
    // Réduction automatique si le dock dépasse la largeur de l'écran
    for (int guard = 0; guard < 80; ++guard) {
        dm::Layout b = dm::makeBase(sep, s0, g.sepW, g.gap);
        if (b.total + 2 * g.padSide + 2 * g.margin <= monLen || s0 <= 20 * S) break;
        float k = 0.97f;
        s0 *= k; sMax *= k;
    }
    g.s0 = s0; g.sMax = sMax;
    g.base = dm::makeBase(sep, s0, g.sepW, g.gap);
    g.barH = s0 + g.padTop + g.padBottom;
    int nIcons = 0;
    for (bool b : sep) if (!b) ++nIcons;
    float extra = (float)std::min(nIcons, 2 * A.set.zoomWidth + 1) * (sMax - s0);
    float L = g.base.total + extra + 2 * g.padSide + 2 * g.margin;
    L = std::min(std::max(L, 320.0f * S), monLen);
    float labelArea = g.vertical ? (12 + 240) * S : (12 + 32) * S;
    float T = g.edgeGap + g.padBottom + sMax * 1.55f + g.padTop + labelArea;
    T = std::min(T, monThick);
    g.L = L;
    g.hideDist = g.edgeGap + g.barH + 28 * S;
    g.baseStart = (L - g.base.total) * 0.5f;
    int Li = (int)std::ceil(L), Ti = (int)std::ceil(T);
    g.W = g.vertical ? Ti : Li;
    g.H = g.vertical ? Li : Ti;
    int along = A.set.offset;
    switch (A.set.side) {
        case SIDE_TOP:
            g.X = g.mon.left + ((g.mon.right - g.mon.left) - g.W) / 2 + along; g.Y = g.mon.top; break;
        case SIDE_LEFT:
            g.X = g.mon.left; g.Y = g.mon.top + ((g.mon.bottom - g.mon.top) - g.H) / 2 + along; break;
        case SIDE_RIGHT:
            g.X = g.mon.right - g.W; g.Y = g.mon.top + ((g.mon.bottom - g.mon.top) - g.H) / 2 + along; break;
        default:
            g.X = g.mon.left + ((g.mon.right - g.mon.left) - g.W) / 2 + along; g.Y = g.mon.bottom - g.H; break;
    }
    g.X = std::max<int>(g.mon.left, std::min<int>(g.X, g.mon.right - g.W));
    g.Y = std::max<int>(g.mon.top, std::min<int>(g.Y, g.mon.bottom - g.H));
    for (auto& it : A.items) if (it.size <= 0) it.size = s0;
    BuildFonts();
    EnsureSurface();
    SetWindowPos(A.hwnd, HWND_TOPMOST, g.X, g.Y, g.W, g.H, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    A.dirty = true;
}

// ---------------------------------------------------------------------------------------------
// Rendu
// ---------------------------------------------------------------------------------------------
static Bitmap* PickLevel(const std::vector<Bitmap*>& lv, float px) {
    for (auto it = lv.rbegin(); it != lv.rend(); ++it)
        if ((float)(*it)->GetWidth() >= px) return *it;
    return lv.empty() ? nullptr : lv.front();
}

static Color WithAlpha(const Color& c, float k) {
    return Color((BYTE)dm::clampf(c.GetA() * k, 0, 255), c.GetR(), c.GetG(), c.GetB());
}

static void Present() {
    if (!A.hwnd || !A.mem) return;
    SIZE sz = {A.g.W, A.g.H};
    POINT src = {0, 0};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(A.hwnd, nullptr, nullptr, &sz, A.mem, &src, 0, &bf, ULW_ALPHA);
}

static void Render() {
    if (!A.gfx) return;
    Graphics& g = *A.gfx;
    const Geo& G = A.g;
    const float S = G.S;
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.Clear(Color(0, 0, 0, 0));
    if (A.hide >= 0.995f) { Present(); return; }

    const Theme& th = A.th;
    const float op = A.set.themeOpacity / 100.0f;
    const float vs = -A.hide * G.hideDist;  // décalage de masquage (vers l'extérieur de l'écran)
    float u0 = A.realStart - G.padSide, u1 = A.realStart + A.real.total + G.padSide;
    if (A.items.empty()) { u0 = G.L * 0.5f - 45 * S; u1 = G.L * 0.5f + 45 * S; }  // pastille minimale cliquable
    const float bv0 = G.edgeGap + vs, bv1 = bv0 + G.barH;
    const RectF bar = MapRect(u0, u1, bv0, bv1);
    const float rad = G.barH * 0.34f;

    // Ombre portée douce (couches concentriques)
    for (int i = 14; i >= 1; --i) {
        float e = i * 1.7f * S;
        RectF r(bar.X - e, bar.Y - e + 4 * S, bar.Width + 2 * e, bar.Height + 2 * e);
        GraphicsPath p;
        RRPath(p, r, rad + e);
        float k = 1.0f - (float)i / 15.0f;
        SolidBrush sb(Color((BYTE)(th.shadowA * k * k * 0.30f * op), 0, 0, 0));
        g.FillPath(&sb, &p);
    }
    // Fond "verre"
    GraphicsPath barPath;
    RRPath(barPath, bar, rad);
    LinearGradientBrush bg(RectF(bar.X, bar.Y - 1, bar.Width, bar.Height + 2), WithAlpha(th.barTop, op),
                           WithAlpha(th.barBot, op), LinearGradientModeVertical);
    g.FillPath(&bg, &barPath);
    g.SetClip(&barPath);
    RectF sheenR(bar.X, bar.Y, bar.Width, bar.Height * 0.5f);
    LinearGradientBrush sheen(RectF(sheenR.X, sheenR.Y - 1, sheenR.Width, sheenR.Height + 2), WithAlpha(th.sheen, op),
                              Color(0, th.sheen.GetR(), th.sheen.GetG(), th.sheen.GetB()), LinearGradientModeVertical);
    g.FillRectangle(&sheen, sheenR);
    g.ResetClip();
    LinearGradientBrush edgeBr(RectF(bar.X, bar.Y - 1, bar.Width, bar.Height + 2), WithAlpha(th.border1, op),
                               WithAlpha(th.border2, op), LinearGradientModeVertical);
    Pen edge(&edgeBr, 1.2f * S);
    g.DrawPath(&edge, &barPath);

    // Éléments
    const float baseV = G.edgeGap + G.padBottom + vs;
    for (size_t i = 0; i < A.items.size(); ++i) {
        Item& it = A.items[i];
        float ul = A.realStart + A.real.left[i];
        float uw = A.real.width[i];
        if (it.sep) {
            float cu = ul + uw * 0.5f;
            RectF r = MapRect(cu - 0.8f * S, cu + 0.8f * S, baseV + G.s0 * 0.12f, baseV + G.s0 * 0.88f);
            SolidBrush sb(th.divider);
            g.FillRectangle(&sb, r);
            continue;
        }
        float bounce = 0;
        if (it.bounceT >= 0) {
            float t = it.bounceT / 0.9f;
            bounce = it.size * 0.42f * std::fabs(std::sin(t * 3.14159265f * 3.0f)) * (1.0f - t);
        }
        float v0 = baseV + bounce;
        RectF ir = MapRect(ul, ul + it.size, v0, v0 + it.size);
        // ombre de contact sous l'icône
        {
            RectF sr = MapRect(ul + it.size * 0.14f, ul + it.size * 0.86f, baseV - 3 * S, baseV + 3 * S);
            if (!G.vertical || true) {
                SolidBrush sb(Color((BYTE)(40 * op), 0, 0, 0));
                if (A.set.side == SIDE_BOTTOM || A.set.side == SIDE_TOP) g.FillEllipse(&sb, sr);
            }
        }
        // halo de survol
        if (it.hl > 0.01f) {
            float pad = 5 * S;
            RectF hr(ir.X - pad, ir.Y - pad, ir.Width + 2 * pad, ir.Height + 2 * pad);
            GraphicsPath hp;
            RRPath(hp, hr, it.size * 0.26f);
            SolidBrush hb(WithAlpha(th.hl, it.hl));
            g.FillPath(&hb, &hp);
        }
        // icône
        const std::vector<Bitmap*>& lv = (A.binFull && !it.lv2.empty() && it.cmd == kRecycle) ? it.lv2 : it.lv;
        Bitmap* img = PickLevel(lv, it.size);
        if (img) g.DrawImage(img, ir, 0, 0, (REAL)img->GetWidth(), (REAL)img->GetHeight(), UnitPixel, A.ia);
        // pastille "application ouverte"
        if (it.running && A.set.showRunning) {
            float cu = ul + uw * 0.5f;
            float cv = G.edgeGap + G.padBottom * 0.42f + vs;
            float r = 2.4f * S;
            RectF gl = MapRect(cu - r * 2.4f, cu + r * 2.4f, cv - r * 2.4f, cv + r * 2.4f);
            SolidBrush glow(Color(46, th.accent.GetR(), th.accent.GetG(), th.accent.GetB()));
            g.FillEllipse(&glow, gl);
            RectF dr = MapRect(cu - r, cu + r, cv - r, cv + r);
            SolidBrush dot(th.accent);
            g.FillEllipse(&dot, dr);
        }
    }

    // Étiquette
    if (A.labelA > 0.02f && A.labelIdx >= 0 && A.labelIdx < (int)A.items.size() && !A.set.hideLabels) {
        Item& it = A.items[A.labelIdx];
        if (!it.sep && !it.title.empty()) {
            StringFormat sf(StringFormat::GenericTypographic());
            sf.SetFormatFlags(StringFormatFlagsNoWrap);
            sf.SetTrimming(StringTrimmingEllipsisCharacter);
            sf.SetAlignment(StringAlignmentCenter);
            sf.SetLineAlignment(StringAlignmentCenter);
            RectF box;
            g.MeasureString(it.title.c_str(), -1, A.font, PointF(0, 0), &sf, &box);
            float tw = std::min(box.Width, 260 * S);
            float lw = tw + 24 * S, lh = 26 * S;
            float cu = A.realStart + A.real.left[A.labelIdx] + A.real.width[A.labelIdx] * 0.5f;
            float topV = baseV + it.size + 10 * S;
            float W = (float)G.W, H = (float)G.H;
            RectF lr;
            switch (A.set.side) {
                case SIDE_TOP:
                    lr = RectF(cu - lw / 2, topV, lw, lh); break;
                case SIDE_LEFT:
                    lr = RectF(topV, cu - lh / 2, lw, lh); break;
                case SIDE_RIGHT:
                    lr = RectF(W - topV - lw, cu - lh / 2, lw, lh); break;
                default:
                    lr = RectF(cu - lw / 2, H - topV - lh, lw, lh); break;
            }
            lr.X = dm::clampf(lr.X, 4 * S, std::max(4 * S, W - lw - 4 * S));
            lr.Y = dm::clampf(lr.Y, 4 * S, std::max(4 * S, H - lh - 4 * S));
            float a = A.labelA * (1.0f - A.hide);
            // ombre
            for (int i = 6; i >= 1; --i) {
                float e = i * 1.4f * S;
                RectF sr(lr.X - e, lr.Y - e + 3 * S, lr.Width + 2 * e, lr.Height + 2 * e);
                GraphicsPath sp;
                RRPath(sp, sr, lh / 2 + e);
                SolidBrush sb(Color((BYTE)(10 * a * (1.0f - i / 7.0f)), 0, 0, 0));
                g.FillPath(&sb, &sp);
            }
            GraphicsPath lp;
            RRPath(lp, lr, lh / 2);
            SolidBrush lb(WithAlpha(th.labelBg, a));
            g.FillPath(&lb, &lp);
            Pen lpn(WithAlpha(th.labelBorder, a), 1.0f * S);
            g.DrawPath(&lpn, &lp);
            SolidBrush tb(WithAlpha(th.labelFg, a));
            g.DrawString(it.title.c_str(), -1, A.font, RectF(lr.X + 12 * S, lr.Y, lr.Width - 24 * S, lr.Height), &sf, &tb);
        }
    }
    Present();
}

// ---------------------------------------------------------------------------------------------
// Persistance
// ---------------------------------------------------------------------------------------------
static void SaveSettings() {
    const Settings& s = A.set;
    IniPut(kSecMain, L"Version", L"2.1.0");
    IniPut(kSecMain, L"Position", SideName(s.side));
    IniPutInt(kSecMain, L"Offset", s.offset);
    IniPutInt(kSecMain, L"Monitor", s.monitor);
    IniPutInt(kSecMain, L"IconMin", s.iconMin);
    IniPutInt(kSecMain, L"IconMax", s.iconMax);
    IniPutInt(kSecMain, L"ZoomWidth", s.zoomWidth);
    IniPutInt(kSecMain, L"AutoHide", s.autoHide);
    IniPutInt(kSecMain, L"AutoHideDelay", s.autoHideDelay);
    IniPutInt(kSecMain, L"HideLabels", s.hideLabels);
    IniPutInt(kSecMain, L"LockIcons", s.lockIcons);
    IniPutInt(kSecMain, L"ShowRunning", s.showRunning);
    IniPutInt(kSecMain, L"OpenRunning", s.openRunning);
    IniPutInt(kSecMain, L"ThemeOpacity", s.themeOpacity);
    IniPutInt(kSecMain, L"IconOpacity", s.iconOpacity);
    IniPut(kSecMain, L"Theme", s.theme);
    IniPut(kSecMain, L"HotKey-Toggle", s.hotkey);
    IniPut(kSecMain, L"FontName", s.fontName);
    // Section des icônes : on la reconstruit entièrement
    WritePrivateProfileStringW(kSecIcons, nullptr, nullptr, g_ini.c_str());
    for (size_t i = 0; i < A.items.size(); ++i) {
        const Item& it = A.items[i];
        std::wstring p = std::to_wstring(i) + L"-";
        IniPut(kSecIcons, (p + L"FileName").c_str(), it.file);
        IniPut(kSecIcons, (p + L"FileName2").c_str(), it.file2);
        IniPut(kSecIcons, (p + L"Title").c_str(), it.title);
        IniPut(kSecIcons, (p + L"Command").c_str(), it.cmd);
        IniPut(kSecIcons, (p + L"Arguments").c_str(), it.args);
        IniPut(kSecIcons, (p + L"WorkingDirectory").c_str(), it.wd);
        IniPutInt(kSecIcons, (p + L"ShowCmd").c_str(), it.showCmd);
        IniPutInt(kSecIcons, (p + L"OpenRunning").c_str(), it.openRunning);
        IniPutInt(kSecIcons, (p + L"IsSeparator").c_str(), it.sep);
        IniPutInt(kSecIcons, (p + L"UseContext").c_str(), it.ctx);
    }
    IniPutInt(kSecIcons, L"count", (int)A.items.size());
}

static Item MakeItem(const std::wstring& title, const std::wstring& cmd, const std::wstring& file) {
    Item it;
    it.title = title; it.cmd = cmd; it.file = file;
    return it;
}

static void DefaultItems() {
    A.items.clear();
    A.items.push_back(MakeItem(T(L"Ce PC", L"This PC"), L"shell:MyComputerFolder", L"glyph:E7F4:5B9BFF:2F64D8"));
    A.items.push_back(MakeItem(T(L"Documents", L"Documents"), L"shell:Personal", L"glyph:E8B7:F6B04A:E3801C"));
    A.items.push_back(MakeItem(T(L"Téléchargements", L"Downloads"), L"shell:Downloads", L"glyph:E896:46C9A0:1C9072"));
    Item sp; sp.sep = true; A.items.push_back(sp);
    Item rb = MakeItem(T(L"Corbeille", L"Recycle Bin"), kRecycle, L"");
    rb.ctx = true;
    A.items.push_back(rb);
}

static void LoadSettings() {
    Settings& s = A.set;
    s.side = SideFromName(IniGet(kSecMain, L"Position", L"Bottom"));
    s.offset = IniInt(kSecMain, L"Offset", 0);
    s.monitor = IniInt(kSecMain, L"Monitor", 0);
    s.iconMin = std::min(std::max(IniInt(kSecMain, L"IconMin", 44), 24), 128);
    s.iconMax = std::min(std::max(IniInt(kSecMain, L"IconMax", 80), s.iconMin), 256);
    s.zoomWidth = std::min(std::max(IniInt(kSecMain, L"ZoomWidth", 3), 1), 8);
    s.autoHide = IniInt(kSecMain, L"AutoHide", 0) != 0;
    s.autoHideDelay = std::min(std::max(IniInt(kSecMain, L"AutoHideDelay", 400), 0), 5000);
    s.hideLabels = IniInt(kSecMain, L"HideLabels", 0) != 0;
    s.lockIcons = IniInt(kSecMain, L"LockIcons", 0) != 0;
    s.showRunning = IniInt(kSecMain, L"ShowRunning", 1) != 0;
    s.openRunning = IniInt(kSecMain, L"OpenRunning", 1) != 0;
    s.themeOpacity = std::min(std::max(IniInt(kSecMain, L"ThemeOpacity", 100), 20), 100);
    s.iconOpacity = std::min(std::max(IniInt(kSecMain, L"IconOpacity", 100), 20), 100);
    s.theme = IniGet(kSecMain, L"Theme", L"Dark");
    s.hotkey = IniGet(kSecMain, L"HotKey-Toggle", L"Control+Alt+R");
    s.fontName = IniGet(kSecMain, L"FontName", L"Segoe UI");
    if (s.fontName.empty()) s.fontName = L"Segoe UI";

    for (auto& it : A.items) FreeItem(it);
    A.items.clear();
    int count = IniInt(kSecIcons, L"count", -1);
    if (count < 0) {
        DefaultItems();
    } else {
        for (int i = 0; i < count && i < 200; ++i) {
            std::wstring p = std::to_wstring(i) + L"-";
            Item it;
            it.sep = IniInt(kSecIcons, (p + L"IsSeparator").c_str(), 0) != 0;
            it.file = IniGet(kSecIcons, (p + L"FileName").c_str(), L"");
            it.file2 = IniGet(kSecIcons, (p + L"FileName2").c_str(), L"");
            it.title = IniGet(kSecIcons, (p + L"Title").c_str(), L"");
            it.cmd = IniGet(kSecIcons, (p + L"Command").c_str(), L"");
            it.args = IniGet(kSecIcons, (p + L"Arguments").c_str(), L"");
            it.wd = IniGet(kSecIcons, (p + L"WorkingDirectory").c_str(), L"");
            it.showCmd = IniInt(kSecIcons, (p + L"ShowCmd").c_str(), 0);
            it.openRunning = IniInt(kSecIcons, (p + L"OpenRunning").c_str(), 0) != 0;
            it.ctx = IniInt(kSecIcons, (p + L"UseContext").c_str(), 0) != 0;
            A.items.push_back(it);
        }
    }
    for (auto& it : A.items) LoadItemImages(it);
    A.th = MakeTheme(s.theme);
}

// ---------------------------------------------------------------------------------------------
// Applications en cours d'exécution & corbeille
// ---------------------------------------------------------------------------------------------
struct RunEnum {
    std::vector<std::pair<std::wstring, HWND>> wins;
    std::map<DWORD, std::wstring> cache;
};
static BOOL CALLBACK EnumWinCb(HWND h, LPARAM lp) {
    auto* re = (RunEnum*)lp;
    if (h == A.hwnd || !IsWindowVisible(h) || GetWindow(h, GW_OWNER)) return TRUE;
    if (GetWindowTextLengthW(h) == 0) return TRUE;
    LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
    if ((ex & WS_EX_TOOLWINDOW) && !(ex & WS_EX_APPWINDOW)) return TRUE;
    BOOL cloaked = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(h, 14 /*DWMWA_CLOAKED*/, &cloaked, sizeof(cloaked))) && cloaked) return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    auto f = re->cache.find(pid);
    if (f == re->cache.end()) {
        std::wstring path;
        HANDLE ph = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (ph) {
            wchar_t buf[MAX_PATH * 2];
            DWORD n = MAX_PATH * 2;
            if (QueryFullProcessImageNameW(ph, 0, buf, &n)) path = Lower(buf);
            CloseHandle(ph);
        }
        f = re->cache.emplace(pid, path).first;
    }
    if (!f->second.empty()) re->wins.emplace_back(f->second, h);
    return TRUE;
}

static std::wstring ItemExe(const Item& it) {
    if (it.cmd.empty() || it.cmd[0] == L'[') return L"";
    std::wstring p = Lower(ResolvePath(it.cmd));
    return ExtOf(p) == L".exe" ? p : L"";
}

static void RefreshRunning() {
    RunEnum re;
    EnumWindows(EnumWinCb, (LPARAM)&re);
    for (auto& it : A.items) {
        bool was = it.running;
        it.running = false;
        it.runWnd = nullptr;
        if (it.sep) continue;
        std::wstring exe = ItemExe(it);
        if (exe.empty()) continue;
        for (auto& w : re.wins)
            if (w.first == exe) { it.running = true; it.runWnd = w.second; break; }
        if (was != it.running) A.dirty = true;
    }
}

static void RefreshBin() {
    SHQUERYRBINFO qi = {};
    qi.cbSize = sizeof(qi);
    bool full = SUCCEEDED(SHQueryRecycleBinW(nullptr, &qi)) && qi.i64NumItems > 0;
    if (full != A.binFull) { A.binFull = full; A.dirty = true; }
}

// ---------------------------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------------------------
static void ActivateWindow(HWND w) {
    if (IsIconic(w)) ShowWindow(w, SW_RESTORE);
    SetForegroundWindow(w);
}

static int ShowCmdToSW(int c) { return c == 1 ? SW_SHOWMINNOACTIVE : (c == 2 ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL); }

static void LaunchItem(int idx, bool admin) {
    if (idx < 0 || idx >= (int)A.items.size()) return;
    Item& it = A.items[idx];
    if (it.sep) return;
    it.bounceT = 0.0f;
    A.dirty = true;
    if (it.cmd == kRecycle) {
        ShellExecuteW(nullptr, L"open", L"shell:RecycleBinFolder", nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
    if (!admin && A.set.showRunning && it.running && it.runWnd && (A.set.openRunning || it.openRunning)) {
        ActivateWindow(it.runWnd);
        return;
    }
    std::wstring cmd = ResolvePath(it.cmd);
    std::wstring args = ExpandEnv(it.args);
    std::wstring wd = ExpandEnv(it.wd);
    if (wd.empty() && !cmd.empty() && cmd[0] != L'[' && PathFileExistsW(cmd.c_str()) && !PathIsDirectoryW(cmd.c_str())) {
        wchar_t d[MAX_PATH * 2];
        wcsncpy(d, cmd.c_str(), MAX_PATH * 2 - 1);
        d[MAX_PATH * 2 - 1] = 0;
        PathRemoveFileSpecW(d);
        wd = d;
    }
    HINSTANCE r = ShellExecuteW(A.hwnd, admin ? L"runas" : L"open", cmd.c_str(), args.empty() ? nullptr : args.c_str(),
                                wd.empty() ? nullptr : wd.c_str(), ShowCmdToSW(it.showCmd));
    if ((INT_PTR)r <= 32 && !(admin && (INT_PTR)r == SE_ERR_ACCESSDENIED)) {
        std::wstring msg = std::wstring(T(L"Impossible de lancer :\n", L"Unable to launch:\n")) + cmd;
        MessageBoxW(A.hwnd, msg.c_str(), kAppName, MB_OK | MB_ICONWARNING | MB_TOPMOST);
    }
}

static bool ResolveLnk(const std::wstring& lnk, std::wstring& target, std::wstring& args, std::wstring& wd) {
    IShellLinkW* sl = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&sl)) || !sl)
        return false;
    bool ok = false;
    IPersistFile* pf = nullptr;
    if (SUCCEEDED(sl->QueryInterface(IID_IPersistFile, (void**)&pf)) && pf) {
        if (SUCCEEDED(pf->Load(lnk.c_str(), STGM_READ))) {
            wchar_t buf[MAX_PATH * 2];
            buf[0] = 0;
            if (SUCCEEDED(sl->GetPath(buf, MAX_PATH * 2, nullptr, SLGP_RAWPATH)) && buf[0]) {
                target = buf; ok = true;
                buf[0] = 0;
                if (SUCCEEDED(sl->GetArguments(buf, MAX_PATH * 2))) args = buf;
                buf[0] = 0;
                if (SUCCEEDED(sl->GetWorkingDirectory(buf, MAX_PATH * 2))) wd = buf;
            }
        }
        pf->Release();
    }
    sl->Release();
    return ok;
}

static void AddPath(const std::wstring& pathIn, int at) {
    std::wstring path = pathIn;
    Item it;
    it.cmd = path;
    wchar_t* fn = PathFindFileNameW(path.c_str());
    std::wstring name = fn ? fn : path;
    bool isDir = PathIsDirectoryW(path.c_str()) != FALSE;
    if (!isDir) {
        size_t dot = name.find_last_of(L'.');
        if (dot != std::wstring::npos && dot > 0) name = name.substr(0, dot);
    }
    it.title = name;
    if (!isDir && ExtOf(path) == L".lnk") {
        std::wstring t, a, w;
        if (ResolveLnk(path, t, a, w)) { it.cmd = t; it.args = a; it.wd = w; }
        it.file = path;  // l'icône est extraite du raccourci
    }
    if (isDir) it.file = L"glyph:E8B7:F6B04A:E3801C";
    LoadItemImages(it);
    it.size = A.g.s0;
    if (at < 0 || at > (int)A.items.size()) at = (int)A.items.size();
    A.items.insert(A.items.begin() + at, it);
    RecalcGeometry();
    SaveSettings();
}

static void RemoveItem(int idx) {
    if (idx < 0 || idx >= (int)A.items.size()) return;
    FreeItem(A.items[idx]);
    A.items.erase(A.items.begin() + idx);
    A.hoverIdx = A.labelIdx = -1;
    RecalcGeometry();
    SaveSettings();
}

static void ApplyMenuTheme() {
    // Menus contextuels sombres sur Windows 10 1903+ / 11 (API non documentée, sans effet si absente).
    typedef LONG(WINAPI * PFN_RtlGetVersion)(OSVERSIONINFOW*);
    PFN_RtlGetVersion rtl = (PFN_RtlGetVersion)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
    OSVERSIONINFOW vi = {};
    vi.dwOSVersionInfoSize = sizeof(vi);
    if (!rtl || rtl(&vi) != 0 || vi.dwBuildNumber < 18362) return;  // API disponible depuis Windows 10 1903
    HMODULE ux = GetModuleHandleW(L"uxtheme.dll");
    if (!ux) ux = LoadLibraryW(L"uxtheme.dll");
    if (!ux) return;
    typedef int(WINAPI * PFN_Mode)(int);
    typedef void(WINAPI * PFN_Flush)();
    PFN_Mode setMode = (PFN_Mode)GetProcAddress(ux, MAKEINTRESOURCEA(135));
    PFN_Flush flush = (PFN_Flush)GetProcAddress(ux, MAKEINTRESOURCEA(136));
    if (setMode) setMode(A.th.dark ? 2 : 3);
    if (flush) flush();
}

static bool ParseHotkey(const std::wstring& s, UINT& mods, UINT& vk) {
    mods = 0; vk = 0;
    size_t st = 0;
    while (st <= s.size()) {
        size_t p = s.find(L'+', st);
        std::wstring tok = Lower(Trim(s.substr(st, p == std::wstring::npos ? p : p - st)));
        if (tok == L"control" || tok == L"ctrl") mods |= MOD_CONTROL;
        else if (tok == L"alt") mods |= MOD_ALT;
        else if (tok == L"shift") mods |= MOD_SHIFT;
        else if (tok == L"win" || tok == L"windows") mods |= MOD_WIN;
        else if (tok.size() == 1) vk = (UINT)towupper(tok[0]);
        else if (tok.size() >= 2 && tok[0] == L'f') vk = VK_F1 + (UINT)(_wtoi(tok.c_str() + 1) - 1);
        if (p == std::wstring::npos) break;
        st = p + 1;
    }
    return vk != 0;
}

static void RegisterToggleHotkey() {
    UnregisterHotKey(A.hwnd, 1);
    UINT m, v;
    if (ParseHotkey(A.set.hotkey, m, v)) RegisterHotKey(A.hwnd, 1, m | MOD_NOREPEAT, v);
}

// ---------------------------------------------------------------------------------------------
// Menus contextuels
// ---------------------------------------------------------------------------------------------
enum {
    ID_OPEN = 100, ID_ADMIN, ID_LOCATION, ID_REMOVE, ID_EMPTYBIN,
    ID_ADDPROG = 200, ID_ADDSEP, ID_LOCK, ID_AUTOHIDE, ID_LABELS, ID_RUNNING, ID_OPENINI, ID_RELOAD, ID_EXIT, ID_SETTINGS,
    ID_POS = 300, ID_THEME = 310, ID_SIZE = 320
};
static const int kSizes[][2] = {{36, 64}, {44, 80}, {56, 100}, {68, 120}};
static const wchar_t* kThemes[] = {L"Dark", L"Light", L"Aurora"};

static HWND g_setWnd = nullptr;     // fenêtre Paramètres (settings_ui.h)
static void OpenSettings();
static void ApplyIconOpacity();

static void Reload() {
    LoadSettings();
    A.g.S = SystemScale();
    for (auto& it : A.items) it.size = 0;
    RecalcGeometry();
    RegisterToggleHotkey();
    ApplyMenuTheme();
    ApplyIconOpacity();
}

static void ShowMenu(int hitIdx) {
    HMENU m = CreatePopupMenu();
    const bool onItem = hitIdx >= 0 && hitIdx < (int)A.items.size() && !A.items[hitIdx].sep;
    if (onItem) {
        const Item& it = A.items[hitIdx];
        AppendMenuW(m, MF_STRING, ID_OPEN, T(L"Ouvrir", L"Open"));
        if (it.cmd == kRecycle) AppendMenuW(m, MF_STRING, ID_EMPTYBIN, T(L"Vider la corbeille", L"Empty Recycle Bin"));
        else {
            AppendMenuW(m, MF_STRING, ID_ADMIN, T(L"Exécuter en tant qu'administrateur", L"Run as administrator"));
            AppendMenuW(m, MF_STRING, ID_LOCATION, T(L"Ouvrir l'emplacement du fichier", L"Open file location"));
        }
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING | (A.set.lockIcons ? MF_GRAYED : 0), ID_REMOVE, T(L"Retirer du dock", L"Remove from dock"));
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    }
    AppendMenuW(m, MF_STRING, ID_ADDPROG, T(L"Ajouter un programme…", L"Add a program…"));
    AppendMenuW(m, MF_STRING, ID_ADDSEP, T(L"Ajouter un séparateur", L"Add a separator"));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);

    HMENU pos = CreatePopupMenu();
    const wchar_t* posNames[] = {T(L"Haut", L"Top"), T(L"Gauche", L"Left"), T(L"Bas", L"Bottom"), T(L"Droite", L"Right")};
    for (int i = 0; i < 4; ++i)
        AppendMenuW(pos, MF_STRING | (A.set.side == i ? MF_CHECKED : 0), ID_POS + i, posNames[i]);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)pos, T(L"Position à l'écran", L"Screen position"));

    HMENU th = CreatePopupMenu();
    const wchar_t* thNames[] = {T(L"Sombre", L"Dark"), T(L"Clair", L"Light"), T(L"Aurore", L"Aurora")};
    for (int i = 0; i < 3; ++i)
        AppendMenuW(th, MF_STRING | (Lower(A.set.theme) == Lower(kThemes[i]) ? MF_CHECKED : 0), ID_THEME + i, thNames[i]);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)th, T(L"Thème", L"Theme"));

    HMENU sz = CreatePopupMenu();
    const wchar_t* szNames[] = {T(L"Petites", L"Small"), T(L"Moyennes", L"Medium"), T(L"Grandes", L"Large"),
                                T(L"Très grandes", L"Extra large")};
    for (int i = 0; i < 4; ++i)
        AppendMenuW(sz, MF_STRING | (A.set.iconMin == kSizes[i][0] ? MF_CHECKED : 0), ID_SIZE + i, szNames[i]);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)sz, T(L"Taille des icônes", L"Icon size"));

    AppendMenuW(m, MF_STRING | (A.set.autoHide ? MF_CHECKED : 0), ID_AUTOHIDE, T(L"Masquer automatiquement", L"Auto-hide"));
    AppendMenuW(m, MF_STRING | (A.set.hideLabels ? 0 : MF_CHECKED), ID_LABELS, T(L"Afficher les noms", L"Show labels"));
    AppendMenuW(m, MF_STRING | (A.set.showRunning ? MF_CHECKED : 0), ID_RUNNING,
                T(L"Indiquer les applications ouvertes", L"Show running applications"));
    AppendMenuW(m, MF_STRING | (A.set.lockIcons ? MF_CHECKED : 0), ID_LOCK, T(L"Verrouiller les icônes", L"Lock icons"));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_SETTINGS, T(L"Paramètres…", L"Settings…"));
    AppendMenuW(m, MF_STRING, ID_OPENINI, T(L"Modifier Settings.ini", L"Edit Settings.ini"));
    AppendMenuW(m, MF_STRING, ID_RELOAD, T(L"Recharger la configuration", L"Reload configuration"));
    AppendMenuW(m, MF_STRING, ID_EXIT, T(L"Quitter", L"Exit"));

    POINT pt;
    GetCursorPos(&pt);
    UINT flags = TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY;
    if (A.set.side == SIDE_BOTTOM) flags |= TPM_BOTTOMALIGN;
    if (A.set.side == SIDE_RIGHT) flags |= TPM_RIGHTALIGN;
    A.menuOpen = true;
    SetForegroundWindow(A.hwnd);
    int cmd = (int)TrackPopupMenu(m, flags, pt.x, pt.y, 0, A.hwnd, nullptr);
    PostMessageW(A.hwnd, WM_NULL, 0, 0);
    A.menuOpen = false;
    DestroyMenu(m);
    A.idleT = 0;

    switch (cmd) {
        case ID_OPEN: LaunchItem(hitIdx, false); break;
        case ID_ADMIN: LaunchItem(hitIdx, true); break;
        case ID_EMPTYBIN: SHEmptyRecycleBinW(A.hwnd, nullptr, 0); RefreshBin(); break;
        case ID_LOCATION: {
            std::wstring p = ResolvePath(A.items[hitIdx].cmd);
            if (PathFileExistsW(p.c_str())) {
                std::wstring arg = L"/select,\"" + p + L"\"";
                ShellExecuteW(nullptr, L"open", L"explorer.exe", arg.c_str(), nullptr, SW_SHOWNORMAL);
            }
            break;
        }
        case ID_REMOVE: RemoveItem(hitIdx); break;
        case ID_ADDPROG: {
            wchar_t file[MAX_PATH * 2] = {};
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = A.hwnd;
            static wchar_t filter[] = L"Programmes et raccourcis\0*.exe;*.lnk;*.bat;*.cmd\0Tous les fichiers\0*.*\0\0";
            ofn.lpstrFilter = filter;
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH * 2;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
            if (GetOpenFileNameW(&ofn)) AddPath(file, hitIdx >= 0 ? hitIdx + 1 : -1);
            break;
        }
        case ID_ADDSEP: {
            Item sp;
            sp.sep = true;
            A.items.insert(A.items.begin() + (hitIdx >= 0 ? hitIdx + 1 : (int)A.items.size()), sp);
            RecalcGeometry();
            SaveSettings();
            break;
        }
        case ID_LOCK: A.set.lockIcons = !A.set.lockIcons; SaveSettings(); break;
        case ID_AUTOHIDE: A.set.autoHide = !A.set.autoHide; RecalcGeometry(); SaveSettings(); break;
        case ID_LABELS: A.set.hideLabels = !A.set.hideLabels; A.dirty = true; SaveSettings(); break;
        case ID_RUNNING: A.set.showRunning = !A.set.showRunning; A.dirty = true; SaveSettings(); break;
        case ID_SETTINGS: OpenSettings(); break;
        case ID_OPENINI: ShellExecuteW(nullptr, L"open", L"notepad.exe", (L"\"" + g_ini + L"\"").c_str(), nullptr, SW_SHOWNORMAL); break;
        case ID_RELOAD: Reload(); break;
        case ID_EXIT: DestroyWindow(A.hwnd); break;
        default:
            if (cmd >= ID_POS && cmd < ID_POS + 4) {
                A.set.side = cmd - ID_POS;
                RecalcGeometry(); SaveSettings();
            } else if (cmd >= ID_THEME && cmd < ID_THEME + 3) {
                A.set.theme = kThemes[cmd - ID_THEME];
                A.th = MakeTheme(A.set.theme);
                ApplyMenuTheme();
                A.dirty = true; SaveSettings();
            } else if (cmd >= ID_SIZE && cmd < ID_SIZE + 4) {
                A.set.iconMin = kSizes[cmd - ID_SIZE][0];
                A.set.iconMax = kSizes[cmd - ID_SIZE][1];
                RecalcGeometry(); SaveSettings();
            }
    }
}

// ---------------------------------------------------------------------------------------------
// Boucle d'animation
// ---------------------------------------------------------------------------------------------
static void SetTimerRate(int ms) {
    if (ms == A.timerMs) return;
    A.timerMs = ms;
    SetTimer(A.hwnd, 1, ms, nullptr);
}

static float MaxSize() {
    float m = A.g.s0;
    for (auto& it : A.items) if (!it.sep) m = std::max(m, it.size);
    return m;
}

static void Tick() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float dt = (float)((double)(now.QuadPart - A.last.QuadPart) / (double)A.freq.QuadPart);
    A.last = now;
    dt = dm::clampf(dt, 0.001f, 0.05f);
    const Geo& G = A.g;
    float moved = 0;

    POINT cp;
    GetCursorPos(&cp);
    RECT wr;
    GetWindowRect(A.hwnd, &wr);
    float cu = 0, cv = 0;
    ToUV((float)(cp.x - wr.left), (float)(cp.y - wr.top), cu, cv);
    bool inWin = PtInRect(&wr, cp) != 0;

    // --- Survol ---
    bool hot = false;
    if (!A.forceHidden) {
        float vis = A.real.total + 2 * G.padSide;
        float uA = A.realStart - G.padSide - 6 * G.S, uB = A.realStart + A.real.total + G.padSide + 6 * G.S;
        if (A.hide < 0.5f) {
            float topV = G.edgeGap + std::max(G.barH, G.padBottom + MaxSize() + G.padTop) + 4 * G.S;
            hot = inWin && cu >= uA && cu <= uB && cv >= -2 && cv <= topV;
        } else if (A.set.autoHide) {
            float mid = G.L * 0.5f;
            hot = inWin && cv <= 2.5f * G.S && std::fabs(cu - mid) <= std::max(vis * 0.5f, 120 * G.S);
        }
        if (A.menuOpen || A.dragging) hot = A.hotPrev;
    }
    A.hotPrev = hot;
    {
        float old = A.hoverW;
        A.hoverW = dm::clampf(A.hoverW + (hot ? 1.0f : -1.0f) * dt * 9.0f, 0.0f, 1.0f);
        moved += std::fabs(A.hoverW - old);
    }
    if (hot) A.idleT = 0; else A.idleT += dt;

    // --- Masquage automatique ---
    bool wantHidden = A.forceHidden || (A.set.autoHide && !hot && !A.menuOpen && !A.dragging &&
                                        A.idleT * 1000.0f >= (float)A.set.autoHideDelay);
    {
        float old = A.hide;
        A.hide = dm::clampf(A.hide + (wantHidden ? 1.0f : -1.0f) * dt * 4.5f, 0.0f, 1.0f);
        moved += std::fabs(A.hide - old);
    }

    // --- Zoom ---
    std::vector<bool> sep = SepVec();
    std::vector<float> targets;
    float baseCursor = cu - G.baseStart;
    dm::zoomTargets(G.base, sep, G.s0, G.sMax, (float)A.set.zoomWidth, G.gap, baseCursor, hot && A.hide < 0.5f, targets);
    std::vector<float> sizes(A.items.size());
    float k = 1.0f - std::exp(-dt * 17.0f);
    for (size_t i = 0; i < A.items.size(); ++i) {
        Item& it = A.items[i];
        if (it.sep) { sizes[i] = G.sepW; continue; }
        float old = it.size;
        it.size += (targets[i] - it.size) * k;
        if (std::fabs(targets[i] - it.size) < 0.04f) it.size = targets[i];
        moved += std::fabs(it.size - old);
        sizes[i] = it.size;
    }
    A.real = dm::makeReal(sep, sizes, G.sepW, G.gap);
    {
        float centered = (G.L - A.real.total) * 0.5f;
        float anchored = cu - dm::mapCursor(G.base, A.real, G.gap, baseCursor);
        float st = dm::lerpf(centered, anchored, A.hoverW * (hot ? 1.0f : 1.0f));
        if (A.real.total <= G.L - 2 * G.margin) st = dm::clampf(st, G.margin, G.L - G.margin - A.real.total);
        else st = centered;
        moved += std::fabs(st - A.realStart);
        A.realStart = st;
    }

    // --- Index survolé ---
    int hi = -1;
    if (hot && A.hide < 0.5f) hi = dm::hitIndex(A.real, G.gap, cu - A.realStart);
    if (A.dragging) hi = A.pressIdx;
    A.hoverIdx = hi;
    if (hi >= 0 && !A.items[hi].sep) A.labelIdx = hi;
    {
        bool showLabel = hi >= 0 && !A.items[hi].sep && !A.dragging && !A.menuOpen && !A.set.hideLabels;
        float old = A.labelA;
        A.labelA = dm::clampf(A.labelA + (showLabel ? 1.0f : -1.0f) * dt * 9.0f, 0.0f, 1.0f);
        moved += std::fabs(A.labelA - old);
    }
    bool bouncing = false;
    for (size_t i = 0; i < A.items.size(); ++i) {
        Item& it = A.items[i];
        float hold = ((int)i == hi) ? 1.0f : 0.0f;
        float old = it.hl;
        it.hl = dm::clampf(it.hl + (hold > it.hl ? 1.0f : -1.0f) * dt * 10.0f, 0.0f, 1.0f);
        if (std::fabs(hold - it.hl) < 0.001f) it.hl = hold;
        moved += std::fabs(it.hl - old);
        if (it.bounceT >= 0) {
            it.bounceT += dt;
            if (it.bounceT > 0.9f) it.bounceT = -1.0f; else bouncing = true;
            moved += 1;
        }
    }

    // --- Tâches périodiques ---
    DWORD t = GetTickCount();
    if (A.set.showRunning && t - A.lastRunning > 1200) { A.lastRunning = t; RefreshRunning(); }
    if (t - A.lastBin > 3000) {
        A.lastBin = t;
        for (auto& it : A.items) if (it.cmd == kRecycle) { RefreshBin(); break; }
    }
    if (t - A.lastTop > 3000) {
        A.lastTop = t;
        SetWindowPos(A.hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    if (A.dirty || moved > 0.002f || bouncing) {
        A.dirty = false;
        Render();
        SetTimerRate(15);
    } else {
        SetTimerRate(hot ? 15 : 40);
    }
}

#include "settings_ui.h"

// ---------------------------------------------------------------------------------------------
// Fenêtre
// ---------------------------------------------------------------------------------------------
static int HitAt(int x, int y) {
    if (A.real.left.size() != A.items.size()) return -1;  // layout pas encore recalculé
    float u, v;
    ToUV((float)x, (float)y, u, v);
    float topV = A.g.edgeGap + std::max(A.g.barH, A.g.padBottom + MaxSize() + A.g.padTop);
    if (v < 0 || v > topV) return -1;
    return dm::hitIndex(A.real, A.g.gap, u - A.realStart);
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_TIMER:
            if (wp == 1) Tick();
            return 0;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_LBUTTONDOWN: {
            int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
            A.pressIdx = HitAt(x, y);
            A.pressPt.x = x; A.pressPt.y = y;
            A.dragging = false;
            if (A.pressIdx >= 0) SetCapture(h);
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (A.pressIdx >= 0 && (wp & MK_LBUTTON) && !A.set.lockIcons) {
                int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
                if (!A.dragging && (std::abs(x - A.pressPt.x) + std::abs(y - A.pressPt.y)) > (int)(6 * A.g.S)) A.dragging = true;
                if (A.dragging && A.real.left.size() == A.items.size()) {
                    float u, v;
                    ToUV((float)x, (float)y, u, v);
                    int target = dm::nearestSlot(A.real, u - A.realStart);
                    if (target != A.pressIdx && target >= 0 && target < (int)A.items.size()) {
                        if (target > A.pressIdx) std::rotate(A.items.begin() + A.pressIdx, A.items.begin() + A.pressIdx + 1, A.items.begin() + target + 1);
                        else std::rotate(A.items.begin() + target, A.items.begin() + A.pressIdx, A.items.begin() + A.pressIdx + 1);
                        A.pressIdx = target;
                        A.g.base = dm::makeBase(SepVec(), A.g.s0, A.g.sepW, A.g.gap);
                        A.g.baseStart = (A.g.L - A.g.base.total) * 0.5f;
                        A.dirty = true;
                    }
                }
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            int idx = A.pressIdx;
            bool wasDrag = A.dragging;
            A.pressIdx = -1;
            A.dragging = false;
            if (GetCapture() == h) ReleaseCapture();
            if (wasDrag) { RecalcGeometry(); SaveSettings(); }
            else if (idx >= 0) {
                int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
                if (HitAt(x, y) == idx) LaunchItem(idx, false);
            }
            return 0;
        }
        case WM_MBUTTONUP: {
            int idx = HitAt((short)LOWORD(lp), (short)HIWORD(lp));
            if (idx >= 0 && idx < (int)A.items.size() && !A.items[idx].sep && A.items[idx].cmd != kRecycle) {
                bool prev = A.set.openRunning;
                A.set.openRunning = false;  // clic milieu : toujours une nouvelle instance
                LaunchItem(idx, false);
                A.set.openRunning = prev;
            }
            return 0;
        }
        case WM_RBUTTONUP:
            ShowMenu(HitAt((short)LOWORD(lp), (short)HIWORD(lp)));
            return 0;
        case WM_DROPFILES: {
            HDROP hd = (HDROP)wp;
            POINT pt = {};
            DragQueryPoint(hd, &pt);
            float u, v;
            ToUV((float)pt.x, (float)pt.y, u, v);
            int at = -1;
            int nearIdx = A.items.empty() ? -1 : dm::nearestSlot(A.real, u - A.realStart);
            if (nearIdx >= 0) {
                float c = A.real.left[nearIdx] + A.real.width[nearIdx] * 0.5f;
                at = (u - A.realStart > c) ? nearIdx + 1 : nearIdx;
            }
            UINT n = DragQueryFileW(hd, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < n; ++i) {
                wchar_t path[MAX_PATH * 2];
                if (DragQueryFileW(hd, i, path, MAX_PATH * 2)) {
                    AddPath(path, at);
                    if (at >= 0) ++at;
                }
            }
            DragFinish(hd);
            return 0;
        }
        case WM_HOTKEY:
            A.forceHidden = !A.forceHidden;
            A.dirty = true;
            return 0;
        case WM_SETTINGCHANGE:
            if (wp != SPI_SETWORKAREA) return 0;
            [[fallthrough]];
        case WM_DISPLAYCHANGE:
            if (A.hwnd && A.dib) RecalcGeometry();
            return 0;
        case WM_DESTROY:
            if (g_setWnd) DestroyWindow(g_setWnd);
            KillTimer(h, 1);
            UnregisterHotKey(h, 1);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    // Instance unique
    const bool restart = wcsstr(GetCommandLineW(), L"--restart") != nullptr;
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\RocketDockModern.SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (!restart) return 0;
        // Redémarrage après mise à jour : on attend la fin de l'ancienne instance
        DWORD w = WaitForSingleObject(mutex, 15000);
        if (w != WAIT_OBJECT_0 && w != WAIT_ABANDONED) return 0;
    }
    {
        wchar_t me[MAX_PATH * 2];
        GetModuleFileNameW(nullptr, me, MAX_PATH * 2);
        DeleteFileW((std::wstring(me) + L".old").c_str());  // reste d'une mise à jour précédente
    }

    typedef BOOL(WINAPI * PFN_DpiCtx)(HANDLE);
    PFN_DpiCtx setCtx = (PFN_DpiCtx)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext");
    if (setCtx) setCtx((HANDLE)-4);  // PER_MONITOR_AWARE_V2 (le manifeste le fait déjà si présent)

    g_fr = (PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_FRENCH);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    GdiplusStartupInput gi;
    GdiplusStartup(&A.gdipToken, &gi, nullptr);

    A.hInst = hInst;
    timeBeginPeriod(1);  // timer système à 1 ms pour des animations fluides
    g_ini = ExeDir() + L"\\Settings.ini";
    if (!PathFileExistsW(g_ini.c_str())) {
        // Création en UTF-16 pour supporter tous les chemins
        HANDLE f = CreateFileW(g_ini.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            const WORD bom = 0xFEFF;
            DWORD wrote;
            WriteFile(f, &bom, 2, &wrote, nullptr);
            CloseHandle(f);
        }
    }
    A.g.S = SystemScale();
    A.ia = new ImageAttributes();
    A.ia->SetWrapMode(WrapModeTileFlipXY);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClass;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(2));
    RegisterClassExW(&wc);
    A.hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_ACCEPTFILES,
                             kClass, kAppName, WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, hInst, nullptr);
    if (!A.hwnd) return 1;
    DragAcceptFiles(A.hwnd, TRUE);

    LoadSettings();
    ApplyIconOpacity();
    QueryPerformanceFrequency(&A.freq);
    QueryPerformanceCounter(&A.last);
    ApplyMenuTheme();
    RecalcGeometry();
    RegisterToggleHotkey();
    if (!PathFileExistsW((ExeDir() + L"\\Settings.ini").c_str()) || IniInt(kSecIcons, L"count", -1) < 0) SaveSettings();
    RefreshRunning();
    RefreshBin();
    ShowWindow(A.hwnd, SW_SHOWNOACTIVATE);
    SetTimer(A.hwnd, 1, A.timerMs, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    for (auto& it : A.items) FreeItem(it);
    delete A.gfx; delete A.surf; delete A.font; delete A.fam; delete A.ia;
    if (A.dib) DeleteObject(A.dib);
    if (A.mem) DeleteDC(A.mem);
    GdiplusShutdown(A.gdipToken);
    CoUninitialize();
    timeEndPeriod(1);
    if (mutex) CloseHandle(mutex);
    return (int)msg.wParam;
}
