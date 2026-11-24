// settings_ui.h - Fenêtre "Paramètres" moderne (thème dynamique, galerie de thèmes, réglages, éléments, mise à jour).
// Inclus par main.cpp : utilise l'état global A et les fonctions du dock.
#pragma once
#include <commctrl.h>
#include <uxtheme.h>
#include <winhttp.h>

static const wchar_t* kVersionW = L"2.3.1";
static const wchar_t* kDefaultUpdateUrl = L"https://raw.githubusercontent.com/agentia-GIT/Windows-Dock/main/dist/";

enum {
    IDC_NAV = 999,
    IDC_TAB = 1000,
    IDC_ICONMIN = 1101, IDC_ICONMAX, IDC_ZOOM, IDC_THOP, IDC_ICOP, IDC_OFFSET, IDC_AHDELAY,
    IDC_POS = 1110, IDC_THEME, IDC_MON, IDC_FONT, IDC_HOTKEY, IDC_HKAPPLY,
    IDC_AUTOHIDE = 1120, IDC_SHOWLABELS, IDC_LOCK, IDC_RUNNING, IDC_OPENRUN, IDC_AUTOSTART, IDC_LOCKDOCK, IDC_FREEMODE,
    IDC_LIST = 1130, IDC_ADD, IDC_ADDSEP, IDC_DEL, IDC_UP, IDC_DOWN,
    IDC_E_TITLE = 1140, IDC_E_CMD, IDC_E_ARGS, IDC_E_WD, IDC_E_ICON, IDC_B_CMD, IDC_B_ICON, IDC_E_SHOW, IDC_APPLYITEM,
    IDC_U_LOCAL = 1150, IDC_U_REMOTE, IDC_U_CHECK, IDC_U_UPDATE, IDC_U_LOG, IDC_U_FOLDER,
    IDC_CLOSE = 1160,
    IDC_GAL = 1170, IDC_GAL_FOLDER, IDC_GAL_REFRESH, IDC_HDR, IDC_HINT
};
static const int kValueLabelOffset = 500;  // le libellé de valeur d'un curseur a l'ID du curseur + 500
enum { WM_UPD = WM_APP + 1 };               // wParam : 0 statut, 1 version distante, 2 succès, 3 échec

// Pages : 0 Apparence, 1 Comportement, 2 Éléments, 3 Mise à jour, 4 Galerie de thèmes
static const int kPages = 5;
static const int kNavPage[kPages] = {4, 0, 1, 2, 3};  // ordre de la barre latérale -> page
static HWND g_nav = nullptr, g_pg[kPages] = {};
static int g_navSel = 0, g_navHot = -1;
static HFONT g_font = nullptr, g_fontH = nullptr, g_fontS = nullptr;
static bool g_filling = false;
static volatile LONG g_updBusy = 0;
static int g_dx = 0, g_dy = 0;  // décalage appliqué aux contrôles créés par Mk()

// Couleurs de l'interface, copiées du thème actif
static COLORREF g_cBg = 0, g_cPanel = 0, g_cText = 0, g_cMuted = 0, g_cAccent = 0;
static HBRUSH g_brBg = nullptr, g_brPanel = nullptr;
static bool g_uiDark = true;

static int SC(int v) { return (int)(v * A.g.S + 0.5f); }
static COLORREF Cr(const Color& c) { return RGB(c.GetR(), c.GetG(), c.GetB()); }
static Color Gc(COLORREF c, BYTE a = 255) { return Color(a, GetRValue(c), GetGValue(c), GetBValue(c)); }
static COLORREF Mix(COLORREF a, COLORREF b, int pct) {  // pct % de b
    auto m = [&](int x, int y) { return (x * (100 - pct) + y * pct) / 100; };
    return RGB(m(GetRValue(a), GetRValue(b)), m(GetGValue(a), GetGValue(b)), m(GetBValue(a), GetBValue(b)));
}

static void UpdateUiColors() {
    const Theme& t = A.th;
    g_cBg = Cr(t.uiBg); g_cPanel = Cr(t.uiPanel); g_cText = Cr(t.uiText); g_cMuted = Cr(t.uiMuted);
    g_cAccent = Cr(t.accent);
    g_uiDark = t.dark;
    if (g_brBg) DeleteObject(g_brBg);
    if (g_brPanel) DeleteObject(g_brPanel);
    g_brBg = CreateSolidBrush(g_cBg);
    g_brPanel = CreateSolidBrush(g_cPanel);
}

// ---------------------------------------------------------------------------------------------
// Outils de création de contrôles
// ---------------------------------------------------------------------------------------------
static HWND Mk(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id,
               DWORD ex = 0) {
    HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, SC(x + g_dx), SC(y + g_dy), SC(w), SC(h), parent,
                             (HMENU)(INT_PTR)id, A.hInst, nullptr);
    if (c && g_font) SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    return c;
}
static HWND Lbl(HWND p, const wchar_t* t, int x, int y, int w, int h = 20) { return Mk(p, L"STATIC", t, SS_LEFT, x, y, w, h, 0); }
static HWND Hint(HWND p, const wchar_t* t, int x, int y, int w, int h = 20) { return Mk(p, L"STATIC", t, SS_LEFT, x, y, w, h, IDC_HINT); }
static HWND Btn(HWND p, const wchar_t* t, int x, int y, int w, int id, int h = 28) {
    return Mk(p, L"BUTTON", t, BS_OWNERDRAW | WS_TABSTOP, x, y, w, h, id);
}
// Case à cocher dessinée en interrupteur : l'état est stocké dans GWLP_USERDATA
static HWND Chk(HWND p, const wchar_t* t, int x, int y, int w, int id) {
    return Mk(p, L"BUTTON", t, BS_OWNERDRAW | WS_TABSTOP, x, y, w, 26, id);
}
static bool ChkGet(HWND c) { return GetWindowLongPtrW(c, GWLP_USERDATA) != 0; }
static void ChkSet(HWND c, bool on) {
    SetWindowLongPtrW(c, GWLP_USERDATA, on ? 1 : 0);
    InvalidateRect(c, nullptr, FALSE);
}
static bool IsSwitchId(int id) { return id >= IDC_AUTOHIDE && id <= IDC_FREEMODE; }
static HWND Edt(HWND p, int x, int y, int w, int id) {
    return Mk(p, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, x, y, w, 26, id);
}
static HWND Cmb(HWND p, int x, int y, int w, int id, std::initializer_list<const wchar_t*> items) {
    HWND c = Mk(p, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, x, y, w, 240, id);
    for (auto* s : items) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)s);
    return c;
}
static void Slider(HWND p, int id, const wchar_t* label, int y, int mn, int mx) {
    Lbl(p, label, 14, y + 4, 175);
    HWND t = Mk(p, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, 190, y, 270, 28, id);
    SendMessageW(t, TBM_SETRANGE, TRUE, MAKELPARAM(mn, mx));
    Mk(p, L"STATIC", L"", SS_LEFT, 470, y + 4, 90, 20, id + kValueLabelOffset);
}

static std::wstring ItoW(int v) { return std::to_wstring(v); }

static void SetSlider(HWND page, int id, int v, const wchar_t* unit) {
    SendMessageW(GetDlgItem(page, id), TBM_SETPOS, TRUE, v);
    SetWindowTextW(GetDlgItem(page, id + kValueLabelOffset), (ItoW(v) + unit).c_str());
}

static bool PickFile(HWND owner, const wchar_t* filter, std::wstring& out) {
    wchar_t file[MAX_PATH * 2] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH * 2;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    if (!GetOpenFileNameW(&ofn)) return false;
    out = file;
    return true;
}

// ---------------------------------------------------------------------------------------------
// Démarrage avec Windows
// ---------------------------------------------------------------------------------------------
static const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static bool AutostartGet() {
    HKEY k;
    bool on = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &k) == ERROR_SUCCESS) {
        on = RegQueryValueExW(k, L"WinDock", nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS ||
             RegQueryValueExW(k, L"RocketDockModern", nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
        RegCloseKey(k);
    }
    return on;
}
static void AutostartSet(bool on) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    if (on) {
        wchar_t exe[MAX_PATH * 2];
        GetModuleFileNameW(nullptr, exe, MAX_PATH * 2);
        std::wstring v = L"\"" + std::wstring(exe) + L"\"";
        RegDeleteValueW(k, L"RocketDockModern");  // ancien nom
        RegSetValueExW(k, L"WinDock", 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(k, L"WinDock");
        RegDeleteValueW(k, L"RocketDockModern");
    }
    RegCloseKey(k);
}

// ---------------------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------------------
// Mise à jour directe (WinHTTP)
// ---------------------------------------------------------------------------------------------
static DWORD g_httpErr = 0;  // dernier code d'erreur WinHTTP (diagnostic)
static HINTERNET g_sess = nullptr;  // session WinHTTP réutilisée pendant une mise à jour
static void SessOpen() {
    if (g_sess) return;
    g_sess = WinHttpOpen(L"WinDock/2.3", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!g_sess) g_sess = WinHttpOpen(L"WinDock/2.3", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
}
static void SessClose() {
    if (g_sess) { WinHttpCloseHandle(g_sess); g_sess = nullptr; }
}
static bool HttpGet(const std::wstring& url, const std::wstring& token, std::string& out, DWORD& status) {
    status = 0;
    out.clear();
    wchar_t host[256] = {}, path[2048] = {};
    URL_COMPONENTSW uc = {};
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) return false;
    const bool own = (g_sess == nullptr);
    if (own) SessOpen();
    HINTERNET s = g_sess;
    if (!s) return false;
    bool ok = false;
    HINTERNET c = WinHttpConnect(s, host, uc.nPort, 0);
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                    : nullptr;
    if (r) {
        WinHttpSetTimeouts(r, 8000, 8000, 15000, 30000);
        std::wstring hdr = L"Cache-Control: no-cache\r\n";
        if (!token.empty()) hdr += L"Authorization: token " + token + L"\r\n";
        if (WinHttpSendRequest(r, hdr.c_str(), (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(r, nullptr)) {
            DWORD sz = sizeof(status);
            WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                                &status, &sz, WINHTTP_NO_HEADER_INDEX);
            ok = true;
            g_httpErr = 0;
            for (;;) {
                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(r, &avail) || avail == 0) break;
                size_t old = out.size();
                out.resize(old + avail);
                DWORD got = 0;
                if (!WinHttpReadData(r, &out[old], avail, &got)) { ok = false; break; }
                out.resize(old + got);
                if (out.size() > 64u * 1024 * 1024) { ok = false; break; }  // garde-fou
            }
        }
        else g_httpErr = GetLastError();
        WinHttpCloseHandle(r);
    } else {
        g_httpErr = GetLastError();
    }
    if (c) WinHttpCloseHandle(c);
    if (own) SessClose();
    return ok;
}

// GET avec 3 tentatives (réseau instable, limitation 429, erreurs 5xx)
static bool HttpGetRetry(const std::wstring& url, const std::wstring& token, std::string& out, DWORD& status) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (attempt) Sleep(700 * attempt);
        if (HttpGet(url, token, out, status) && status == 200) return true;
        if (status == 404 || status == 401 || status == 403) break;  // inutile de réessayer
    }
    return false;
}
// Description d'un échec : code HTTP ou code d'erreur réseau WinHTTP
static std::wstring NetErr(DWORD st) {
    return st ? L"HTTP " + ItoW((int)st) : (g_fr ? L"erreur réseau " : L"network error ") + ItoW((int)g_httpErr);
}

static bool RunHidden(const std::wstring& cmd, DWORD timeoutMs) {
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(0);
    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return false;
    DWORD code = 1;
    if (WaitForSingleObject(pi.hProcess, timeoutMs) == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code);
    else TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code == 0;
}
// Décompresse un .zip dans un dossier avec tar.exe (Windows 10+), sinon PowerShell.
static bool ExtractZip(const std::wstring& zip, const std::wstring& dir) {
    wchar_t win[MAX_PATH] = {};
    GetWindowsDirectoryW(win, MAX_PATH);
    BOOL wow = FALSE;
    if (sizeof(void*) == 4) IsWow64Process(GetCurrentProcess(), &wow);
    std::wstring sys = std::wstring(win) + (wow ? L"\\Sysnative" : L"\\System32");
    std::wstring tar = sys + L"\\tar.exe";
    if (PathFileExistsW(tar.c_str()) && RunHidden(L"\"" + tar + L"\" -xf \"" + zip + L"\" -C \"" + dir + L"\"", 180000)) return true;
    auto q = [](std::wstring v) { size_t i = 0; while ((i = v.find(L'\'', i)) != std::wstring::npos) { v.insert(i, 1, L'\''); i += 2; } return v; };
    std::wstring ps = sys + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    return RunHidden(L"\"" + ps + L"\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"Expand-Archive -LiteralPath '" + q(zip) +
                         L"' -DestinationPath '" + q(dir) + L"' -Force\"",
                     240000);
}

static std::wstring Utf8ToW(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

struct UpdJob {
    bool apply;
    std::wstring base, token, exePath, exeDir;
};

static void UpdPost(int kind, const std::wstring& text) {
    HWND h = g_setWnd;
    if (!h || !PostMessageW(h, WM_UPD, (WPARAM)kind, (LPARAM) new std::wstring(text))) return;
}

static std::wstring TrimW(std::wstring s) { return Trim(s); }

// Thread : vérifie la version distante puis (si apply) remplace les fichiers listés dans manifest.txt.
// Settings.ini n'est JAMAIS touché.
static DWORD WINAPI UpdThread(LPVOID p) {
    UpdJob* job = (UpdJob*)p;
    std::string body;
    DWORD st = 0;
    const bool fr = g_fr;
    auto fail = [&](const std::wstring& m) {
        UpdPost(3, m);
        SessClose();
        InterlockedExchange(&g_updBusy, 0);
        delete job;
        return (DWORD)0;
    };
    SessOpen();
    UpdPost(0, fr ? L"Connexion à GitHub…" : L"Connecting to GitHub…");
    if (!HttpGetRetry(job->base + L"version.txt", job->token, body, st))
        return fail((fr ? L"Impossible de lire la version distante (" : L"Cannot read remote version (") + NetErr(st) + L").");
    std::wstring remote = TrimW(Utf8ToW(body));
    UpdPost(1, remote);
    if (!job->apply) {
        SessClose();
        InterlockedExchange(&g_updBusy, 0);
        delete job;
        return 0;
    }
    if (!HttpGetRetry(job->base + L"manifest.txt", job->token, body, st))
        return fail((fr ? L"Liste des fichiers (manifest.txt) introuvable (" : L"File list (manifest.txt) not found (") + NetErr(st) + L").");

    // Liste des fichiers à mettre à jour
    struct F { std::wstring rel, tmp, dst; bool isExe, isZip; };
    std::vector<F> files;
    {
        std::wstring txt = Utf8ToW(body);
        size_t pos = 0;
        // Le fichier de l'exécutable en cours est remplacé par l'entrée du manifeste qui porte son nom
        // (ou à défaut WinDock_x64/x86.exe). Les exécutables des autres noms / architectures sont ignorés.
        const wchar_t* defName = (sizeof(void*) == 8) ? L"windock_x64.exe" : L"windock_x86.exe";
        std::wstring mine = Lower(PathFindFileNameW(job->exePath.c_str()));
        if (Lower(txt).find(mine) == std::wstring::npos) mine = defName;
        auto isExeName = [](const std::wstring& n) {
            return n.size() > 8 && (n.substr(n.size() - 8) == L"_x64.exe" || n.substr(n.size() - 8) == L"_x86.exe");
        };
        while (pos <= txt.size()) {
            size_t e = txt.find(L'\n', pos);
            std::wstring line = TrimW(txt.substr(pos, e == std::wstring::npos ? e : e - pos));
            pos = (e == std::wstring::npos) ? txt.size() + 1 : e + 1;
            if (line.empty() || line[0] == L'#') continue;
            std::replace(line.begin(), line.end(), L'\\', L'/');
            std::wstring low = Lower(line);
            if (line.find(L"..") != std::wstring::npos || line[0] == L'/' || line.find(L':') != std::wstring::npos) continue;
            if (low == L"settings.ini") continue;  // la configuration n'est jamais remplacée
            if (isExeName(low) && low != mine) continue;
            F f;
            f.rel = line;
            f.isExe = (low == mine);
            f.isZip = low.size() > 4 && low.substr(low.size() - 4) == L".zip";
            std::wstring local = line;
            std::replace(local.begin(), local.end(), L'/', L'\\');
            f.dst = f.isExe ? job->exePath : job->exeDir + L"\\" + local;
            f.tmp = f.dst + L".new";
            files.push_back(f);
        }
    }
    if (files.empty()) return fail(fr ? L"Aucun fichier à mettre à jour." : L"Nothing to update.");

    // Téléchargement dans des fichiers temporaires
    for (auto& f : files) {
        UpdPost(0, (fr ? L"Téléchargement : " : L"Downloading: ") + f.rel);
        if (!HttpGetRetry(job->base + f.rel, job->token, body, st) || body.empty()) {
            for (auto& g : files) DeleteFileW(g.tmp.c_str());
            return fail((fr ? L"Échec du téléchargement : " : L"Download failed: ") + f.rel + L" (" + NetErr(st) + L")");
        }
        if (f.isExe && (body.size() < 64 || body[0] != 'M' || body[1] != 'Z')) {
            for (auto& g : files) DeleteFileW(g.tmp.c_str());
            return fail(fr ? L"Le fichier téléchargé n'est pas un exécutable valide." : L"Downloaded file is not a valid executable.");
        }
        std::wstring dir = f.dst.substr(0, f.dst.find_last_of(L'\\'));
        SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
        HANDLE h = CreateFileW(f.tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        DWORD wrote = 0;
        bool okw = h != INVALID_HANDLE_VALUE && WriteFile(h, body.data(), (DWORD)body.size(), &wrote, nullptr) && wrote == body.size();
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
        if (!okw) {
            for (auto& g : files) DeleteFileW(g.tmp.c_str());
            return fail((fr ? L"Écriture impossible (droits d'accès ?) : " : L"Cannot write (permissions?): ") + f.dst);
        }
    }

    // Remplacement : fichiers ordinaires d'abord, exécutable en dernier (renommé en .old puis remplacé)
    UpdPost(0, fr ? L"Remplacement des fichiers…" : L"Replacing files…");
    for (auto& f : files) {
        if (!f.isZip) continue;  // archive (thèmes) : décompressée dans le dossier de l'application
        UpdPost(0, (fr ? L"Décompression : " : L"Extracting: ") + f.rel);
        bool okz = ExtractZip(f.tmp, job->exeDir);
        DeleteFileW(f.tmp.c_str());
        if (!okz) {
            for (auto& g : files) DeleteFileW(g.tmp.c_str());
            return fail(fr ? L"Décompression impossible (tar.exe / PowerShell indisponible ?)." : L"Cannot extract (tar.exe / PowerShell unavailable?).");
        }
    }
    for (auto& f : files) {
        if (f.isExe || f.isZip) continue;
        if (!MoveFileExW(f.tmp.c_str(), f.dst.c_str(), MOVEFILE_REPLACE_EXISTING))
            return fail((fr ? L"Remplacement impossible : " : L"Cannot replace: ") + f.rel);
    }
    for (auto& f : files) {
        if (!f.isExe) continue;
        std::wstring old = f.dst + L".old";
        DeleteFileW(old.c_str());
        if (!MoveFileExW(f.dst.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            DeleteFileW(f.tmp.c_str());
            return fail(fr ? L"Impossible de renommer l'exécutable en cours." : L"Cannot rename the running executable.");
        }
        if (!MoveFileExW(f.tmp.c_str(), f.dst.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            MoveFileExW(old.c_str(), f.dst.c_str(), MOVEFILE_REPLACE_EXISTING);  // retour arrière
            return fail(fr ? L"Remplacement de l'exécutable impossible (ancienne version restaurée)." : L"Executable replacement failed (old version restored).");
        }
    }
    UpdPost(2, remote);
    SessClose();
    InterlockedExchange(&g_updBusy, 0);
    delete job;
    return 0;
}

static void StartUpdate(bool apply) {
    if (InterlockedCompareExchange(&g_updBusy, 1, 0) != 0) return;
    UpdJob* j = new UpdJob();
    j->apply = apply;
    std::wstring base = Trim(IniGet(kSecMain, L"UpdateUrl", kDefaultUpdateUrl));
    if (base.empty()) base = kDefaultUpdateUrl;
    if (base.back() != L'/') base += L'/';
    j->base = base;
    j->token = Trim(IniGet(kSecMain, L"UpdateToken", L""));
    wchar_t exe[MAX_PATH * 2];
    GetModuleFileNameW(nullptr, exe, MAX_PATH * 2);
    j->exePath = exe;
    j->exeDir = ExeDir();
    HANDLE h = CreateThread(nullptr, 0, UpdThread, j, 0, nullptr);
    if (h) CloseHandle(h);
    else { InterlockedExchange(&g_updBusy, 0); delete j; }
}

// ---------------------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------------------
// Synchronisation des contrôles <-> réglages
// ---------------------------------------------------------------------------------------------
static int SelectedItem() {
    return (int)SendMessageW(GetDlgItem(g_pg[2], IDC_LIST), LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
}

static void FillList(int select) {
    HWND lv = GetDlgItem(g_pg[2], IDC_LIST);
    g_filling = true;
    SendMessageW(lv, LVM_DELETEALLITEMS, 0, 0);
    for (size_t i = 0; i < A.items.size(); ++i) {
        const Item& it = A.items[i];
        std::wstring title = it.sep ? std::wstring(T(L"─── séparateur ───", L"─── separator ───")) : it.title;
        LVITEMW li = {};
        li.mask = LVIF_TEXT;
        li.iItem = (int)i;
        li.pszText = (LPWSTR)title.c_str();
        SendMessageW(lv, LVM_INSERTITEMW, 0, (LPARAM)&li);
        li.iSubItem = 1;
        li.pszText = (LPWSTR)it.cmd.c_str();
        SendMessageW(lv, LVM_SETITEMTEXTW, i, (LPARAM)&li);
    }
    g_filling = false;
    if (select >= (int)A.items.size()) select = (int)A.items.size() - 1;
    if (select >= 0) {
        LVITEMW st = {};
        st.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
        st.state = LVIS_SELECTED | LVIS_FOCUSED;
        SendMessageW(lv, LVM_SETITEMSTATE, select, (LPARAM)&st);
    }
}

static void LoadFields() {
    int i = SelectedItem();
    HWND p = g_pg[2];
    bool valid = i >= 0 && i < (int)A.items.size() && !A.items[i].sep;
    const int ids[] = {IDC_E_TITLE, IDC_E_CMD, IDC_E_ARGS, IDC_E_WD, IDC_E_ICON, IDC_B_CMD, IDC_B_ICON, IDC_E_SHOW, IDC_APPLYITEM};
    for (int id : ids) EnableWindow(GetDlgItem(p, id), valid);
    if (!valid) {
        for (int id : {IDC_E_TITLE, IDC_E_CMD, IDC_E_ARGS, IDC_E_WD, IDC_E_ICON}) SetDlgItemTextW(p, id, L"");
        return;
    }
    const Item& it = A.items[i];
    SetDlgItemTextW(p, IDC_E_TITLE, it.title.c_str());
    SetDlgItemTextW(p, IDC_E_CMD, it.cmd.c_str());
    SetDlgItemTextW(p, IDC_E_ARGS, it.args.c_str());
    SetDlgItemTextW(p, IDC_E_WD, it.wd.c_str());
    SetDlgItemTextW(p, IDC_E_ICON, it.file.c_str());
    SendMessageW(GetDlgItem(p, IDC_E_SHOW), CB_SETCURSEL, std::min(std::max(it.showCmd, 0), 2), 0);
}

static bool ListIsStale() {
    HWND lv = GetDlgItem(g_pg[2], IDC_LIST);
    if ((int)SendMessageW(lv, LVM_GETITEMCOUNT, 0, 0) != (int)A.items.size()) return true;
    for (size_t i = 0; i < A.items.size(); ++i) {
        wchar_t t[512] = {};
        LVITEMW li = {};
        li.iSubItem = 0;
        li.pszText = t;
        li.cchTextMax = 512;
        SendMessageW(lv, LVM_GETITEMTEXTW, i, (LPARAM)&li);
        std::wstring want = A.items[i].sep ? std::wstring(T(L"─── séparateur ───", L"─── separator ───")) : A.items[i].title;
        if (want != t) return true;
    }
    return false;
}

static std::vector<ThemeInfo> g_setThemes;  // thèmes proposés dans la liste déroulante
static void GalRebuild();

static void FillThemeCombo() {
    if (!g_pg[0]) return;
    HWND c = GetDlgItem(g_pg[0], IDC_THEME);
    if (!c) return;
    SendMessageW(c, CB_RESETCONTENT, 0, 0);
    g_setThemes = EnumThemes();
    int sel = 0;
    for (size_t i = 0; i < g_setThemes.size(); ++i) {
        std::wstring l = g_setThemes[i].name + L"   ·   " + CategoryLabel(g_setThemes[i].category);
        SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)l.c_str());
        if (Lower(g_setThemes[i].folder) == Lower(A.set.theme)) sel = (int)i;
    }
    SendMessageW(c, CB_SETCURSEL, sel, 0);
}

// full = true : tout recharger (ouverture) ; false : réactivation (ne touche pas aux champs en cours d'édition)
static void SyncControls(bool full = true) {
    if (!g_setWnd) return;
    const Settings& s = A.set;
    HWND a = g_pg[0], b = g_pg[1];
    SendMessageW(GetDlgItem(a, IDC_POS), CB_SETCURSEL, s.side, 0);
    if (full) FillThemeCombo();
    SendMessageW(GetDlgItem(a, IDC_MON), CB_SETCURSEL, std::max(0, s.monitor), 0);
    if (full || GetFocus() != GetDlgItem(a, IDC_FONT)) SetDlgItemTextW(a, IDC_FONT, s.fontName.c_str());
    SetSlider(a, IDC_ICONMIN, s.iconMin, L" px");
    SetSlider(a, IDC_ICONMAX, s.iconMax, L" px");
    SetSlider(a, IDC_ZOOM, s.zoomWidth, L"");
    SetSlider(a, IDC_OFFSET, s.offset, L" px");
    SetSlider(a, IDC_THOP, s.themeOpacity, L" %");
    SetSlider(a, IDC_ICOP, s.iconOpacity, L" %");
    ChkSet(GetDlgItem(b, IDC_AUTOHIDE), s.autoHide);
    ChkSet(GetDlgItem(b, IDC_SHOWLABELS), !s.hideLabels);
    ChkSet(GetDlgItem(b, IDC_LOCK), s.lockIcons);
    ChkSet(GetDlgItem(b, IDC_RUNNING), s.showRunning);
    ChkSet(GetDlgItem(b, IDC_OPENRUN), s.openRunning);
    ChkSet(GetDlgItem(b, IDC_AUTOSTART), AutostartGet());
    ChkSet(GetDlgItem(b, IDC_LOCKDOCK), s.lockDock);
    ChkSet(GetDlgItem(b, IDC_FREEMODE), s.free);
    SetSlider(b, IDC_AHDELAY, s.autoHideDelay, L" ms");
    if (full || GetFocus() != GetDlgItem(b, IDC_HOTKEY)) SetDlgItemTextW(b, IDC_HOTKEY, s.hotkey.c_str());
    if (full || ListIsStale()) {
        int sel = SelectedItem();
        FillList(sel < 0 ? 0 : sel);
        LoadFields();
    }
}

// ---------------------------------------------------------------------------------------------
// Dessin GDI+ commun
// ---------------------------------------------------------------------------------------------
static void DrawTxt(Graphics& g, const std::wstring& s, const Font& f, const RectF& r, const Color& c,
                    StringAlignment h = StringAlignmentNear, StringAlignment v = StringAlignmentCenter, bool wrap = false) {
    StringFormat sf;
    sf.SetAlignment(h);
    sf.SetLineAlignment(v);
    sf.SetTrimming(StringTrimmingEllipsisCharacter);
    if (!wrap) sf.SetFormatFlags(StringFormatFlagsNoWrap);
    SolidBrush br(c);
    g.DrawString(s.c_str(), -1, &f, r, &sf, &br);
}
static void FillRR(Graphics& g, const RectF& r, float rad, const Color& c) {
    GraphicsPath p;
    RRPath(p, r, rad);
    SolidBrush b(c);
    g.FillPath(&b, &p);
}
static void StrokeRR(Graphics& g, const RectF& r, float rad, const Color& c, float w) {
    GraphicsPath p;
    RRPath(p, r, rad);
    Pen pen(c, w);
    g.DrawPath(&pen, &p);
}
static bool IsLightColor(COLORREF c) { return (GetRValue(c) * 299 + GetGValue(c) * 587 + GetBValue(c) * 114) / 1000 > 150; }

// Boutons et interrupteurs (BS_OWNERDRAW)
static void OnDrawItem(DRAWITEMSTRUCT* d) {
    if (d->CtlType != ODT_BUTTON) return;
    HDC hdc = d->hDC;
    RECT rc = d->rcItem;
    FillRect(hdc, &rc, g_brBg);
    HGDIOBJ oldF = SelectObject(hdc, g_font);
    {
        Graphics g(hdc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        Font f(hdc);
        wchar_t txt[256] = {};
        GetWindowTextW(d->hwndItem, txt, 256);
        const bool dis = (d->itemState & ODS_DISABLED) != 0, sel = (d->itemState & ODS_SELECTED) != 0,
                   foc = (d->itemState & ODS_FOCUS) != 0;
        const float S = A.g.S;
        RectF r((float)rc.left, (float)rc.top, (float)(rc.right - rc.left), (float)(rc.bottom - rc.top));
        if (IsSwitchId((int)d->CtlID)) {
            bool on = ChkGet(d->hwndItem);
            float pw = 38 * S, ph = 20 * S, py = r.Y + (r.Height - ph) / 2;
            RectF pill(r.X + 1, py, pw, ph);
            FillRR(g, pill, ph / 2, on ? Gc(g_cAccent) : Gc(Mix(g_cBg, g_cText, 22)));
            float kd = ph - 6 * S;
            SolidBrush kb(on ? (IsLightColor(g_cAccent) ? Color(255, 20, 20, 24) : Color(255, 255, 255, 255)) : Gc(g_cMuted));
            g.FillEllipse(&kb, pill.X + (on ? pw - kd - 3 * S : 3 * S), py + 3 * S, kd, kd);
            if (foc) StrokeRR(g, RectF(pill.X - 2, pill.Y - 2, pill.Width + 4, pill.Height + 4), ph / 2 + 2, Gc(g_cAccent, 140), 1.0f);
            DrawTxt(g, txt, f, RectF(r.X + pw + 12 * S, r.Y, r.Width - pw - 12 * S, r.Height), Gc(dis ? g_cMuted : g_cText));
        } else {
            const bool primary = (int)d->CtlID == IDC_U_UPDATE;
            RectF b(r.X + 0.5f, r.Y + 0.5f, r.Width - 1, r.Height - 1);
            COLORREF fill = primary ? g_cAccent : Mix(g_cPanel, g_cText, sel ? 20 : 6);
            COLORREF txtc = primary ? (IsLightColor(g_cAccent) ? RGB(20, 20, 24) : RGB(255, 255, 255)) : g_cText;
            if (dis) { fill = g_cPanel; txtc = g_cMuted; }
            FillRR(g, b, 7 * S, Gc(fill));
            StrokeRR(g, b, 7 * S, foc ? Gc(g_cAccent) : Gc(Mix(g_cPanel, g_cText, 16)), 1.0f);
            DrawTxt(g, txt, f, b, Gc(txtc), StringAlignmentCenter, StringAlignmentCenter);
        }
    }
    SelectObject(hdc, oldF);
}

// ---------------------------------------------------------------------------------------------
// Barre latérale
// ---------------------------------------------------------------------------------------------
static const wchar_t* kNavGlyph[kPages] = {L"", L"", L"", L"", L""};
static void ShowPage(int navIdx);

static std::wstring NavLabel(int i) {
    const wchar_t* n[kPages] = {T(L"Thèmes", L"Themes"), T(L"Apparence", L"Appearance"), T(L"Comportement", L"Behavior"),
                                T(L"Éléments", L"Items"), T(L"Mise à jour", L"Update")};
    return n[i];
}
static RectF NavItemRect(int i) {
    const float S = A.g.S;
    return RectF(12 * S, (84 + i * 46) * S, 166 * S, 40 * S);
}
static LRESULT CALLBACK NavProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT cr;
            GetClientRect(h, &cr);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bm = CreateCompatibleBitmap(dc, cr.right, cr.bottom);
            HGDIOBJ old = SelectObject(mem, bm);
            {
                Graphics g(mem);
                g.SetSmoothingMode(SmoothingModeAntiAlias);
                g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
                const float S = A.g.S;
                SolidBrush bg(Gc(g_cPanel));
                g.FillRectangle(&bg, 0, 0, cr.right, cr.bottom);
                FontFamily ff(L"Segoe UI"), fi(L"Segoe MDL2 Assets");
                Font fTitle(&ff, 17 * S, FontStyleBold, UnitPixel), fSub(&ff, 11 * S, FontStyleRegular, UnitPixel),
                    fItem(&ff, 14 * S, FontStyleRegular, UnitPixel), fIcon(&fi, 17 * S, FontStyleRegular, UnitPixel);
                DrawTxt(g, L"Win-Dock", fTitle, RectF(24 * S, 20 * S, 160 * S, 26 * S), Gc(g_cText));
                DrawTxt(g, std::wstring(T(L"Paramètres", L"Settings")), fSub,
                        RectF(24 * S, 46 * S, 160 * S, 20 * S), Gc(g_cMuted));
                for (int i = 0; i < kPages; ++i) {
                    RectF r = NavItemRect(i);
                    bool sel = i == g_navSel, hot = i == g_navHot;
                    if (sel) {
                        FillRR(g, r, 9 * S, Gc(g_cAccent, 46));
                        FillRR(g, RectF(r.X, r.Y + 9 * S, 3.5f * S, r.Height - 18 * S), 2 * S, Gc(g_cAccent));
                    } else if (hot) {
                        FillRR(g, r, 9 * S, Gc(g_cText, 18));
                    }
                    Color tc = sel ? Gc(g_cText) : Gc(Mix(g_cMuted, g_cText, hot ? 55 : 15));
                    DrawTxt(g, kNavGlyph[i], fIcon, RectF(r.X + 12 * S, r.Y, 28 * S, r.Height), sel ? Gc(g_cAccent) : tc,
                            StringAlignmentCenter);
                    DrawTxt(g, NavLabel(i), fItem, RectF(r.X + 48 * S, r.Y, r.Width - 52 * S, r.Height), tc);
                }
                DrawTxt(g, L"v" + std::wstring(kVersionW), fSub, RectF(24 * S, cr.bottom - 36 * S, 150 * S, 20 * S), Gc(g_cMuted));
            }
            BitBlt(dc, 0, 0, cr.right, cr.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bm);
            DeleteDC(mem);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            int hot = -1;
            float x = (float)(short)LOWORD(lp), y = (float)(short)HIWORD(lp);
            for (int i = 0; i < kPages; ++i)
                if (NavItemRect(i).Contains(x, y)) hot = i;
            if (hot != g_navHot) { g_navHot = hot; InvalidateRect(h, nullptr, FALSE); }
            TRACKMOUSEEVENT te = {sizeof(te), TME_LEAVE, h, 0};
            TrackMouseEvent(&te);
            return 0;
        }
        case WM_MOUSELEAVE:
            if (g_navHot != -1) { g_navHot = -1; InvalidateRect(h, nullptr, FALSE); }
            return 0;
        case WM_SETCURSOR:
            SetCursor(LoadCursorW(nullptr, g_navHot >= 0 ? IDC_HAND : IDC_ARROW));
            return TRUE;
        case WM_LBUTTONDOWN:
            if (g_navHot >= 0) ShowPage(g_navHot);
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// ---------------------------------------------------------------------------------------------
// Galerie de thèmes
// ---------------------------------------------------------------------------------------------
struct GalCard {
    ThemeInfo ti;
    Theme th;
    Bitmap* ic[5] = {};
    RectF r;
};
struct GalHdr {
    std::wstring text;
    float y;
};
static std::vector<GalCard> g_cards;
static std::vector<GalHdr> g_hdrs;
static float g_galH = 0;
static int g_galScroll = 0, g_galHot = -1;
static HWND g_gal = nullptr;

static void GalFree() {
    for (auto& c : g_cards)
        for (auto*& b : c.ic) { delete b; b = nullptr; }
    g_cards.clear();
    g_hdrs.clear();
}
static Bitmap* LoadIcon64(const std::wstring& p) {
    if (!PathFileExistsW(p.c_str())) return nullptr;
    Bitmap src(p.c_str());
    if (src.GetLastStatus() != Ok) return nullptr;
    Bitmap* o = new Bitmap(64, 64, PixelFormat32bppPARGB);
    Graphics g(o);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.Clear(Color(0, 0, 0, 0));
    g.DrawImage(&src, Rect(0, 0, 64, 64));
    return o;
}
static void GalLayout() {
    if (!g_gal) return;
    RECT cr;
    GetClientRect(g_gal, &cr);
    const float S = A.g.S;
    float W = (float)cr.right, margin = 24 * S, gap = 16 * S, cardH = 178 * S, hdrH = 42 * S;
    int cols = std::max(1, (int)((W - 2 * margin + gap) / (210 * S + gap)));
    float cw = (W - 2 * margin - (cols - 1) * gap) / cols;
    g_hdrs.clear();
    std::wstring last;
    float y = 6 * S;
    int col = 0;
    for (auto& c : g_cards) {
        std::wstring cat = Lower(c.ti.category);
        if (cat != last) {
            if (col > 0) { y += cardH + gap; col = 0; }
            last = cat;
            g_hdrs.push_back({CategoryLabel(c.ti.category), y});
            y += hdrH;
        }
        c.r = RectF(margin + col * (cw + gap), y, cw, cardH);
        if (++col >= cols) { col = 0; y += cardH + gap; }
    }
    if (col > 0) y += cardH + gap;
    g_galH = y + 10 * S;
    SCROLLINFO si = {sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS};
    si.nMin = 0;
    si.nMax = (int)g_galH;
    si.nPage = cr.bottom;
    g_galScroll = std::max(0, std::min(g_galScroll, std::max(0, (int)g_galH - (int)cr.bottom)));
    si.nPos = g_galScroll;
    SetScrollInfo(g_gal, SB_VERT, &si, TRUE);
    InvalidateRect(g_gal, nullptr, FALSE);
}
static void GalRebuild() {
    GalFree();
    for (const ThemeInfo& ti : EnumThemes()) {
        GalCard c;
        c.ti = ti;
        c.th = LoadThemeEx(ti.folder, false);
        std::wstring cat = Lower(ti.category);
        const wchar_t* k3 = cat == L"ia" ? L"ai" : (cat == L"games" ? L"games" : L"terminal");
        const wchar_t* k4 = cat == L"ia" ? L"chat" : (cat == L"games" ? L"music" : L"browser");
        const wchar_t* keys[5] = {L"computer", L"folder", k3, k4, L"settings"};
        for (int i = 0; i < 5; ++i)
            c.ic[i] = LoadIcon64(ThemesRoot() + L"\\" + ti.folder + L"\\Icons\\" + keys[i] + L".png");
        g_cards.push_back(std::move(c));
    }
    GalLayout();
}
static void GalPaintCard(Graphics& g, const GalCard& c, bool hot, bool active) {
    const float S = A.g.S;
    RectF r = c.r;
    FillRR(g, r, 12 * S, Gc(Mix(g_cPanel, g_cText, hot ? 8 : 0)));
    if (active) StrokeRR(g, RectF(r.X + 1, r.Y + 1, r.Width - 2, r.Height - 2), 12 * S, Gc(g_cAccent), 2.2f * S);
    else StrokeRR(g, RectF(r.X + 0.5f, r.Y + 0.5f, r.Width - 1, r.Height - 1), 12 * S, Gc(Mix(g_cPanel, g_cText, hot ? 30 : 12)), 1.0f);
    // aperçu : fond de bureau + barre du thème
    RectF pv(r.X + 10 * S, r.Y + 10 * S, r.Width - 20 * S, 82 * S);
    const Theme& t = c.th;
    {
        GraphicsPath pp;
        RRPath(pp, pv, 8 * S);
        LinearGradientBrush bgb(PointF(pv.X, pv.Y), PointF(pv.X + pv.Width, pv.Y + pv.Height), Color(255, t.uiBg.GetR(), t.uiBg.GetG(), t.uiBg.GetB()),
                                Color(255, (BYTE)std::min(255, t.uiBg.GetR() + 30), (BYTE)std::min(255, t.uiBg.GetG() + 30), (BYTE)std::min(255, t.uiBg.GetB() + 40)));
        g.FillPath(&bgb, &pp);
    }
    RectF bar(pv.X + 10 * S, pv.Y + (pv.Height - 50 * S) / 2 + 6 * S, pv.Width - 20 * S, 50 * S);
    {
        float rad = std::min(bar.Height * t.cornerR, bar.Height / 2);
        GraphicsPath bp;
        RRPath(bp, bar, rad);
        LinearGradientBrush gb(PointF(bar.X, bar.Y), PointF(bar.X, bar.Y + bar.Height), Color(255, t.barTop.GetR(), t.barTop.GetG(), t.barTop.GetB()),
                               Color(255, t.barBot.GetR(), t.barBot.GetG(), t.barBot.GetB()));
        if (t.glow.GetR() || t.glow.GetG() || t.glow.GetB()) {
            RectF gl(bar.X - 2 * S, bar.Y - 2 * S, bar.Width + 4 * S, bar.Height + 4 * S);
            StrokeRR(g, gl, rad + 2 * S, Color(60, t.glow.GetR(), t.glow.GetG(), t.glow.GetB()), 3 * S);
        }
        g.FillPath(&gb, &bp);
        Pen pen(Color(210, t.border1.GetR(), t.border1.GetG(), t.border1.GetB()), std::max(0.8f, t.borderW * S * 0.8f));
        g.DrawPath(&pen, &bp);
    }
    float pad = 10 * S, gap = 4 * S;
    float isz = std::min(34 * S, (bar.Width - 2 * pad - 4 * gap) / 5);
    float ix = bar.X + (bar.Width - (5 * isz + 4 * gap)) / 2, iy = bar.Y + (bar.Height - isz) / 2;
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    for (int i = 0; i < 5; ++i) {
        float x = ix + i * (isz + gap);
        if (c.ic[i]) g.DrawImage(c.ic[i], RectF(x, iy, isz, isz), 0, 0, 64, 64, UnitPixel);
        else FillRR(g, RectF(x, iy, isz, isz), 7 * S, Color(120, t.accent.GetR(), t.accent.GetG(), t.accent.GetB()));
    }
    FontFamily ff(L"Segoe UI"), fi(L"Segoe MDL2 Assets");
    Font fName(&ff, 15 * S, FontStyleBold, UnitPixel), fDesc(&ff, 11.5f * S, FontStyleRegular, UnitPixel);
    DrawTxt(g, c.ti.name, fName, RectF(r.X + 12 * S, r.Y + 98 * S, r.Width - 48 * S, 22 * S), Gc(g_cText));
    DrawTxt(g, c.ti.desc, fDesc, RectF(r.X + 12 * S, r.Y + 122 * S, r.Width - 24 * S, 50 * S), Gc(g_cMuted),
            StringAlignmentNear, StringAlignmentNear, true);
    if (active) {
        float d = 22 * S;
        RectF bd(r.X + r.Width - d - 10 * S, r.Y + 98 * S, d, d);
        SolidBrush ab(Gc(g_cAccent));
        g.FillEllipse(&ab, bd);
        Font fck(&fi, 12 * S, FontStyleRegular, UnitPixel);
        DrawTxt(g, L"", fck, bd, IsLightColor(g_cAccent) ? Color(255, 20, 20, 24) : Color(255, 255, 255, 255),
                StringAlignmentCenter);
    }
}
static int GalHit(int x, int y) {
    for (size_t i = 0; i < g_cards.size(); ++i)
        if (g_cards[i].r.Contains((float)x, (float)(y + g_galScroll))) return (int)i;
    return -1;
}
static void GalScrollTo(int pos) {
    RECT cr;
    GetClientRect(g_gal, &cr);
    pos = std::max(0, std::min(pos, std::max(0, (int)g_galH - (int)cr.bottom)));
    if (pos == g_galScroll) return;
    g_galScroll = pos;
    SetScrollPos(g_gal, SB_VERT, pos, TRUE);
    InvalidateRect(g_gal, nullptr, FALSE);
}
static LRESULT CALLBACK GalProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND: return 1;
        case WM_SIZE: GalLayout(); return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT cr;
            GetClientRect(h, &cr);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bm = CreateCompatibleBitmap(dc, cr.right, cr.bottom);
            HGDIOBJ old = SelectObject(mem, bm);
            {
                Graphics g(mem);
                g.SetSmoothingMode(SmoothingModeAntiAlias);
                g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
                SolidBrush bg(Gc(g_cBg));
                g.FillRectangle(&bg, 0, 0, cr.right, cr.bottom);
                g.TranslateTransform(0, (REAL)-g_galScroll);
                const float S = A.g.S;
                FontFamily ff(L"Segoe UI");
                Font fH(&ff, 13 * S, FontStyleBold, UnitPixel);
                for (auto& hd : g_hdrs) {
                    DrawTxt(g, hd.text, fH, RectF(24 * S, hd.y + 6 * S, 400 * S, 24 * S), Gc(g_cAccent));
                    Pen lp2(Gc(g_cText, 26), 1.0f);
                    g.DrawLine(&lp2, (REAL)(24 * S + 110 * S), (REAL)(hd.y + 19 * S), (REAL)(cr.right - 24 * S), (REAL)(hd.y + 19 * S));
                }
                for (size_t i = 0; i < g_cards.size(); ++i) {
                    const GalCard& c = g_cards[i];
                    if (c.r.Y + c.r.Height < g_galScroll || c.r.Y > g_galScroll + cr.bottom) continue;
                    GalPaintCard(g, c, (int)i == g_galHot, Lower(c.ti.folder) == Lower(A.set.theme));
                }
            }
            BitBlt(dc, 0, 0, cr.right, cr.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bm);
            DeleteDC(mem);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            int hot = GalHit((short)LOWORD(lp), (short)HIWORD(lp));
            if (GetFocus() != h) SetFocus(h);
            if (hot != g_galHot) { g_galHot = hot; InvalidateRect(h, nullptr, FALSE); }
            TRACKMOUSEEVENT te = {sizeof(te), TME_LEAVE, h, 0};
            TrackMouseEvent(&te);
            return 0;
        }
        case WM_MOUSELEAVE:
            if (g_galHot != -1) { g_galHot = -1; InvalidateRect(h, nullptr, FALSE); }
            return 0;
        case WM_SETCURSOR:
            SetCursor(LoadCursorW(nullptr, g_galHot >= 0 ? IDC_HAND : IDC_ARROW));
            return TRUE;
        case WM_LBUTTONUP: {
            int i = GalHit((short)LOWORD(lp), (short)HIWORD(lp));
            if (i >= 0 && i < (int)g_cards.size() && Lower(g_cards[i].ti.folder) != Lower(A.set.theme))
                ApplyTheme(g_cards[i].ti.folder);
            return 0;
        }
        case WM_MOUSEWHEEL:
            GalScrollTo(g_galScroll - (int)((short)HIWORD(wp) * (int)(60 * A.g.S) / WHEEL_DELTA));
            return 0;
        case WM_VSCROLL: {
            RECT cr;
            GetClientRect(h, &cr);
            int pos = g_galScroll;
            switch (LOWORD(wp)) {
                case SB_LINEUP: pos -= SC(40); break;
                case SB_LINEDOWN: pos += SC(40); break;
                case SB_PAGEUP: pos -= cr.bottom; break;
                case SB_PAGEDOWN: pos += cr.bottom; break;
                case SB_THUMBTRACK:
                case SB_THUMBPOSITION: pos = HIWORD(wp); break;
            }
            GalScrollTo(pos);
            return 0;
        }
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// ---------------------------------------------------------------------------------------------
// Pages
// ---------------------------------------------------------------------------------------------
static LRESULT CALLBACK PageProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND:
        case WM_HSCROLL:
        case WM_NOTIFY:
        case WM_DRAWITEM:
            return SendMessageW(GetParent(h), msg, wp, lp);
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC dc = (HDC)wp;
            SetTextColor(dc, g_cText);
            SetBkColor(dc, g_cPanel);
            return (LRESULT)g_brPanel;
        }
        case WM_CTLCOLORSTATIC: {
            HDC dc = (HDC)wp;
            int id = GetDlgCtrlID((HWND)lp);
            if (id == IDC_U_LOG) {
                SetTextColor(dc, g_cText);
                SetBkColor(dc, g_cPanel);
                return (LRESULT)g_brPanel;
            }
            SetTextColor(dc, id == IDC_HINT ? g_cMuted : g_cText);
            SetBkColor(dc, g_cBg);
            return (LRESULT)g_brBg;
        }
        case WM_CTLCOLORBTN: {
            SetBkColor((HDC)wp, g_cBg);
            return (LRESULT)g_brBg;
        }
        case WM_ERASEBKGND: {
            RECT r;
            GetClientRect(h, &r);
            FillRect((HDC)wp, &r, g_brBg);
            return 1;
        }
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static void BuildPageThemes(HWND p) {
    g_dx = 0; g_dy = 0;
    HWND hd = Mk(p, L"STATIC", T(L"Thèmes", L"Themes"), SS_LEFT, 24, 16, 400, 34, IDC_HDR);
    if (g_fontH) SendMessageW(hd, WM_SETFONT, (WPARAM)g_fontH, TRUE);
    Hint(p, T(L"Un clic applique le thème : barre, icônes et cette fenêtre changent aussitôt.",
              L"One click applies the theme: dock bar, icons and this window update at once."),
         24, 52, 560, 20);
    Btn(p, T(L"Dossier Themes", L"Themes folder"), 452, 14, 130, IDC_GAL_FOLDER, 30);
    Btn(p, T(L"Actualiser", L"Refresh"), 588, 14, 100, IDC_GAL_REFRESH, 30);
    g_gal = CreateWindowExW(0, L"RDMGallery", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPSIBLINGS, 0, SC(84), SC(710), SC(620 - 84),
                            p, (HMENU)(INT_PTR)IDC_GAL, A.hInst, nullptr);
}

static void ShowPage(int navIdx) {
    if (navIdx < 0 || navIdx >= kPages) return;
    g_navSel = navIdx;
    for (int i = 0; i < kPages; ++i) ShowWindow(g_pg[i], kNavPage[navIdx] == i ? SW_SHOW : SW_HIDE);
    if (g_nav) InvalidateRect(g_nav, nullptr, FALSE);
}

static void Header(HWND p, const wchar_t* title) {
    g_dx = 0; g_dy = 0;
    HWND h = Mk(p, L"STATIC", title, SS_LEFT, 24, 16, 560, 34, IDC_HDR);
    if (g_fontH) SendMessageW(h, WM_SETFONT, (WPARAM)g_fontH, TRUE);
    g_dx = 24; g_dy = 64;
}

static void BuildPageAppearance(HWND p) {
    Header(p, T(L"Apparence", L"Appearance"));
    Lbl(p, T(L"Position à l'écran", L"Screen position"), 14, 18, 170);
    Cmb(p, 190, 14, 200, IDC_POS, {T(L"Haut", L"Top"), T(L"Gauche", L"Left"), T(L"Bas", L"Bottom"), T(L"Droite", L"Right")});
    Lbl(p, T(L"Thème", L"Theme"), 14, 52, 170);
    Cmb(p, 190, 48, 290, IDC_THEME, {});
    Lbl(p, T(L"Écran", L"Display"), 14, 86, 170);
    HWND mc = Cmb(p, 190, 82, 200, IDC_MON, {});
    std::vector<MONITORINFO> mons;
    EnumDisplayMonitors(nullptr, nullptr, MonCb, (LPARAM)&mons);
    for (size_t i = 0; i < std::max<size_t>(1, mons.size()); ++i) {
        std::wstring n = ItoW((int)i + 1) + (i == 0 ? std::wstring(T(L" (principal)", L" (primary)")) : L"");
        SendMessageW(mc, CB_ADDSTRING, 0, (LPARAM)n.c_str());
    }
    Lbl(p, T(L"Police des noms", L"Label font"), 14, 120, 170);
    Edt(p, 190, 116, 200, IDC_FONT);
    Slider(p, IDC_ICONMIN, T(L"Taille des icônes (repos)", L"Icon size (rest)"), 160, 24, 128);
    Slider(p, IDC_ICONMAX, T(L"Taille maximale (zoom)", L"Maximum size (zoom)"), 198, 24, 256);
    Slider(p, IDC_ZOOM, T(L"Rayon du zoom (icônes)", L"Zoom radius (icons)"), 236, 1, 8);
    Slider(p, IDC_OFFSET, T(L"Décalage le long du bord", L"Offset along the edge"), 274, -600, 600);
    Slider(p, IDC_THOP, T(L"Opacité de la barre", L"Bar opacity"), 312, 20, 100);
    Slider(p, IDC_ICOP, T(L"Opacité des icônes", L"Icon opacity"), 350, 20, 100);
}

static void BuildPageBehavior(HWND p) {
    Header(p, T(L"Comportement", L"Behavior"));
    Chk(p, T(L"Masquer automatiquement le dock", L"Auto-hide the dock"), 14, 14, 420, IDC_AUTOHIDE);
    Chk(p, T(L"Afficher le nom des icônes au survol", L"Show icon labels on hover"), 14, 42, 420, IDC_SHOWLABELS);
    Chk(p, T(L"Verrouiller les icônes (pas de déplacement)", L"Lock icons (no dragging)"), 14, 70, 420, IDC_LOCK);
    Chk(p, T(L"Indiquer les applications ouvertes", L"Show running applications"), 14, 98, 420, IDC_RUNNING);
    Chk(p, T(L"Un clic sur une application ouverte l'affiche au premier plan", L"Clicking a running app brings it to front"), 14, 126, 540, IDC_OPENRUN);
    Chk(p, T(L"Lancer le dock au démarrage de Windows", L"Start the dock with Windows"), 14, 154, 420, IDC_AUTOSTART);
    Chk(p, T(L"Dock flottant : le placer où l'on veut sur le bureau", L"Floating dock: place it anywhere on the desktop"), 14, 182, 540, IDC_FREEMODE);
    Chk(p, T(L"Verrouiller la position du dock", L"Lock the dock position"), 14, 210, 540, IDC_LOCKDOCK);
    Slider(p, IDC_AHDELAY, T(L"Délai avant masquage", L"Hide delay"), 254, 0, 3000);
    Lbl(p, T(L"Raccourci afficher / masquer", L"Show / hide shortcut"), 14, 300, 175);
    Edt(p, 190, 296, 170, IDC_HOTKEY);
    Btn(p, T(L"Appliquer", L"Apply"), 370, 295, 100, IDC_HKAPPLY);
    Hint(p, T(L"Exemples : Control+Alt+R, Win+F9, Shift+Alt+D", L"Examples: Control+Alt+R, Win+F9, Shift+Alt+D"), 190, 326, 400);
    Hint(p, T(L"Déplacer le dock : glissez le fond de la barre (ou Alt + glisser). Près d'un bord il s'ancre ; ailleurs il flotte. "
              L"Déposez des raccourcis ou programmes depuis le bureau / l'Explorateur pour les ajouter.",
              L"Move the dock: drag the bar background (or Alt + drag). Near an edge it snaps; elsewhere it floats. "
              L"Drop shortcuts or programs from the desktop / Explorer to add them."),
         14, 362, 600, 60);
}

static void BuildPageItems(HWND p) {
    Header(p, T(L"Éléments du dock", L"Dock items"));
    HWND lv = Mk(p, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP, 12, 10, 590, 170, IDC_LIST);
    SendMessageW(lv, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    LVCOLUMNW c = {};
    c.mask = LVCF_TEXT | LVCF_WIDTH;
    c.cx = SC(170);
    c.pszText = (LPWSTR)T(L"Titre", L"Title");
    SendMessageW(lv, LVM_INSERTCOLUMNW, 0, (LPARAM)&c);
    c.cx = SC(400);
    c.pszText = (LPWSTR)T(L"Commande", L"Command");
    SendMessageW(lv, LVM_INSERTCOLUMNW, 1, (LPARAM)&c);
    Btn(p, T(L"Ajouter un programme…", L"Add a program…"), 12, 188, 150, IDC_ADD);
    Btn(p, T(L"Séparateur", L"Separator"), 168, 188, 90, IDC_ADDSEP);
    Btn(p, T(L"Supprimer", L"Remove"), 264, 188, 90, IDC_DEL);
    Btn(p, T(L"Monter", L"Up"), 428, 188, 80, IDC_UP);
    Btn(p, T(L"Descendre", L"Down"), 514, 188, 88, IDC_DOWN);
    Lbl(p, T(L"Titre", L"Title"), 14, 236, 100);       Edt(p, 120, 232, 482, IDC_E_TITLE);
    Lbl(p, T(L"Commande", L"Command"), 14, 266, 100);  Edt(p, 120, 262, 380, IDC_E_CMD);   Btn(p, L"…", 508, 261, 94, IDC_B_CMD, 26);
    Lbl(p, T(L"Arguments", L"Arguments"), 14, 296, 100); Edt(p, 120, 292, 482, IDC_E_ARGS);
    Lbl(p, T(L"Dossier de travail", L"Working folder"), 14, 326, 105); Edt(p, 120, 322, 482, IDC_E_WD);
    Lbl(p, T(L"Icône", L"Icon"), 14, 356, 100);        Edt(p, 120, 352, 380, IDC_E_ICON);  Btn(p, L"…", 508, 351, 94, IDC_B_ICON, 26);
    Lbl(p, T(L"Fenêtre", L"Window"), 14, 386, 100);
    Cmb(p, 120, 382, 200, IDC_E_SHOW, {T(L"Normale", L"Normal"), T(L"Réduite", L"Minimized"), T(L"Agrandie", L"Maximized")});
    Btn(p, T(L"Appliquer à l'élément", L"Apply to item"), 440, 380, 162, IDC_APPLYITEM);
    Hint(p, T(L"Icône vide = icône du programme. Icône du thème : theme:folder.  Tuile : glyph:E8B7:F6B04A:E3801C", L"Empty icon = program icon. Theme icon: theme:folder. Tile: glyph:E8B7:F6B04A:E3801C"),
        120, 412, 482, 36);
}

static void BuildPageUpdate(HWND p) {
    Header(p, T(L"Mise à jour", L"Update"));
    Lbl(p, T(L"Version installée", L"Installed version"), 14, 18, 170);
    Lbl(p, kVersionW, 190, 18, 200);
    Lbl(p, T(L"Version disponible", L"Available version"), 14, 46, 170);
    Mk(p, L"STATIC", L"—", SS_LEFT, 190, 46, 300, 20, IDC_U_REMOTE);
    Btn(p, T(L"Vérifier", L"Check"), 14, 82, 130, IDC_U_CHECK, 32);
    Btn(p, T(L"Mettre à jour maintenant", L"Update now"), 152, 82, 230, IDC_U_UPDATE, 30);
    Btn(p, T(L"Ouvrir le dossier", L"Open folder"), 390, 82, 140, IDC_U_FOLDER, 30);
    Mk(p, L"EDIT", L"", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 14, 126, 620, 150, IDC_U_LOG);
    Hint(p, T(L"La mise à jour télécharge les fichiers depuis GitHub (agentia-GIT/Windows-Dock) et remplace l'exécutable et les "
             L"fichiers listés dans manifest.txt. Settings.ini n'est jamais modifié : toute votre configuration est conservée. "
             L"Le dock redémarre ensuite automatiquement.",
             L"The update downloads files from GitHub (agentia-GIT/Windows-Dock) and replaces the executable and the files "
             L"listed in manifest.txt. Settings.ini is never modified: your whole configuration is kept. "
             L"The dock then restarts automatically."),
        14, 290, 580, 90);
}

static void LogUpd(const std::wstring& s) {
    HWND e = GetDlgItem(g_pg[3], IDC_U_LOG);
    int n = GetWindowTextLengthW(e);
    SendMessageW(e, EM_SETSEL, n, n);
    std::wstring l = s + L"\r\n";
    SendMessageW(e, EM_REPLACESEL, FALSE, (LPARAM)l.c_str());
}


static void ApplyIconOpacity() {
    if (!A.ia) return;
    if (A.set.iconOpacity >= 100) A.ia->ClearColorMatrix();
    else {
        ColorMatrix cm = {};
        for (int d = 0; d < 5; ++d) cm.m[d][d] = 1.0f;
        cm.m[3][3] = A.set.iconOpacity / 100.0f;
        A.ia->SetColorMatrix(&cm);
    }
    A.dirty = true;
}

static void OnSlider(HWND ctl, WPARAM wp) {
    int id = GetDlgCtrlID(ctl);
    int v = (int)SendMessageW(ctl, TBM_GETPOS, 0, 0);
    HWND a = g_pg[0], b = g_pg[1];
    Settings& s = A.set;
    switch (id) {
        case IDC_ICONMIN:
            s.iconMin = v;
            if (s.iconMax < v) { s.iconMax = v; SetSlider(a, IDC_ICONMAX, v, L" px"); }
            SetSlider(a, id, v, L" px"); RecalcGeometry(); break;
        case IDC_ICONMAX:
            if (v < s.iconMin) { v = s.iconMin; }
            s.iconMax = v; SetSlider(a, id, v, L" px"); RecalcGeometry(); break;
        case IDC_ZOOM: s.zoomWidth = v; SetSlider(a, id, v, L""); RecalcGeometry(); break;
        case IDC_OFFSET: s.offset = v; SetSlider(a, id, v, L" px"); RecalcGeometry(); break;
        case IDC_THOP: s.themeOpacity = v; SetSlider(a, id, v, L" %"); A.dirty = true; break;
        case IDC_ICOP: s.iconOpacity = v; SetSlider(a, id, v, L" %"); ApplyIconOpacity(); break;
        case IDC_AHDELAY: s.autoHideDelay = v; SetSlider(b, id, v, L" ms"); break;
        default: return;
    }
    if (LOWORD(wp) != TB_THUMBTRACK) SaveSettings();  // pas d'écriture disque pendant le glissement
}

static void OnCommand(HWND h, int id, int code, HWND ctl) {
    HWND b = g_pg[1], it = g_pg[2];
    Settings& s = A.set;
    wchar_t buf[2048];
    if (IsSwitchId(id) && code == BN_CLICKED) ChkSet(ctl, !ChkGet(ctl));
    switch (id) {
        case IDC_POS:
            if (code == CBN_SELCHANGE) { s.side = (int)SendMessageW(ctl, CB_GETCURSEL, 0, 0); RecalcGeometry(); SaveSettings(); }
            break;
        case IDC_THEME:
            if (code == CBN_SELCHANGE) {
                int i = (int)SendMessageW(ctl, CB_GETCURSEL, 0, 0);
                if (i >= 0 && i < (int)g_setThemes.size()) ApplyTheme(g_setThemes[i].folder);
            }
            break;
        case IDC_MON:
            if (code == CBN_SELCHANGE) { s.monitor = (int)SendMessageW(ctl, CB_GETCURSEL, 0, 0); RecalcGeometry(); SaveSettings(); }
            break;
        case IDC_FONT:
            if (code == EN_KILLFOCUS) {
                GetWindowTextW(ctl, buf, 256);
                std::wstring f = Trim(buf);
                if (!f.empty() && f != s.fontName) { s.fontName = f; RecalcGeometry(); SaveSettings(); }
            }
            break;
        case IDC_AUTOHIDE: s.autoHide = ChkGet(ctl); RecalcGeometry(); SaveSettings(); break;
        case IDC_SHOWLABELS: s.hideLabels = !ChkGet(ctl); A.dirty = true; SaveSettings(); break;
        case IDC_LOCK: s.lockIcons = ChkGet(ctl); SaveSettings(); break;
        case IDC_RUNNING: s.showRunning = ChkGet(ctl); A.dirty = true; SaveSettings(); break;
        case IDC_OPENRUN: s.openRunning = ChkGet(ctl); SaveSettings(); break;
        case IDC_LOCKDOCK: s.lockDock = ChkGet(ctl); SaveSettings(); break;
        case IDC_FREEMODE:
            s.free = ChkGet(ctl);
            if (s.free) { s.freeX = A.g.X; s.freeY = A.g.Y; }
            RecalcGeometry(); SaveSettings();
            break;
        case IDC_AUTOSTART: AutostartSet(ChkGet(ctl)); break;
        case IDC_HKAPPLY: {
            GetDlgItemTextW(b, IDC_HOTKEY, buf, 256);
            std::wstring hk = Trim(buf);
            UINT m, v;
            if (!ParseHotkey(hk, m, v)) {
                MessageBoxW(h, T(L"Raccourci invalide. Exemple : Control+Alt+R", L"Invalid shortcut. Example: Control+Alt+R"), kAppName, MB_OK | MB_ICONWARNING);
                break;
            }
            s.hotkey = hk;
            RegisterToggleHotkey();
            SaveSettings();
            break;
        }
        // --- Éléments ---
        case IDC_ADD: {
            std::wstring f;
            if (PickFile(h, L"Programmes et raccourcis\0*.exe;*.lnk;*.bat;*.cmd\0Tous les fichiers\0*.*\0\0", f)) {
                int sel = SelectedItem();
                AddPath(f, sel >= 0 ? sel + 1 : -1);
                FillList(sel >= 0 ? sel + 1 : (int)A.items.size() - 1);
                LoadFields();
            }
            break;
        }
        case IDC_ADDSEP: {
            int sel = SelectedItem();
            Item sp;
            sp.sep = true;
            int at = sel >= 0 ? sel + 1 : (int)A.items.size();
            A.items.insert(A.items.begin() + at, sp);
            RecalcGeometry(); SaveSettings();
            FillList(at); LoadFields();
            break;
        }
        case IDC_DEL: {
            int sel = SelectedItem();
            if (sel >= 0) { RemoveItem(sel); FillList(sel); LoadFields(); }
            break;
        }
        case IDC_UP:
        case IDC_DOWN: {
            int sel = SelectedItem();
            int to = id == IDC_UP ? sel - 1 : sel + 1;
            if (sel >= 0 && to >= 0 && to < (int)A.items.size()) {
                std::swap(A.items[sel], A.items[to]);
                RecalcGeometry(); SaveSettings();
                FillList(to); LoadFields();
            }
            break;
        }
        case IDC_B_CMD: {
            std::wstring f;
            if (PickFile(h, L"Tous les fichiers\0*.*\0\0", f)) SetDlgItemTextW(it, IDC_E_CMD, f.c_str());
            break;
        }
        case IDC_B_ICON: {
            std::wstring f;
            if (PickFile(h, L"Images et icônes\0*.png;*.ico;*.jpg;*.bmp;*.exe;*.dll\0Tous les fichiers\0*.*\0\0", f))
                SetDlgItemTextW(it, IDC_E_ICON, f.c_str());
            break;
        }
        case IDC_APPLYITEM: {
            int sel = SelectedItem();
            if (sel < 0 || sel >= (int)A.items.size() || A.items[sel].sep) break;
            Item& m = A.items[sel];
            auto get = [&](int cid) { GetDlgItemTextW(it, cid, buf, 2048); return Trim(buf); };
            m.title = get(IDC_E_TITLE);
            m.cmd = get(IDC_E_CMD);
            m.args = get(IDC_E_ARGS);
            m.wd = get(IDC_E_WD);
            m.file = get(IDC_E_ICON);
            m.showCmd = (int)SendMessageW(GetDlgItem(it, IDC_E_SHOW), CB_GETCURSEL, 0, 0);
            if (m.showCmd < 0) m.showCmd = 0;
            LoadItemImages(m);
            RecalcGeometry(); SaveSettings();
            FillList(sel);
            break;
        }
        // --- Mise à jour ---
        case IDC_U_CHECK:
            if (g_updBusy) break;
            SetDlgItemTextW(g_pg[3], IDC_U_REMOTE, L"…");
            StartUpdate(false);
            break;
        case IDC_U_UPDATE:
            if (g_updBusy) break;
            if (MessageBoxW(h, T(L"Télécharger la dernière version et remplacer les fichiers locaux ?\n\nVotre configuration (Settings.ini) sera conservée. "
                                 L"Le dock redémarrera automatiquement.",
                                 L"Download the latest version and replace local files?\n\nYour configuration (Settings.ini) will be kept. "
                                 L"The dock will restart automatically."),
                            kAppName, MB_YESNO | MB_ICONQUESTION) == IDYES) {
                SetDlgItemTextW(g_pg[3], IDC_U_LOG, L"");
                EnableWindow(GetDlgItem(g_pg[3], IDC_U_UPDATE), FALSE);
                StartUpdate(true);
            }
            break;
        case IDC_U_FOLDER: ShellExecuteW(nullptr, L"open", ExeDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
        case IDC_GAL_FOLDER: {
            CreateDirectoryW(ThemesRoot().c_str(), nullptr);
            ShellExecuteW(nullptr, L"open", ThemesRoot().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        }
        case IDC_GAL_REFRESH: GalRebuild(); FillThemeCombo(); break;
        case IDC_CLOSE: DestroyWindow(h); break;
    }
}

static BOOL CALLBACK StyleChild(HWND c, LPARAM) {
    wchar_t cls[64] = {};
    GetClassNameW(c, cls, 64);
    if (!wcscmp(cls, L"ComboBox")) {
        SetWindowTheme(c, g_uiDark ? L"DarkMode_CFD" : L"CFD", nullptr);
    } else if (!wcscmp(cls, L"SysListView32")) {
        SetWindowTheme(c, g_uiDark ? L"DarkMode_ItemsView" : L"Explorer", nullptr);
        ListView_SetBkColor(c, g_cPanel);
        ListView_SetTextBkColor(c, g_cPanel);
        ListView_SetTextColor(c, g_cText);
    } else if (!wcscmp(cls, L"msctls_trackbar32") || !wcscmp(cls, L"RDMGallery")) {
        SetWindowTheme(c, g_uiDark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    } else if (!wcscmp(cls, L"Edit") && GetDlgCtrlID(c) == IDC_U_LOG) {
        SetWindowTheme(c, g_uiDark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    }
    return TRUE;
}

// Appelée après tout changement de thème : couleurs, barre de titre, contrôles, galerie.
static void RefreshSettingsTheme() {
    if (!g_setWnd) return;
    UpdateUiColors();
    BOOL d = g_uiDark ? TRUE : FALSE;
    DwmSetWindowAttribute(g_setWnd, 20, &d, sizeof(d));
    DwmSetWindowAttribute(g_setWnd, 19, &d, sizeof(d));
    COLORREF cap = g_cBg, txt = g_cText;
    DwmSetWindowAttribute(g_setWnd, 35, &cap, sizeof(cap));  // couleur de barre de titre (Windows 11)
    DwmSetWindowAttribute(g_setWnd, 36, &txt, sizeof(txt));
    EnumChildWindows(g_setWnd, StyleChild, 0);
    FillThemeCombo();
    RedrawWindow(g_setWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
}

static LRESULT CALLBACK SettingsProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND: {
            RECT r;
            GetClientRect(h, &r);
            FillRect((HDC)wp, &r, g_brBg);
            return 1;
        }
        case WM_COMMAND:
            OnCommand(h, LOWORD(wp), HIWORD(wp), (HWND)lp);
            return 0;
        case WM_DRAWITEM:
            OnDrawItem((DRAWITEMSTRUCT*)lp);
            return TRUE;
        case WM_HSCROLL:
            OnSlider((HWND)lp, wp);
            return 0;
        case WM_NOTIFY: {
            NMHDR* n = (NMHDR*)lp;
            if (n->idFrom == IDC_LIST && n->code == LVN_ITEMCHANGED && !g_filling) {
                NMLISTVIEW* nl = (NMLISTVIEW*)lp;
                if ((nl->uChanged & LVIF_STATE) && (nl->uNewState & LVIS_SELECTED)) LoadFields();
            }
            return 0;
        }
        case WM_UPD: {
            std::wstring* s = (std::wstring*)lp;
            switch (wp) {
                case 0: LogUpd(*s); break;
                case 1: {
                    SetDlgItemTextW(g_pg[3], IDC_U_REMOTE, s->c_str());
                    LogUpd(std::wstring(T(L"Version disponible : ", L"Available version: ")) + *s);
                    break;
                }
                case 2: {
                    LogUpd(std::wstring(T(L"Mise à jour terminée (version ", L"Update complete (version ")) + *s + L"). " +
                           T(L"Redémarrage…", L"Restarting…"));
                    wchar_t exe[MAX_PATH * 2];
                    GetModuleFileNameW(nullptr, exe, MAX_PATH * 2);
                    ShellExecuteW(nullptr, L"open", exe, L"--restart", ExeDir().c_str(), SW_SHOWNORMAL);
                    delete s;
                    DestroyWindow(A.hwnd);  // le nouveau processus attend la fin de celui-ci
                    return 0;
                }
                case 3:
                    LogUpd(std::wstring(T(L"ÉCHEC : ", L"FAILED: ")) + *s);
                    EnableWindow(GetDlgItem(g_pg[3], IDC_U_UPDATE), TRUE);
                    break;
            }
            delete s;
            return 0;
        }
        case WM_ACTIVATE:
            if (LOWORD(wp) != WA_INACTIVE) SyncControls(false);  // reflète les changements faits depuis le dock
            return 0;
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            g_setWnd = nullptr;
            g_nav = nullptr;
            g_gal = nullptr;
            GalFree();
            for (auto& p : g_pg) p = nullptr;
            if (g_font) { DeleteObject(g_font); g_font = nullptr; }
            if (g_fontH) { DeleteObject(g_fontH); g_fontH = nullptr; }
            if (g_fontS) { DeleteObject(g_fontS); g_fontS = nullptr; }
            if (g_brBg) { DeleteObject(g_brBg); g_brBg = nullptr; }
            if (g_brPanel) { DeleteObject(g_brPanel); g_brPanel = nullptr; }
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static void OpenSettings() {
    if (g_setWnd) {
        ShowWindow(g_setWnd, SW_SHOW);
        SetForegroundWindow(g_setWnd);
        return;
    }
    INITCOMMONCONTROLSEX ic = {sizeof(ic), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&ic);
    static bool reg = false;
    if (!reg) {
        reg = true;
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.hInstance = A.hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(A.hInst, MAKEINTRESOURCEW(2));
        wc.lpfnWndProc = SettingsProc;
        wc.lpszClassName = L"RDMSettings";
        RegisterClassExW(&wc);
        wc.hIcon = nullptr;
        wc.lpfnWndProc = PageProc;
        wc.lpszClassName = L"RDMPage";
        RegisterClassExW(&wc);
        wc.lpfnWndProc = NavProc;
        wc.lpszClassName = L"RDMNav";
        RegisterClassExW(&wc);
        wc.lpfnWndProc = GalProc;
        wc.lpszClassName = L"RDMGallery";
        RegisterClassExW(&wc);
    }
    UpdateUiColors();
    const int dpi = (int)(96 * A.g.S + 0.5f);
    g_font = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontH = CreateFontW(-MulDiv(19, dpi, 72), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI Semibold");
    g_fontS = CreateFontW(-MulDiv(10, dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    RECT rc = {0, 0, SC(900), SC(620)};
    DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    AdjustWindowRectEx(&rc, style, FALSE, WS_EX_TOPMOST);
    int w = rc.right - rc.left, hgt = rc.bottom - rc.top;
    int sx = GetSystemMetrics(SM_CXSCREEN), sy = GetSystemMetrics(SM_CYSCREEN);
    std::wstring title = std::wstring(kAppName) + L" - " + T(L"Paramètres", L"Settings");
    g_setWnd = CreateWindowExW(WS_EX_TOPMOST, L"RDMSettings", title.c_str(), style, (sx - w) / 2, std::max(0, (sy - hgt) / 2), w, hgt,
                               nullptr, nullptr, A.hInst, nullptr);
    if (!g_setWnd) return;
    HWND h = g_setWnd;
    g_nav = CreateWindowExW(0, L"RDMNav", L"", WS_CHILD | WS_VISIBLE, 0, 0, SC(190), SC(620), h, (HMENU)(INT_PTR)IDC_NAV, A.hInst, nullptr);
    for (int i = 0; i < kPages; ++i)
        g_pg[i] = CreateWindowExW(0, L"RDMPage", L"", WS_CHILD | WS_CLIPCHILDREN, SC(190), 0, SC(710), SC(620), h, nullptr, A.hInst, nullptr);
    g_dx = g_dy = 0;
    BuildPageAppearance(g_pg[0]);
    BuildPageBehavior(g_pg[1]);
    BuildPageItems(g_pg[2]);
    BuildPageUpdate(g_pg[3]);
    BuildPageThemes(g_pg[4]);
    g_dx = g_dy = 0;
    GalRebuild();
    RefreshSettingsTheme();
    SyncControls();
    g_navSel = 0;
    ShowPage(0);
    ShowWindow(h, SW_SHOW);
    SetForegroundWindow(h);
}
