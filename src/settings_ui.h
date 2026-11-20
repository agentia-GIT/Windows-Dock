// settings_ui.h - Fenêtre "Paramètres" (réglages complets, éléments du dock, mise à jour directe).
// Inclus par main.cpp : utilise l'état global A et les fonctions du dock.
#pragma once
#include <commctrl.h>
#include <winhttp.h>

static const wchar_t* kVersionW = L"2.1.0";
static const wchar_t* kDefaultUpdateUrl = L"https://raw.githubusercontent.com/agentia-GIT/Windows-Dock/main/dist/";

enum {
    IDC_TAB = 1000,
    IDC_ICONMIN = 1101, IDC_ICONMAX, IDC_ZOOM, IDC_THOP, IDC_ICOP, IDC_OFFSET, IDC_AHDELAY,
    IDC_POS = 1110, IDC_THEME, IDC_MON, IDC_FONT, IDC_HOTKEY, IDC_HKAPPLY,
    IDC_AUTOHIDE = 1120, IDC_SHOWLABELS, IDC_LOCK, IDC_RUNNING, IDC_OPENRUN, IDC_AUTOSTART,
    IDC_LIST = 1130, IDC_ADD, IDC_ADDSEP, IDC_DEL, IDC_UP, IDC_DOWN,
    IDC_E_TITLE = 1140, IDC_E_CMD, IDC_E_ARGS, IDC_E_WD, IDC_E_ICON, IDC_B_CMD, IDC_B_ICON, IDC_E_SHOW, IDC_APPLYITEM,
    IDC_U_LOCAL = 1150, IDC_U_REMOTE, IDC_U_CHECK, IDC_U_UPDATE, IDC_U_LOG, IDC_U_FOLDER,
    IDC_CLOSE = 1160
};
static const int kValueLabelOffset = 500;  // le libellé de valeur d'un curseur a l'ID du curseur + 500
enum { WM_UPD = WM_APP + 1 };               // wParam : 0 statut, 1 version distante, 2 succès, 3 échec

static HWND g_tab = nullptr, g_pg[4] = {};
static HFONT g_font = nullptr;
static bool g_filling = false;
static volatile LONG g_updBusy = 0;

static int SC(int v) { return (int)(v * A.g.S + 0.5f); }

// ---------------------------------------------------------------------------------------------
// Outils de création de contrôles
// ---------------------------------------------------------------------------------------------
static HWND Mk(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id,
               DWORD ex = 0) {
    HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, SC(x), SC(y), SC(w), SC(h), parent,
                             (HMENU)(INT_PTR)id, A.hInst, nullptr);
    if (c && g_font) SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    return c;
}
static HWND Lbl(HWND p, const wchar_t* t, int x, int y, int w, int h = 20) { return Mk(p, L"STATIC", t, SS_LEFT, x, y, w, h, 0); }
static HWND Btn(HWND p, const wchar_t* t, int x, int y, int w, int id, int h = 26) {
    return Mk(p, L"BUTTON", t, BS_PUSHBUTTON | WS_TABSTOP, x, y, w, h, id);
}
static HWND Chk(HWND p, const wchar_t* t, int x, int y, int w, int id) {
    return Mk(p, L"BUTTON", t, BS_AUTOCHECKBOX | WS_TABSTOP, x, y, w, 22, id);
}
static HWND Edt(HWND p, int x, int y, int w, int id) {
    return Mk(p, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, x, y, w, 24, id, WS_EX_CLIENTEDGE);
}
static HWND Cmb(HWND p, int x, int y, int w, int id, std::initializer_list<const wchar_t*> items) {
    HWND c = Mk(p, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, x, y, w, 200, id);
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
        on = RegQueryValueExW(k, L"RocketDockModern", nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
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
        RegSetValueExW(k, L"RocketDockModern", 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(k, L"RocketDockModern");
    }
    RegCloseKey(k);
}

// ---------------------------------------------------------------------------------------------
// Mise à jour directe (WinHTTP)
// ---------------------------------------------------------------------------------------------
static bool HttpGet(const std::wstring& url, const std::wstring& token, std::string& out, DWORD& status) {
    status = 0;
    out.clear();
    wchar_t host[256] = {}, path[2048] = {};
    URL_COMPONENTSW uc = {};
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) return false;
    HINTERNET s = WinHttpOpen(L"RocketDockModern/2.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) s = WinHttpOpen(L"RocketDockModern/2.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS, 0);
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
        WinHttpCloseHandle(r);
    }
    if (c) WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return ok;
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
        InterlockedExchange(&g_updBusy, 0);
        delete job;
        return (DWORD)0;
    };
    UpdPost(0, fr ? L"Connexion à GitHub…" : L"Connecting to GitHub…");
    if (!HttpGet(job->base + L"version.txt", job->token, body, st) || st != 200)
        return fail((fr ? L"Impossible de lire la version distante (HTTP " : L"Cannot read remote version (HTTP ") +
                    ItoW((int)st) + L").");
    std::wstring remote = TrimW(Utf8ToW(body));
    UpdPost(1, remote);
    if (!job->apply) {
        InterlockedExchange(&g_updBusy, 0);
        delete job;
        return 0;
    }
    if (!HttpGet(job->base + L"manifest.txt", job->token, body, st) || st != 200)
        return fail(fr ? L"Liste des fichiers (manifest.txt) introuvable." : L"File list (manifest.txt) not found.");

    // Liste des fichiers à mettre à jour
    struct F { std::wstring rel, tmp, dst; bool isExe; };
    std::vector<F> files;
    {
        std::wstring txt = Utf8ToW(body);
        size_t pos = 0;
        const std::wstring mine = (sizeof(void*) == 8) ? L"rocketdockmodern_x64.exe" : L"rocketdockmodern_x86.exe";
        const std::wstring other = (sizeof(void*) == 8) ? L"rocketdockmodern_x86.exe" : L"rocketdockmodern_x64.exe";
        while (pos <= txt.size()) {
            size_t e = txt.find(L'\n', pos);
            std::wstring line = TrimW(txt.substr(pos, e == std::wstring::npos ? e : e - pos));
            pos = (e == std::wstring::npos) ? txt.size() + 1 : e + 1;
            if (line.empty() || line[0] == L'#') continue;
            std::replace(line.begin(), line.end(), L'\\', L'/');
            std::wstring low = Lower(line);
            if (line.find(L"..") != std::wstring::npos || line[0] == L'/' || line.find(L':') != std::wstring::npos) continue;
            if (low == L"settings.ini") continue;  // la configuration n'est jamais remplacée
            if (low == other) continue;
            F f;
            f.rel = line;
            f.isExe = (low == mine);
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
        if (!HttpGet(job->base + f.rel, job->token, body, st) || st != 200 || body.empty()) {
            for (auto& g : files) DeleteFileW(g.tmp.c_str());
            return fail((fr ? L"Échec du téléchargement : " : L"Download failed: ") + f.rel);
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
            return fail((fr ? L"Écriture impossible : " : L"Cannot write: ") + f.dst);
        }
    }

    // Remplacement : fichiers ordinaires d'abord, exécutable en dernier (renommé en .old puis remplacé)
    UpdPost(0, fr ? L"Remplacement des fichiers…" : L"Replacing files…");
    for (auto& f : files) {
        if (f.isExe) continue;
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

// full = true : tout recharger (ouverture) ; false : réactivation (ne touche pas aux champs en cours d'édition)
static void SyncControls(bool full = true) {
    if (!g_setWnd) return;
    const Settings& s = A.set;
    HWND a = g_pg[0], b = g_pg[1];
    SendMessageW(GetDlgItem(a, IDC_POS), CB_SETCURSEL, s.side, 0);
    int th = 0;
    for (int i = 0; i < 3; ++i) if (Lower(s.theme) == Lower(kThemes[i])) th = i;
    SendMessageW(GetDlgItem(a, IDC_THEME), CB_SETCURSEL, th, 0);
    SendMessageW(GetDlgItem(a, IDC_MON), CB_SETCURSEL, std::max(0, s.monitor), 0);
    if (full || GetFocus() != GetDlgItem(a, IDC_FONT)) SetDlgItemTextW(a, IDC_FONT, s.fontName.c_str());
    SetSlider(a, IDC_ICONMIN, s.iconMin, L" px");
    SetSlider(a, IDC_ICONMAX, s.iconMax, L" px");
    SetSlider(a, IDC_ZOOM, s.zoomWidth, L"");
    SetSlider(a, IDC_OFFSET, s.offset, L" px");
    SetSlider(a, IDC_THOP, s.themeOpacity, L" %");
    SetSlider(a, IDC_ICOP, s.iconOpacity, L" %");
    SendMessageW(GetDlgItem(b, IDC_AUTOHIDE), BM_SETCHECK, s.autoHide, 0);
    SendMessageW(GetDlgItem(b, IDC_SHOWLABELS), BM_SETCHECK, !s.hideLabels, 0);
    SendMessageW(GetDlgItem(b, IDC_LOCK), BM_SETCHECK, s.lockIcons, 0);
    SendMessageW(GetDlgItem(b, IDC_RUNNING), BM_SETCHECK, s.showRunning, 0);
    SendMessageW(GetDlgItem(b, IDC_OPENRUN), BM_SETCHECK, s.openRunning, 0);
    SendMessageW(GetDlgItem(b, IDC_AUTOSTART), BM_SETCHECK, AutostartGet(), 0);
    SetSlider(b, IDC_AHDELAY, s.autoHideDelay, L" ms");
    if (full || GetFocus() != GetDlgItem(b, IDC_HOTKEY)) SetDlgItemTextW(b, IDC_HOTKEY, s.hotkey.c_str());
    if (full || ListIsStale()) {
        int sel = SelectedItem();
        FillList(sel < 0 ? 0 : sel);
        LoadFields();
    }
}

// ---------------------------------------------------------------------------------------------
// Construction de la fenêtre
// ---------------------------------------------------------------------------------------------
static LRESULT CALLBACK PageProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND:
        case WM_HSCROLL:
        case WM_NOTIFY:
            return SendMessageW(GetParent(h), msg, wp, lp);
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC dc = (HDC)wp;
            SetBkColor(dc, GetSysColor(COLOR_WINDOW));
            return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
        }
        case WM_ERASEBKGND: {
            RECT r;
            GetClientRect(h, &r);
            FillRect((HDC)wp, &r, GetSysColorBrush(COLOR_WINDOW));
            return 1;
        }
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static void BuildPageAppearance(HWND p) {
    Lbl(p, T(L"Position à l'écran", L"Screen position"), 14, 18, 170);
    Cmb(p, 190, 14, 200, IDC_POS, {T(L"Haut", L"Top"), T(L"Gauche", L"Left"), T(L"Bas", L"Bottom"), T(L"Droite", L"Right")});
    Lbl(p, T(L"Thème", L"Theme"), 14, 52, 170);
    Cmb(p, 190, 48, 200, IDC_THEME, {T(L"Sombre", L"Dark"), T(L"Clair", L"Light"), T(L"Aurore", L"Aurora")});
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
    Chk(p, T(L"Masquer automatiquement le dock", L"Auto-hide the dock"), 14, 14, 420, IDC_AUTOHIDE);
    Chk(p, T(L"Afficher le nom des icônes au survol", L"Show icon labels on hover"), 14, 42, 420, IDC_SHOWLABELS);
    Chk(p, T(L"Verrouiller les icônes (pas de déplacement)", L"Lock icons (no dragging)"), 14, 70, 420, IDC_LOCK);
    Chk(p, T(L"Indiquer les applications ouvertes", L"Show running applications"), 14, 98, 420, IDC_RUNNING);
    Chk(p, T(L"Un clic sur une application ouverte l'affiche au premier plan", L"Clicking a running app brings it to front"), 14, 126, 540, IDC_OPENRUN);
    Chk(p, T(L"Lancer le dock au démarrage de Windows", L"Start the dock with Windows"), 14, 154, 420, IDC_AUTOSTART);
    Slider(p, IDC_AHDELAY, T(L"Délai avant masquage", L"Hide delay"), 194, 0, 3000);
    Lbl(p, T(L"Raccourci afficher / masquer", L"Show / hide shortcut"), 14, 240, 175);
    Edt(p, 190, 236, 170, IDC_HOTKEY);
    Btn(p, T(L"Appliquer", L"Apply"), 370, 235, 100, IDC_HKAPPLY);
    Lbl(p, T(L"Exemples : Control+Alt+R, Win+F9, Shift+Alt+D", L"Examples: Control+Alt+R, Win+F9, Shift+Alt+D"), 190, 266, 400);
}

static void BuildPageItems(HWND p) {
    HWND lv = Mk(p, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP, 12, 10, 590, 170, IDC_LIST,
                 WS_EX_CLIENTEDGE);
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
    Lbl(p, T(L"Icône vide = icône du programme. Tuile générée : glyph:E8B7:F6B04A:E3801C", L"Empty icon = program icon. Generated tile: glyph:E8B7:F6B04A:E3801C"),
        120, 412, 482, 36);
}

static void BuildPageUpdate(HWND p) {
    Lbl(p, T(L"Version installée", L"Installed version"), 14, 18, 170);
    Lbl(p, kVersionW, 190, 18, 200);
    Lbl(p, T(L"Version disponible", L"Available version"), 14, 46, 170);
    Mk(p, L"STATIC", L"—", SS_LEFT, 190, 46, 300, 20, IDC_U_REMOTE);
    Btn(p, T(L"Vérifier", L"Check"), 14, 82, 130, IDC_U_CHECK, 30);
    Btn(p, T(L"Mettre à jour maintenant", L"Update now"), 152, 82, 230, IDC_U_UPDATE, 30);
    Btn(p, T(L"Ouvrir le dossier", L"Open folder"), 390, 82, 140, IDC_U_FOLDER, 30);
    Mk(p, L"EDIT", L"", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 14, 126, 580, 150, IDC_U_LOG, WS_EX_CLIENTEDGE);
    Lbl(p, T(L"La mise à jour télécharge les fichiers depuis GitHub (agentia-GIT/Windows-Dock) et remplace l'exécutable et les "
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

static void ShowPage(int idx) {
    for (int i = 0; i < 4; ++i) ShowWindow(g_pg[i], i == idx ? SW_SHOW : SW_HIDE);
}

static void LayoutPages(HWND h) {
    RECT r;
    GetWindowRect(g_tab, &r);
    MapWindowPoints(nullptr, h, (POINT*)&r, 2);
    TabCtrl_AdjustRect(g_tab, FALSE, &r);
    for (int i = 0; i < 4; ++i) MoveWindow(g_pg[i], r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
}

// ---------------------------------------------------------------------------------------------
// Traitement des commandes
// ---------------------------------------------------------------------------------------------
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
    switch (id) {
        case IDC_POS:
            if (code == CBN_SELCHANGE) { s.side = (int)SendMessageW(ctl, CB_GETCURSEL, 0, 0); RecalcGeometry(); SaveSettings(); }
            break;
        case IDC_THEME:
            if (code == CBN_SELCHANGE) {
                int i = (int)SendMessageW(ctl, CB_GETCURSEL, 0, 0);
                if (i >= 0 && i < 3) { s.theme = kThemes[i]; A.th = MakeTheme(s.theme); ApplyMenuTheme(); A.dirty = true; SaveSettings(); }
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
        case IDC_AUTOHIDE: s.autoHide = SendMessageW(ctl, BM_GETCHECK, 0, 0) == BST_CHECKED; RecalcGeometry(); SaveSettings(); break;
        case IDC_SHOWLABELS: s.hideLabels = SendMessageW(ctl, BM_GETCHECK, 0, 0) != BST_CHECKED; A.dirty = true; SaveSettings(); break;
        case IDC_LOCK: s.lockIcons = SendMessageW(ctl, BM_GETCHECK, 0, 0) == BST_CHECKED; SaveSettings(); break;
        case IDC_RUNNING: s.showRunning = SendMessageW(ctl, BM_GETCHECK, 0, 0) == BST_CHECKED; A.dirty = true; SaveSettings(); break;
        case IDC_OPENRUN: s.openRunning = SendMessageW(ctl, BM_GETCHECK, 0, 0) == BST_CHECKED; SaveSettings(); break;
        case IDC_AUTOSTART: AutostartSet(SendMessageW(ctl, BM_GETCHECK, 0, 0) == BST_CHECKED); break;
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
        case IDC_CLOSE: DestroyWindow(h); break;
    }
}

static LRESULT CALLBACK SettingsProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND:
            OnCommand(h, LOWORD(wp), HIWORD(wp), (HWND)lp);
            return 0;
        case WM_HSCROLL:
            OnSlider((HWND)lp, wp);
            return 0;
        case WM_NOTIFY: {
            NMHDR* n = (NMHDR*)lp;
            if (n->idFrom == IDC_TAB && n->code == TCN_SELCHANGE) ShowPage(TabCtrl_GetCurSel(g_tab));
            else if (n->idFrom == IDC_LIST && n->code == LVN_ITEMCHANGED && !g_filling) {
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
            g_tab = nullptr;
            for (auto& p : g_pg) p = nullptr;
            if (g_font) { DeleteObject(g_font); g_font = nullptr; }
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
    INITCOMMONCONTROLSEX ic = {sizeof(ic), ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&ic);
    static bool reg = false;
    if (!reg) {
        reg = true;
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.hInstance = A.hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(A.hInst, MAKEINTRESOURCEW(2));
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpfnWndProc = SettingsProc;
        wc.lpszClassName = L"RDMSettings";
        RegisterClassExW(&wc);
        wc.lpfnWndProc = PageProc;
        wc.hbrBackground = nullptr;
        wc.hIcon = nullptr;
        wc.lpszClassName = L"RDMPage";
        RegisterClassExW(&wc);
    }
    g_font = CreateFontW(-MulDiv(9, (int)(96 * A.g.S + 0.5f), 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                         CLEARTYPE_QUALITY, 0, L"Segoe UI");
    RECT rc = {0, 0, SC(640), SC(580)};
    DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    AdjustWindowRectEx(&rc, style, FALSE, WS_EX_TOPMOST);
    int w = rc.right - rc.left, hgt = rc.bottom - rc.top;
    int sx = GetSystemMetrics(SM_CXSCREEN), sy = GetSystemMetrics(SM_CYSCREEN);
    std::wstring title = std::wstring(kAppName) + L" - " + T(L"Paramètres", L"Settings");
    g_setWnd = CreateWindowExW(WS_EX_TOPMOST, L"RDMSettings", title.c_str(), style, (sx - w) / 2, (sy - hgt) / 2, w, hgt,
                               nullptr, nullptr, A.hInst, nullptr);
    if (!g_setWnd) return;
    HWND h = g_setWnd;
    g_tab = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP, SC(8), SC(8), SC(624),
                            SC(522), h, (HMENU)(INT_PTR)IDC_TAB, A.hInst, nullptr);
    SendMessageW(g_tab, WM_SETFONT, (WPARAM)g_font, TRUE);
    const wchar_t* names[] = {T(L"Apparence", L"Appearance"), T(L"Comportement", L"Behavior"), T(L"Éléments", L"Items"),
                              T(L"Mise à jour", L"Update")};
    for (int i = 0; i < 4; ++i) {
        TCITEMW ti = {};
        ti.mask = TCIF_TEXT;
        ti.pszText = (LPWSTR)names[i];
        SendMessageW(g_tab, TCM_INSERTITEMW, i, (LPARAM)&ti);
        g_pg[i] = CreateWindowExW(0, L"RDMPage", L"", WS_CHILD | WS_CLIPCHILDREN, 0, 0, 10, 10, h, nullptr, A.hInst, nullptr);
    }
    BuildPageAppearance(g_pg[0]);
    BuildPageBehavior(g_pg[1]);
    BuildPageItems(g_pg[2]);
    BuildPageUpdate(g_pg[3]);
    Btn(h, T(L"Fermer", L"Close"), 530, 540, 100, IDC_CLOSE, 28);
    // le bouton Fermer est créé avec des coordonnées en pixels logiques par Btn()
    LayoutPages(h);
    ShowPage(0);
    SyncControls();
    ShowWindow(h, SW_SHOW);
    SetForegroundWindow(h);
}
