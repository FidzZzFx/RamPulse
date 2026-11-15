#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define NOMINMAX
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <windowsx.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <urlmon.h>
#include <algorithm>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <ctime>
using std::min;
using std::max;
#include <gdiplus.h>
using namespace Gdiplus;
#include "lang.h"

#define WM_CLEAN_DONE (WM_APP + 1)
#define WM_TRAY       (WM_APP + 2)
#define WM_SHOWMAIN   (WM_APP + 3)
#define WM_QRIS_DONE  (WM_APP + 4)
static const wchar_t* APP_NAME   = L"RamPulse";
static const wchar_t* APP_CLASS  = L"RamPulseMainWnd";
static const wchar_t* URL_GITHUB = L"https://github.com/FidzZzFx";
static const wchar_t* URL_QRIS   = L"https://raw.githubusercontent.com/FidzZzFx/pictures/main/qris.jpg";
static const wchar_t* LANG_NAME[LANG_COUNT] = { L"English", L"Bahasa Indonesia", L"中文", L"日本語", L"한국어", L"Español", L"Français", L"Deutsch", L"Русский", L"Português" };
static const long long QUIET_NOTIFY_SEC = 1800;
enum { ID_TRAY_OPEN = 101, ID_TRAY_CLEAN, ID_TRAY_PAUSE, ID_TRAY_EXIT };
enum { TIMER_SEC = 1, TIMER_ANIM = 2 };
enum { PG_DASH, PG_CLEAN, PG_HIST, PG_SET, PG_ABOUT, PG_COUNT };
enum { TRIG_MANUAL, TRIG_INTERVAL, TRIG_HIGH };

struct Settings {
    bool autoClean = false; int ival = 10; int unit = 1;
    bool highUsage = false; int highPct = 85;
    bool deferBusy = true;
    bool paused = false; int resumeMin = 0;
    bool startup = false, startMin = true, toTray = true;
    bool logging = true, notify = true, confirm = false, dark = true;
    int lang = 0; bool langChosen = false;
};
static Settings g_set;
static std::wstring g_dir, g_ini, g_histFile, g_qrisPath;

static const wchar_t* Tr(int id) { return kText[id][g_set.lang]; }

static std::wstring F(const wchar_t* fmt, ...) {
    wchar_t b[512]; va_list a; va_start(a, fmt); _vsnwprintf(b, 511, fmt, a); va_end(a); b[511] = 0; return b;
}
static std::wstring FmtBytes(double b) {
    if (b >= 1073741824.0) return F(Tr(T_SIZE_GB), b / 1073741824.0);
    if (b >= 1048576.0)    return F(Tr(T_SIZE_MB), b / 1048576.0);
    return F(Tr(T_SIZE_KB), b / 1024.0);
}
static std::wstring FmtDur(long long s) {
    if (s < 0) s = 0;
    if (s >= 3600) return F(Tr(T_DUR_HM), s / 3600, (s % 3600) / 60);
    if (s >= 60)   return F(Tr(T_DUR_MS), s / 60, s % 60);
    return F(Tr(T_DUR_S), s);
}
static std::wstring TimeStr(time_t t, bool withDate = true) {
    struct tm* tmv = localtime(&t); wchar_t b[64] = L"-";
    if (tmv) wcsftime(b, 64, withDate ? L"%Y-%m-%d %H:%M:%S" : L"%H:%M:%S", tmv);
    return b;
}
static const wchar_t* TrigName(int t) {
    return t == TRIG_MANUAL ? Tr(T_TRIG_MANUAL) : t == TRIG_INTERVAL ? Tr(T_TRIG_INTERVAL) : Tr(T_TRIG_HIGH);
}
static std::wstring ErrText(DWORD e) {
    if (e == ERROR_ACCESS_DENIED) return Tr(T_ERR_ACCESS);
    if (e == ERROR_INVALID_PARAMETER) return Tr(T_ERR_INVALID);
    wchar_t buf[256] = {0};
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 0, e, 0, buf, 255, 0);
    std::wstring s = buf;
    while (!s.empty() && (s.back() == L'\r' || s.back() == L'\n' || s.back() == L' ')) s.pop_back();
    return s + L" " + F(Tr(T_ERR_CODE), (unsigned long)e);
}

static void InitPaths() {
    wchar_t p[MAX_PATH] = {0};
    SHGetFolderPathW(0, CSIDL_APPDATA, 0, 0, p);
    g_dir = std::wstring(p) + L"\\RamPulse";
    CreateDirectoryW(g_dir.c_str(), 0);
    g_ini = g_dir + L"\\settings.ini";
    g_histFile = g_dir + L"\\history.tsv";
    g_qrisPath = g_dir + L"\\qris.jpg";
}
static int RI(const wchar_t* k, int d) { return (int)GetPrivateProfileIntW(L"RamPulse", k, d, g_ini.c_str()); }
static void WI(const wchar_t* k, int v) {
    wchar_t b[32]; _snwprintf(b, 32, L"%d", v); WritePrivateProfileStringW(L"RamPulse", k, b, g_ini.c_str());
}
static bool IsStartupOn() {
    HKEY k; if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &k) != 0) return false;
    LONG r = RegQueryValueExW(k, L"RamPulse", 0, 0, 0, 0); RegCloseKey(k); return r == 0;
}
static bool SetStartup(bool on) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, 0, 0, KEY_SET_VALUE, 0, &k, 0) != 0) return false;
    bool ok;
    if (on) {
        wchar_t exe[MAX_PATH]; GetModuleFileNameW(0, exe, MAX_PATH);
        std::wstring v = L"\"" + std::wstring(exe) + L"\" --startup";
        ok = RegSetValueExW(k, L"RamPulse", 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t))) == 0;
    } else {
        LONG r = RegDeleteValueW(k, L"RamPulse"); ok = (r == 0 || r == ERROR_FILE_NOT_FOUND);
    }
    RegCloseKey(k); return ok;
}
static void LoadSettings() {
    Settings d; Settings& s = g_set; s = d;
    s.autoClean = RI(L"AutoClean", d.autoClean); s.ival = RI(L"IntervalValue", d.ival); s.unit = RI(L"IntervalUnit", d.unit);
    s.highUsage = RI(L"HighUsage", d.highUsage); s.highPct = RI(L"HighPercent", d.highPct);
    s.deferBusy = RI(L"DeferBusy", d.deferBusy); s.paused = RI(L"Paused", d.paused); s.resumeMin = RI(L"ResumeMinutes", d.resumeMin);
    s.startMin = RI(L"StartMinimized", d.startMin); s.toTray = RI(L"MinimizeToTray", d.toTray);
    s.logging = RI(L"Logging", d.logging); s.notify = RI(L"Notify", d.notify); s.confirm = RI(L"Confirm", d.confirm); s.dark = RI(L"Dark", d.dark);
    s.lang = std::clamp(RI(L"Language", 0), 0, (int)LANG_COUNT - 1); s.langChosen = RI(L"LangChosen", 0) != 0;
    s.ival = std::clamp(s.ival, 1, 999); s.unit = std::clamp(s.unit, 0, 2); if (s.unit == 0 && s.ival < 5) s.ival = 5;
    s.highPct = std::clamp(s.highPct, 50, 98); s.resumeMin = std::clamp(s.resumeMin, 0, 240);
    s.startup = IsStartupOn();
}
static void SaveSettings() {
    Settings& s = g_set;
    WI(L"AutoClean", s.autoClean); WI(L"IntervalValue", s.ival); WI(L"IntervalUnit", s.unit);
    WI(L"HighUsage", s.highUsage); WI(L"HighPercent", s.highPct); WI(L"DeferBusy", s.deferBusy);
    WI(L"Paused", s.paused); WI(L"ResumeMinutes", s.resumeMin); WI(L"StartMinimized", s.startMin);
    WI(L"MinimizeToTray", s.toTray); WI(L"Logging", s.logging); WI(L"Notify", s.notify);
    WI(L"Confirm", s.confirm); WI(L"Dark", s.dark); WI(L"Language", s.lang); WI(L"LangChosen", s.langChosen ? 1 : 0);
}
static long long IntervalSec() { static const int m[3] = {1, 60, 3600}; return (long long)g_set.ival * m[g_set.unit]; }
static bool NotifyMuted() { return g_set.autoClean && IntervalSec() < QUIET_NOTIFY_SEC; }

struct SkipItem { std::wstring name; DWORD pid; DWORD err; };
struct Reduced  { std::wstring name; DWORD pid; ULONGLONG before, after; };
struct Record   { time_t t; int trig; int ok, skipped; ULONGLONG usedB, usedA, wsB, wsA; DWORD ms; };
struct CleanDetail { Record rec; std::vector<SkipItem> skips; std::vector<Reduced> top; };

static CleanDetail g_last; static bool g_hasLast = false;
static std::vector<Record> g_hist;
static volatile LONG g_busy = 0;
static bool g_admin = false;
static HWND g_hwnd;

static bool EnablePriv(const wchar_t* name) {
    HANDLE tok; if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok)) return false;
    TOKEN_PRIVILEGES tp; tp.PrivilegeCount = 1; tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    bool ok = LookupPrivilegeValueW(0, name, &tp.Privileges[0].Luid) &&
              AdjustTokenPrivileges(tok, FALSE, &tp, 0, 0, 0) && GetLastError() == ERROR_SUCCESS;
    CloseHandle(tok); return ok;
}
static bool CheckElevated() {
    HANDLE tok; if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) return false;
    TOKEN_ELEVATION e; DWORD n = 0; bool r = GetTokenInformation(tok, TokenElevation, &e, sizeof e, &n) && e.TokenIsElevated;
    CloseHandle(tok); return r;
}
static ULONGLONG UsedPhys() {
    MEMORYSTATUSEX m; m.dwLength = sizeof m; GlobalMemoryStatusEx(&m); return m.ullTotalPhys - m.ullAvailPhys;
}
static ULONGLONG WorkingSetOf(HANDLE h) {
    PROCESS_MEMORY_COUNTERS pmc; pmc.cb = sizeof pmc;
    return GetProcessMemoryInfo(h, &pmc, sizeof pmc) ? (ULONGLONG)pmc.WorkingSetSize : 0;
}
static CleanDetail* DoClean(int trig) {
    CleanDetail* d = new CleanDetail(); Record& r = d->rec; memset(&r, 0, sizeof r);
    DWORD t0 = GetTickCount(); r.t = time(nullptr); r.trig = trig; r.usedB = UsedPhys();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe; pe.dwSize = sizeof pe; DWORD self = GetCurrentProcessId();
        if (Process32FirstW(snap, &pe)) do {
            DWORD pid = pe.th32ProcessID; if (pid == 0 || pid == 4 || pid == self) continue;
            HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA, FALSE, pid);
            if (!h) {
                r.skipped++; if (d->skips.size() < 600) d->skips.push_back({pe.szExeFile, pid, GetLastError()}); continue;
            }
            ULONGLONG b = WorkingSetOf(h);
            if (EmptyWorkingSet(h)) {
                ULONGLONG a = WorkingSetOf(h); r.ok++; r.wsB += b; r.wsA += a;
                d->top.push_back({pe.szExeFile, pid, b, a});
            } else {
                r.skipped++; if (d->skips.size() < 600) d->skips.push_back({pe.szExeFile, pid, GetLastError()});
            }
            CloseHandle(h);
        } while (Process32NextW(snap, &pe));
        CloseHandle(snap);
    }
    r.usedA = UsedPhys(); r.ms = GetTickCount() - t0;
    std::sort(d->top.begin(), d->top.end(), [](const Reduced& a, const Reduced& b) {
        long long x = (long long)a.before - (long long)a.after, y = (long long)b.before - (long long)b.after; return x > y; });
    if (d->top.size() > 12) d->top.resize(12);
    return d;
}
static DWORD WINAPI CleanThread(LPVOID p) {
    CleanDetail* d = DoClean((int)(INT_PTR)p);
    PostMessageW(g_hwnd, WM_CLEAN_DONE, 0, (LPARAM)d); return 0;
}
static void StartClean(int trig) {
    if (InterlockedCompareExchange(&g_busy, 1, 0) != 0) return;
    HANDLE h = CreateThread(0, 0, CleanThread, (LPVOID)(INT_PTR)trig, 0, 0);
    if (h) CloseHandle(h); else InterlockedExchange(&g_busy, 0);
    if (g_hwnd) InvalidateRect(g_hwnd, 0, FALSE);
}
static void AppendHistory(const Record& r) {
    FILE* f = _wfopen(g_histFile.c_str(), L"a"); if (!f) return;
    fprintf(f, "%lld\t%d\t%d\t%d\t%llu\t%llu\t%llu\t%llu\t%lu\n", (long long)r.t, r.trig, r.ok, r.skipped,
            (unsigned long long)r.usedB, (unsigned long long)r.usedA, (unsigned long long)r.wsB, (unsigned long long)r.wsA, (unsigned long)r.ms);
    fclose(f);
}
static void LoadHistory() {
    g_hist.clear(); FILE* f = _wfopen(g_histFile.c_str(), L"r"); if (!f) return; char line[512];
    while (fgets(line, sizeof line, f)) {
        long long t; int trig, ok, sk; unsigned long long ub, ua, wb, wa; unsigned long ms;
        if (sscanf(line, "%lld %d %d %d %llu %llu %llu %llu %lu", &t, &trig, &ok, &sk, &ub, &ua, &wb, &wa, &ms) == 9) {
            Record r; r.t = (time_t)t; r.trig = trig; r.ok = ok; r.skipped = sk; r.usedB = ub; r.usedA = ua; r.wsB = wb; r.wsA = wa; r.ms = ms;
            g_hist.push_back(r);
        }
    }
    fclose(f); if (g_hist.size() > 1000) g_hist.erase(g_hist.begin(), g_hist.end() - 1000);
}

struct Stats { ULONGLONG total = 0, avail = 0; DWORD load = 0, procs = 0; double cpu = 0; };
static Stats g_st; static std::vector<float> g_ramHist;
static float g_gauge = 0, g_gaugeTarget = 0;
static ULONGLONG g_nextAuto = 0, g_resumeAt = 0; static long long g_lastThr = 0; static int g_deferCount = 0;

static ULONGLONG FT(const FILETIME& f) { return ((ULONGLONG)f.dwHighDateTime << 32) | f.dwLowDateTime; }
static void RefreshStats() {
    MEMORYSTATUSEX m; m.dwLength = sizeof m; GlobalMemoryStatusEx(&m);
    g_st.total = m.ullTotalPhys; g_st.avail = m.ullAvailPhys; g_st.load = m.dwMemoryLoad;
    PERFORMANCE_INFORMATION pi; pi.cb = sizeof pi; if (GetPerformanceInfo(&pi, sizeof pi)) g_st.procs = pi.ProcessCount;
    static ULONGLONG pi_, pk, pu; FILETIME i, k, u;
    if (GetSystemTimes(&i, &k, &u)) {
        ULONGLONG I = FT(i), K = FT(k), U_ = FT(u);
        if (pk || pu) { ULONGLONG tot = (K - pk) + (U_ - pu), idle = I - pi_; if (tot) g_st.cpu = 100.0 * (double)(tot - idle) / (double)tot; }
        pi_ = I; pk = K; pu = U_;
    }
    g_ramHist.push_back((float)g_st.load); if (g_ramHist.size() > 120) g_ramHist.erase(g_ramHist.begin());
    g_gaugeTarget = (float)g_st.load;
}
static void ResetSchedule() { g_nextAuto = GetTickCount64() + (ULONGLONG)IntervalSec() * 1000ULL; g_deferCount = 0; }

static NOTIFYICONDATAW g_nid; static UINT g_msgTaskbar; static HANDLE g_mutex; static bool g_exiting = false;
static HICON g_icon, g_iconSmall; static DWORD g_lastTipPct = 999;
static void TrayAdd() {
    memset(&g_nid, 0, sizeof g_nid); g_nid.cbSize = sizeof g_nid; g_nid.hWnd = g_hwnd; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; g_nid.uCallbackMessage = WM_TRAY; g_nid.hIcon = g_iconSmall;
    wcsncpy(g_nid.szTip, L"RamPulse", 63); Shell_NotifyIconW(NIM_ADD, &g_nid);
}
static void TrayTip() {
    if (g_lastTipPct == g_st.load) return;
    g_lastTipPct = g_st.load;
    NOTIFYICONDATAW n = g_nid; n.uFlags = NIF_TIP;
    std::wstring s = F(Tr(T_TIP_RAM), (unsigned long)g_st.load, g_set.paused ? Tr(T_TIP_PAUSED) : L"");
    wcsncpy(n.szTip, s.c_str(), 127); Shell_NotifyIconW(NIM_MODIFY, &n);
}
static void Notify(const wchar_t* title, const wchar_t* text) {
    NOTIFYICONDATAW n = g_nid; n.uFlags = NIF_INFO; wcsncpy(n.szInfoTitle, title, 63); wcsncpy(n.szInfo, text, 255);
    n.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND; Shell_NotifyIconW(NIM_MODIFY, &n);
}
static void ShowMain() {
    ShowWindow(g_hwnd, IsIconic(g_hwnd) ? SW_RESTORE : SW_SHOW); SetForegroundWindow(g_hwnd);
    RefreshStats(); InvalidateRect(g_hwnd, 0, FALSE);
}

enum { K_AUTO, K_HIGH, K_DEFER, K_PAUSE, K_STARTUP, K_STARTMIN, K_TRAY, K_LOG, K_NOTIFY, K_CONFIRM, K_DARK };
enum { ST_INT, ST_UNIT, ST_PCT, ST_RESUME };
static bool g_showPicker = false;
static bool* BoolRef(int k) {
    switch (k) {
        case K_AUTO: return &g_set.autoClean; case K_HIGH: return &g_set.highUsage; case K_DEFER: return &g_set.deferBusy;
        case K_PAUSE: return &g_set.paused; case K_STARTUP: return &g_set.startup; case K_STARTMIN: return &g_set.startMin;
        case K_TRAY: return &g_set.toTray; case K_LOG: return &g_set.logging; case K_NOTIFY: return &g_set.notify;
        case K_CONFIRM: return &g_set.confirm; case K_DARK: return &g_set.dark;
    } return nullptr;
}
static void ApplyTheme() {
    BOOL v = g_set.dark ? TRUE : FALSE; DwmSetWindowAttribute(g_hwnd, 20, &v, sizeof v);
}
static void ApplyLanguage() {
    if (g_hwnd) SetWindowTextW(g_hwnd, Tr(T_WIN_TITLE));
    g_lastTipPct = 999;
}
static void SetPaused(bool p) {
    g_set.paused = p;
    if (p) g_resumeAt = g_set.resumeMin > 0 ? GetTickCount64() + (ULONGLONG)g_set.resumeMin * 60000ULL : 0;
    else { g_resumeAt = 0; ResetSchedule(); }
    SaveSettings(); g_lastTipPct = 999;
}
static void ToggleSetting(int k) {
    bool* p = BoolRef(k); if (!p) return; bool nv = !*p;
    if (k == K_AUTO && nv && IntervalSec() < 60) {
        if (MessageBoxW(g_hwnd, Tr(T_MSG_SHORT_WARN), APP_NAME, MB_YESNO | MB_ICONWARNING) != IDYES) return;
    }
    if (k == K_PAUSE) { SetPaused(nv); return; }
    if (k == K_STARTUP) {
        if (!SetStartup(nv)) { MessageBoxW(g_hwnd, Tr(T_MSG_STARTUP_FAIL), APP_NAME, MB_OK | MB_ICONERROR); return; }
    }
    *p = nv;
    if (k == K_AUTO) ResetSchedule();
    if (k == K_DARK) ApplyTheme();
    SaveSettings();
}
static void StepSetting(int which, int dir) {
    switch (which) {
        case ST_INT: g_set.ival = std::clamp(g_set.ival + dir * (g_set.unit == 0 ? 5 : 1), 1, 999);
                     if (g_set.unit == 0 && g_set.ival < 5) g_set.ival = 5;
                     ResetSchedule(); break;
        case ST_UNIT: g_set.unit = (g_set.unit + 1) % 3;
                      if (g_set.unit == 0 && g_set.ival < 5) g_set.ival = 5;
                      ResetSchedule(); break;
        case ST_PCT: g_set.highPct = std::clamp(g_set.highPct + dir * 5, 50, 98); break;
        case ST_RESUME: g_set.resumeMin = std::clamp(g_set.resumeMin + dir * 5, 0, 240); break;
    }
    SaveSettings();
}
static void ResetDefaults() {
    if (MessageBoxW(g_hwnd, Tr(T_MSG_RESET), APP_NAME, MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    int lang = g_set.lang; bool chosen = g_set.langChosen;
    SetStartup(false); g_set = Settings(); g_set.lang = lang; g_set.langChosen = chosen;
    g_resumeAt = 0; ResetSchedule(); ApplyTheme(); SaveSettings(); g_lastTipPct = 999;
}
static void RequestClean(int trig) {
    if (g_busy) return;
    if (trig == TRIG_MANUAL && g_set.confirm &&
        MessageBoxW(g_hwnd, Tr(T_MSG_CONFIRM_CLEAN), APP_NAME, MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    StartClean(trig);
}
static void RestartAsAdmin() {
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(0, exe, MAX_PATH);
    CloseHandle(g_mutex); g_mutex = 0;
    HINSTANCE r = ShellExecuteW(g_hwnd, L"runas", exe, nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)r > 32) { g_exiting = true; DestroyWindow(g_hwnd); }
    else g_mutex = CreateMutexW(0, TRUE, L"Local\\RamPulse.SingleInstance");
}

static void OnSecondTick() {
    RefreshStats(); ULONGLONG now = GetTickCount64();
    if (g_set.paused && g_resumeAt && now >= g_resumeAt) { g_set.paused = false; g_resumeAt = 0; ResetSchedule(); SaveSettings(); }
    if (!g_set.paused && !g_busy) {
        bool due = false; int trig = TRIG_INTERVAL;
        if (g_set.autoClean && now >= g_nextAuto) due = true;
        else if (g_set.highUsage && g_st.load >= (DWORD)g_set.highPct && (long long)now >= g_lastThr + 60000) { due = true; trig = TRIG_HIGH; }
        if (due) {
            if (g_set.deferBusy && g_st.cpu > 80.0 && g_deferCount < 6) {
                g_deferCount++;
                if (trig == TRIG_INTERVAL) g_nextAuto = now + 10000; else g_lastThr = (long long)now - 50000;
            } else {
                g_deferCount = 0; StartClean(trig);
                if (trig == TRIG_INTERVAL) ResetSchedule(); else g_lastThr = (long long)now;
            }
        }
    }
    TrayTip();
}

static Bitmap* g_qris = nullptr;
static int g_qrisState = 0;
static DWORD WINAPI QrisThread(LPVOID) {
    std::wstring tmp = g_qrisPath + L".part";
    DeleteFileW(tmp.c_str());
    HRESULT hr = URLDownloadToFileW(nullptr, URL_QRIS, tmp.c_str(), 0, nullptr);
    bool ok = SUCCEEDED(hr) && GetFileAttributesW(tmp.c_str()) != INVALID_FILE_ATTRIBUTES;
    if (ok) ok = MoveFileExW(tmp.c_str(), g_qrisPath.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
    PostMessageW(g_hwnd, WM_QRIS_DONE, ok ? 1 : 0, 0);
    return 0;
}
static bool LoadQrisFile() {
    Bitmap* src = Bitmap::FromFile(g_qrisPath.c_str(), FALSE);
    if (!src) return false;
    if (src->GetLastStatus() != Ok) { delete src; return false; }
    Bitmap* copy = src->Clone(0, 0, (INT)src->GetWidth(), (INT)src->GetHeight(), PixelFormat32bppARGB);
    delete src;
    if (!copy) return false;
    if (copy->GetLastStatus() != Ok) { delete copy; return false; }
    if (g_qris) delete g_qris;
    g_qris = copy;
    return true;
}
static void EnsureQris() {
    if (g_qrisState == 1 || g_qrisState == 2) return;
    if (GetFileAttributesW(g_qrisPath.c_str()) != INVALID_FILE_ATTRIBUTES && LoadQrisFile()) { g_qrisState = 2; return; }
    g_qrisState = 1;
    HANDLE h = CreateThread(0, 0, QrisThread, 0, 0, 0);
    if (h) CloseHandle(h); else g_qrisState = 3;
}
static void RetryQris() {
    if (g_qrisState == 1) return;
    DeleteFileW(g_qrisPath.c_str());
    g_qrisState = 0;
    EnsureQris();
}

static float g_scale = 1.0f;
#define U(x) ((float)(x) * g_scale)
struct Theme { Color bg, side, card, card2, text, sub, accent, border, good, warn, bad, onAccent; };
static Theme TH() {
    if (g_set.dark) return { Color(255,20,21,23), Color(255,16,17,19), Color(255,28,29,32), Color(255,40,42,46), Color(255,236,234,229),
                              Color(255,142,145,150), Color(255,125,190,164), Color(255,47,49,54), Color(255,125,190,164), Color(255,226,170,86), Color(255,226,110,98), Color(255,14,26,21) };
    return { Color(255,245,243,239), Color(255,237,234,228), Color(255,252,251,248), Color(255,236,233,227), Color(255,32,33,36),
             Color(255,110,112,117), Color(255,38,120,98), Color(255,224,220,212), Color(255,38,120,98), Color(255,176,112,18), Color(255,196,66,56), Color(255,255,255,255) };
}
static Color Alpha(Color c, int a) { return Color((BYTE)a, c.GetR(), c.GetG(), c.GetB()); }
static Color Lighten(Color c, int d) {
    return Color(255, (BYTE)std::min(255, (int)c.GetR() + d), (BYTE)std::min(255, (int)c.GetG() + d), (BYTE)std::min(255, (int)c.GetB() + d));
}
struct Hit { RectF r; int id; int arg; RectF clip; };
enum { H_NAV = 1, H_CLEAN, H_TOGGLE, H_STEP, H_RESET, H_ADMIN, H_CLRHIST, H_OPENDIR, H_LANGPICK, H_LANGOPEN, H_LANGOK, H_GITHUB, H_QRRETRY, H_QROPEN };
static std::vector<Hit> g_hits; static RectF g_clip(0, 0, 0, 0);
static int g_hoverId = 0, g_hoverArg = -1; static int g_page = PG_DASH;
static float g_scroll[PG_COUNT] = {}, g_scrollMax[PG_COUNT] = {};
static void AddHit(RectF r, int id, int arg = 0) { g_hits.push_back({r, id, arg, g_clip}); }
static bool Hover(int id, int arg) { return g_hoverId == id && g_hoverArg == arg; }
static const Hit* HitAt(float x, float y) {
    for (auto it = g_hits.rbegin(); it != g_hits.rend(); ++it)
        if (it->r.Contains(x, y) && (it->clip.IsEmptyArea() || it->clip.Contains(x, y))) return &*it;
    return nullptr;
}
static void RRPath(GraphicsPath& p, RectF r, float rad) {
    float d = rad * 2; if (d > r.Height) d = r.Height; if (d > r.Width) d = r.Width;
    p.AddArc(r.X, r.Y, d, d, 180, 90); p.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
    p.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90); p.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90); p.CloseFigure();
}
static void FillRR(Graphics& g, RectF r, float rad, Color c) { GraphicsPath p; RRPath(p, r, rad); SolidBrush b(c); g.FillPath(&b, &p); }
static void StrokeRR(Graphics& g, RectF r, float rad, Color c, float w = 1.f) { GraphicsPath p; RRPath(p, r, rad); Pen pen(c, w); g.DrawPath(&pen, &p); }
static const wchar_t* FontNameFor(int l) {
    switch (l) {
        case 2: return L"Microsoft YaHei UI";
        case 3: return L"Yu Gothic UI";
        case 4: return L"Malgun Gothic";
        default: return L"Segoe UI";
    }
}
static const FontFamily* FamFor(int l) {
    static FontFamily* cache[LANG_COUNT] = {};
    if (!cache[l]) {
        FontFamily* f = new FontFamily(FontNameFor(l));
        if (!f->IsAvailable()) {
            delete f; f = new FontFamily(L"Segoe UI");
            if (!f->IsAvailable()) {
                delete f; f = new FontFamily(L"Arial");
                if (!f->IsAvailable()) { delete f; f = FontFamily::GenericSansSerif()->Clone(); }
            }
        }
        cache[l] = f;
    }
    return cache[l];
}
static const FontFamily* Fam() { return FamFor(g_set.lang); }
static void Text(Graphics& g, const std::wstring& s, RectF r, float px, bool bold, Color c,
                 StringAlignment h = StringAlignmentNear, StringAlignment v = StringAlignmentCenter, bool wrap = false,
                 const FontFamily* fam = nullptr) {
    Font f(fam ? fam : Fam(), px, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    StringFormat sf; sf.SetAlignment(h); sf.SetLineAlignment(v); sf.SetTrimming(StringTrimmingEllipsisCharacter);
    if (!wrap) sf.SetFormatFlags(StringFormatFlagsNoWrap);
    SolidBrush b(c); g.DrawString(s.c_str(), -1, &f, r, &sf, &b);
}
static float TextH(Graphics& g, const std::wstring& s, float width, float px, bool bold = false) {
    Font f(Fam(), px, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    StringFormat sf; RectF lay(0.f, 0.f, width, 10000.f), bb;
    g.MeasureString(s.c_str(), -1, &f, lay, &sf, &bb);
    return bb.Height;
}
static void Card(Graphics& g, RectF r, const Theme& t) { FillRR(g, r, U(10), t.card); StrokeRR(g, r, U(10), t.border); }
static void Hairline(Graphics& g, float x1, float x2, float y, const Theme& t) {
    Pen p(t.border, 1.f); g.DrawLine(&p, x1, y, x2, y);
}
static void Btn(Graphics& g, RectF r, const std::wstring& label, int id, int arg, const Theme& t, bool primary, bool enabled = true) {
    bool hv = enabled && Hover(id, arg);
    if (primary) FillRR(g, r, U(7), !enabled ? t.card2 : hv ? Lighten(t.accent, 16) : t.accent);
    else { if (hv) FillRR(g, r, U(7), t.card2); StrokeRR(g, r, U(7), hv ? t.sub : t.border); }
    Text(g, label, r, U(13), primary, !enabled ? t.sub : primary ? t.onAccent : t.text, StringAlignmentCenter);
    if (enabled) AddHit(r, id, arg);
}
static void Toggle(Graphics& g, float rx, float cy, bool on, int key, const Theme& t) {
    RectF r(rx - U(40), cy - U(11), U(40), U(22));
    Color tr = on ? t.accent : t.card2; if (Hover(H_TOGGLE, key)) tr = Lighten(tr, 14);
    FillRR(g, r, U(11), tr); if (!on) StrokeRR(g, r, U(11), t.border);
    float kx = on ? r.GetRight() - U(19) : r.X + U(3);
    SolidBrush kb(on ? t.onAccent : t.sub);
    g.FillEllipse(&kb, kx, r.Y + U(3), U(16), U(16)); AddHit(r, H_TOGGLE, key);
}
static float Stepper(Graphics& g, float rx, float cy, const std::wstring& val, int which, float valW, const Theme& t) {
    RectF plus(rx - U(30), cy - U(15), U(30), U(30)), valr(plus.X - valW, cy - U(15), valW, U(30)), minus(valr.X - U(30), cy - U(15), U(30), U(30));
    Btn(g, minus, L"-", H_STEP, which * 2, t, false); Btn(g, plus, L"+", H_STEP, which * 2 + 1, t, false);
    Text(g, val, valr, U(14), false, t.text, StringAlignmentCenter); return minus.X;
}
static void LogoMark(Graphics& g, float x, float y, float s, const Theme& t) {
    RectF r(x, y, s, s); FillRR(g, r, s * 0.24f, t.accent);
    Pen w(t.onAccent, s * 0.075f); w.SetStartCap(LineCapRound); w.SetEndCap(LineCapRound); w.SetLineJoin(LineJoinRound);
    PointF pts[6] = { {x + s * .16f, y + s * .56f}, {x + s * .34f, y + s * .56f}, {x + s * .44f, y + s * .27f},
                      {x + s * .57f, y + s * .76f}, {x + s * .66f, y + s * .5f}, {x + s * .84f, y + s * .5f} };
    g.DrawLines(&w, pts, 6);
}
static void NavIcon(Graphics& g, int kind, RectF r, Color c) {
    Pen p(c, U(1.6f)); p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound); p.SetLineJoin(LineJoinRound); SolidBrush b(c);
    float cx = r.X + r.Width / 2, cy = r.Y + r.Height / 2, s = U(8);
    if (kind == PG_DASH) {
        float q = s * 0.82f, gp = U(2.6f);
        for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++)
            g.DrawRectangle(&p, cx - q - gp / 2 + i * (q + gp), cy - q - gp / 2 + j * (q + gp), q, q);
    }
    else if (kind == PG_CLEAN) {
        PointF pts[6] = { {cx + s * .15f, cy - s}, {cx - s * .6f, cy + s * .1f}, {cx - s * .05f, cy + s * .1f}, {cx - s * .15f, cy + s}, {cx + s * .6f, cy - s * .15f}, {cx + s * .05f, cy - s * .15f} };
        g.DrawPolygon(&p, pts, 6);
    }
    else if (kind == PG_HIST) { g.DrawEllipse(&p, cx - s, cy - s, s * 2, s * 2); g.DrawLine(&p, cx, cy, cx, cy - s * .6f); g.DrawLine(&p, cx, cy, cx + s * .5f, cy + s * .25f); }
    else if (kind == PG_SET) {
        for (int i = 0; i < 3; i++) {
            float yy = cy - s * .7f + i * s * .7f; g.DrawLine(&p, cx - s, yy, cx + s, yy);
            float kx = cx + (i == 1 ? -s * .4f : s * .35f); g.FillEllipse(&b, kx - U(2.6f), yy - U(2.6f), U(5.2f), U(5.2f));
        }
    }
    else {
        g.DrawEllipse(&p, cx - s, cy - s, s * 2, s * 2);
        g.FillEllipse(&b, cx - U(1.5f), cy - s * .55f - U(1.5f), U(3.f), U(3.f));
        g.DrawLine(&p, cx, cy - s * .15f, cx, cy + s * .5f);
    }
}
static void PageTitle(Graphics& g, const wchar_t* title, const wchar_t* sub, float x, float y, float w, const Theme& t) {
    Text(g, title, RectF(x, y, w, U(36)), U(27), false, t.text);
    Text(g, sub, RectF(x, y + U(36), w, U(20)), U(12.5f), false, t.sub);
}
static void StatCard(Graphics& g, RectF r, const wchar_t* label, const std::wstring& val, const std::wstring& sub, Color vc, const Theme& t) {
    Card(g, r, t);
    Text(g, label, RectF(r.X + U(16), r.Y + U(9), r.Width - U(28), U(18)), U(11.5f), false, t.sub);
    Text(g, val, RectF(r.X + U(16), r.Y + U(27), r.Width - U(28), U(30)), U(21), false, vc);
    Text(g, sub, RectF(r.X + U(16), r.Y + r.Height - U(26), r.Width - U(28), U(18)), U(11.5f), false, t.sub);
}
static void ScrollBar(Graphics& g, RectF vp, int page, const Theme& t) {
    if (g_scrollMax[page] <= 0) return;
    float th = vp.Height * vp.Height / (vp.Height + g_scrollMax[page]);
    float ty = vp.Y + (vp.Height - th) * g_scroll[page] / g_scrollMax[page];
    FillRR(g, RectF(vp.GetRight() - U(5), ty, U(3), th), U(1.5f), t.border);
}

static Color PctColor(float pct, const Theme& t) { return pct < 70 ? t.accent : pct < 85 ? t.warn : t.bad; }

static void PageDashboard(Graphics& g, float x, float y, float w, float h, const Theme& t) {
    PageTitle(g, Tr(T_DASH_TITLE), Tr(T_DASH_SUB), x, y, w, t);
    float top = y + U(68), gw = U(272), gh = U(268);
    RectF gc(x, top, gw, gh); Card(g, gc, t);
    float cx = x + gw / 2, cy = top + U(122), rad = U(78);
    RectF ar(cx - rad, cy - rad, rad * 2, rad * 2);
    Pen tick(t.border, U(1.2f));
    for (int i = 0; i <= 20; i++) {
        float a = (135.f + 270.f * (float)i / 20.f) * 3.14159265f / 180.f;
        float r1 = rad + U(13), r2 = r1 + (i % 5 == 0 ? U(7) : U(4));
        g.DrawLine(&tick, cx + cosf(a) * r1, cy + sinf(a) * r1, cx + cosf(a) * r2, cy + sinf(a) * r2);
    }
    Pen bgp(t.card2, U(7)); bgp.SetStartCap(LineCapRound); bgp.SetEndCap(LineCapRound); g.DrawArc(&bgp, ar, 135, 270);
    float sweep = 270.f * std::clamp(g_gauge, 0.f, 100.f) / 100.f;
    if (sweep > 1) { Pen vp(PctColor(g_gauge, t), U(7)); vp.SetStartCap(LineCapRound); vp.SetEndCap(LineCapRound); g.DrawArc(&vp, ar, 135, sweep); }
    Text(g, F(L"%.0f%%", g_gauge), RectF(cx - rad, cy - U(34), rad * 2, U(52)), U(42), false, t.text, StringAlignmentCenter);
    Text(g, Tr(T_RAM_IN_USE), RectF(cx - rad, cy + U(20), rad * 2, U(18)), U(12), false, t.sub, StringAlignmentCenter);
    Text(g, FmtBytes((double)(g_st.total - g_st.avail)) + L" / " + FmtBytes((double)g_st.total), RectF(cx - rad - U(10), cy + U(40), rad * 2 + U(20), U(18)), U(11.5f), false, t.sub, StringAlignmentCenter);
    Btn(g, RectF(x + U(20), top + gh - U(58), gw - U(40), U(40)), g_busy ? Tr(T_CLEANING) : Tr(T_CLEAN_NOW), H_CLEAN, 0, t, true, !g_busy);

    float rx = x + gw + U(14), rw = w - gw - U(14), cw2 = (rw - U(12)) / 2, rh = (gh - U(24)) / 3;
    Record* lr = g_hist.empty() ? nullptr : &g_hist.back();
    std::wstring autoV, autoS; Color autoC = t.sub;
    if (g_set.paused) { autoV = Tr(T_PAUSED); autoC = t.warn; autoS = g_resumeAt ? F(Tr(T_RESUMES_IN), FmtDur((long long)(g_resumeAt - GetTickCount64()) / 1000).c_str()) : std::wstring(Tr(T_RESUME_MANUAL)); }
    else if (g_set.autoClean) { autoV = Tr(T_ACTIVE); autoC = t.good; long long rem = ((long long)g_nextAuto - (long long)GetTickCount64()) / 1000;
        autoS = F(Tr(T_NEXT_IN), FmtDur(rem).c_str()); }
    else { autoV = Tr(T_OFF); autoS = g_set.highUsage ? F(Tr(T_HIGH_AT), g_set.highPct) : std::wstring(Tr(T_MANUAL_ONLY)); }
    struct { const wchar_t* l; std::wstring v, s; Color c; } cards[6] = {
        { Tr(T_TOTAL_RAM), FmtBytes((double)g_st.total), Tr(T_TOTAL_RAM_SUB), t.text },
        { Tr(T_IN_USE), FmtBytes((double)(g_st.total - g_st.avail)), F(Tr(T_PCT_OF_TOTAL), (unsigned long)g_st.load), PctColor((float)g_st.load, t) },
        { Tr(T_AVAILABLE), FmtBytes((double)g_st.avail), Tr(T_AVAILABLE_SUB), t.good },
        { Tr(T_PROCESSES), F(L"%lu", (unsigned long)g_st.procs), F(Tr(T_CPU_LOAD), g_st.cpu), t.text },
        { Tr(T_LAST_CLEAN), lr ? TimeStr(lr->t, false) : std::wstring(Tr(T_NEVER)), lr ? std::wstring(TrigName(lr->trig)) : std::wstring(Tr(T_NO_CLEAN_YET)), t.text },
        { Tr(T_AUTO_CLEAN), autoV, autoS, autoC } };
    for (int i = 0; i < 6; i++) {
        RectF r(rx + (i % 2) * (cw2 + U(12)), top + (i / 2) * (rh + U(12)), cw2, rh);
        StatCard(g, r, cards[i].l, cards[i].v, cards[i].s, cards[i].c, t);
    }
    float y2 = top + gh + U(14), sh = U(100);
    RectF sc(x, y2, w, sh); Card(g, sc, t);
    Text(g, Tr(T_RAM_CHART), RectF(x + U(16), y2 + U(7), w - U(32), U(18)), U(11.5f), false, t.sub);
    float px0 = x + U(16), pw = w - U(32), py0 = y2 + U(32), ph = sh - U(44);
    Pen grid(Alpha(t.border, 160), 1.f);
    for (int i = 0; i < 3; i++) { float gy = py0 + ph * (float)i / 2.f; g.DrawLine(&grid, px0, gy, px0 + pw, gy); }
    if (g_ramHist.size() > 1) {
        std::vector<PointF> pts; size_t n = g_ramHist.size();
        for (size_t i = 0; i < n; i++) pts.push_back(PointF(px0 + pw * (float)i / 119.f + pw * (float)(120 - n) / 119.f, py0 + ph * (1.f - g_ramHist[i] / 100.f)));
        std::vector<PointF> area = pts; area.push_back(PointF(pts.back().X, py0 + ph)); area.push_back(PointF(pts.front().X, py0 + ph));
        SolidBrush ab(Alpha(t.accent, 34)); g.FillPolygon(&ab, area.data(), (INT)area.size());
        Pen lp(t.accent, U(1.6f)); lp.SetLineJoin(LineJoinRound); g.DrawLines(&lp, pts.data(), (INT)pts.size());
    }
    float y3 = y2 + sh + U(14), h3 = std::max(U(90), y + h - y3);
    RectF ac(x, y3, w, h3); Card(g, ac, t);
    Text(g, Tr(T_RECENT), RectF(x + U(16), y3 + U(8), w - U(32), U(22)), U(13), true, t.text);
    int rows = (int)((h3 - U(40)) / U(26)); int shown = 0;
    float c0 = x + U(16), cwid = w - U(32);
    for (int i = (int)g_hist.size() - 1; i >= 0 && shown < rows; i--, shown++) {
        const Record& r = g_hist[i]; float ry = y3 + U(36) + shown * U(26);
        long long rec = (long long)r.wsB - (long long)r.wsA;
        Hairline(g, x + U(12), x + w - U(12), ry - U(1), t);
        Text(g, TimeStr(r.t), RectF(c0, ry, cwid * 0.22f, U(24)), U(12), false, t.sub);
        Text(g, TrigName(r.trig), RectF(c0 + cwid * 0.23f, ry, cwid * 0.23f, U(24)), U(12), false, t.text);
        Text(g, F(Tr(T_CLEANED_SKIPPED), r.ok, r.skipped), RectF(c0 + cwid * 0.47f, ry, cwid * 0.27f, U(24)), U(12), false, t.text);
        Text(g, F(Tr(T_WS_REDUCED_SHORT), FmtBytes((double)std::max(0LL, rec)).c_str()), RectF(c0 + cwid * 0.75f, ry, cwid * 0.25f, U(24)), U(12), false, r.ok ? t.good : t.bad, StringAlignmentFar);
    }
    if (g_hist.empty()) Text(g, Tr(T_NO_CLEAN_PERFORMED), RectF(x + U(16), y3 + U(38), w - U(32), U(24)), U(12.5f), false, t.sub);
}

static void PageCleaner(Graphics& g, float x, float y, float w, float h, const Theme& t) {
    PageTitle(g, Tr(T_CLEANER_TITLE), Tr(T_CLEANER_SUB), x, y, w, t);
    float top = y + U(68), ah = U(112);
    RectF a(x, top, w, ah); Card(g, a, t);
    Text(g, Tr(T_EWS_TITLE), RectF(x + U(20), top + U(10), w - U(240), U(24)), U(15), true, t.text);
    Text(g, Tr(T_EWS_DESC), RectF(x + U(20), top + U(36), w - U(250), U(40)), U(12), false, t.sub, StringAlignmentNear, StringAlignmentNear, true);
    Text(g, g_admin ? Tr(T_RUN_ADMIN) : Tr(T_RUN_STD), RectF(x + U(20), top + U(82), w - U(250), U(22)), U(12), true, g_admin ? t.good : t.warn);
    Btn(g, RectF(x + w - U(200), top + U(16), U(180), U(42)), g_busy ? Tr(T_CLEANING) : Tr(T_CLEAN_NOW), H_CLEAN, 0, t, true, !g_busy);
    if (!g_admin) Btn(g, RectF(x + w - U(200), top + U(68), U(180), U(32)), Tr(T_RESTART_ADMIN), H_ADMIN, 0, t, false);

    float by = top + ah + U(14), foot = U(64), bh = std::max(U(200), y + h - by - foot - U(8));
    RectF b(x, by, w, bh); Card(g, b, t);
    if (!g_hasLast) { Text(g, Tr(T_NO_CLEAN_SESSION), RectF(x + U(20), by + U(14), w - U(40), U(24)), U(13), false, t.sub); }
    else {
        const Record& r = g_last.rec; float cw = (w - U(40)) / 5;
        const wchar_t* lab[5] = { Tr(T_L_CLEANED), Tr(T_L_SKIPPED), Tr(T_L_WS), Tr(T_L_SYSRAM), Tr(T_L_DURATION) };
        std::wstring val[5] = { F(L"%d", r.ok), F(L"%d", r.skipped), FmtBytes((double)r.wsB) + L" > " + FmtBytes((double)r.wsA),
                                FmtBytes((double)r.usedB) + L" > " + FmtBytes((double)r.usedA), F(Tr(T_MS_FMT), (unsigned long)r.ms) };
        for (int i = 0; i < 5; i++) {
            RectF cr(x + U(20) + i * cw, by + U(12), cw - U(6), U(50));
            Text(g, lab[i], RectF(cr.X, cr.Y, cr.Width, U(18)), U(11.5f), false, t.sub);
            Text(g, val[i], RectF(cr.X, cr.Y + U(18), cr.Width + U(6), U(28)), (i == 2 || i == 3) ? U(13) : U(19), false, i == 1 && r.skipped ? t.warn : t.text);
        }
        Hairline(g, x + U(14), x + w - U(14), by + U(68), t);
        float ly = by + U(76), lh = bh - U(84), half = (w - U(52)) / 2;
        Text(g, Tr(T_L_TOP), RectF(x + U(20), ly, half, U(20)), U(12.5f), true, t.text);
        for (size_t i = 0; i < g_last.top.size() && U(24) + i * U(22) + U(22) < lh; i++) {
            const Reduced& rd = g_last.top[i]; float ry = ly + U(24) + i * U(22);
            Text(g, rd.name, RectF(x + U(20), ry, half * 0.6f, U(20)), U(12), false, t.text);
            Text(g, L"-" + FmtBytes((double)(rd.before > rd.after ? rd.before - rd.after : 0)), RectF(x + U(20) + half * 0.6f, ry, half * 0.4f, U(20)), U(12), false, t.good, StringAlignmentFar);
        }
        float sx = x + U(32) + half; RectF vp(sx, ly + U(24), half, lh - U(24));
        Text(g, F(Tr(T_L_SKIPPED_LIST), r.skipped), RectF(sx, ly, half, U(20)), U(12.5f), true, t.text);
        float total = (float)g_last.skips.size() * U(22); g_scrollMax[PG_CLEAN] = std::max(0.f, total - vp.Height);
        g_scroll[PG_CLEAN] = std::clamp(g_scroll[PG_CLEAN], 0.f, g_scrollMax[PG_CLEAN]);
        g.SetClip(vp);
        for (size_t i = 0; i < g_last.skips.size(); i++) {
            float ry = vp.Y + i * U(22) - g_scroll[PG_CLEAN]; if (ry + U(22) < vp.Y || ry > vp.GetBottom()) continue;
            const SkipItem& s = g_last.skips[i];
            Text(g, s.name, RectF(sx, ry, half * 0.38f, U(20)), U(12), false, t.text);
            Text(g, ErrText(s.err), RectF(sx + half * 0.4f, ry, half * 0.6f, U(20)), U(11), false, t.sub);
        }
        g.ResetClip();
    }
    Text(g, Tr(T_CLEAN_NOTE), RectF(x, by + bh + U(6), w, foot), U(11.5f), false, t.sub, StringAlignmentNear, StringAlignmentNear, true);
}

static void PageHistory(Graphics& g, float x, float y, float w, float h, const Theme& t) {
    PageTitle(g, Tr(T_HIST_TITLE), Tr(T_HIST_SUB), x, y, w, t);
    Btn(g, RectF(x + w - U(140), y + U(8), U(140), U(32)), Tr(T_CLEAR_HIST), H_CLRHIST, 0, t, false);
    Btn(g, RectF(x + w - U(320), y + U(8), U(170), U(32)), Tr(T_OPEN_FOLDER), H_OPENDIR, 0, t, false);
    float top = y + U(68), foot = U(54), th = std::max(U(150), h - U(68) - foot - U(8)); RectF c(x, top, w, th); Card(g, c, t);
    int okc = 0; for (auto& r : g_hist) if (r.ok > 0) okc++;
    float fr[7] = {0.17f, 0.15f, 0.09f, 0.09f, 0.22f, 0.18f, 0.10f};
    const wchar_t* hd[7] = {Tr(T_H_TIME), Tr(T_H_TYPE), Tr(T_H_CLEANED), Tr(T_H_SKIPPED), Tr(T_H_SYSRAM), Tr(T_H_WSFREED), Tr(T_H_STATUS)};
    float cx = x + U(16), tw = w - U(32), hx = cx;
    for (int i = 0; i < 7; i++) { Text(g, hd[i], RectF(hx, top + U(8), tw * fr[i], U(22)), U(11.5f), true, t.sub); hx += tw * fr[i]; }
    Hairline(g, x + U(12), x + w - U(12), top + U(36), t);
    RectF vp(x, top + U(40), w, th - U(44)); float total = g_hist.size() * U(28);
    g_scrollMax[PG_HIST] = std::max(0.f, total - vp.Height); g_scroll[PG_HIST] = std::clamp(g_scroll[PG_HIST], 0.f, g_scrollMax[PG_HIST]);
    g.SetClip(vp);
    for (int i = 0; i < (int)g_hist.size(); i++) {
        const Record& r = g_hist[g_hist.size() - 1 - i]; float ry = vp.Y + i * U(28) - g_scroll[PG_HIST]; if (ry + U(28) < vp.Y || ry > vp.GetBottom()) continue;
        long long rec = (long long)r.wsB - (long long)r.wsA;
        std::wstring cells[7] = { TimeStr(r.t), TrigName(r.trig), F(L"%d", r.ok), F(L"%d", r.skipped), FmtBytes((double)r.usedB) + L" > " + FmtBytes((double)r.usedA),
                                  L"-" + FmtBytes((double)std::max(0LL, rec)), r.ok > 0 ? Tr(T_COMPLETED) : Tr(T_FAILED) };
        float hx2 = cx; for (int k = 0; k < 7; k++) { Text(g, cells[k], RectF(hx2, ry, tw * fr[k] - U(4), U(26)), U(12), false, k == 6 ? (r.ok > 0 ? t.good : t.bad) : t.text); hx2 += tw * fr[k]; }
    }
    g.ResetClip();
    if (g_hist.empty()) Text(g, Tr(T_NO_HISTORY), RectF(x + U(16), top + U(48), w - U(32), U(24)), U(12.5f), false, t.sub);
    Text(g, F(Tr(T_HIST_FOOT), (int)g_hist.size(), okc), RectF(x, top + th + U(6), w, foot), U(11.5f), false, t.sub, StringAlignmentNear, StringAlignmentNear, true);
}

enum { RK_HEADER, RK_TOGGLE, RK_INTERVAL, RK_PCT, RK_RESUME, RK_LANG, RK_RESET };
struct Row { int kind; int label; int desc; int key; };
static const Row SROWS[] = {
    {RK_HEADER,   T_SEC_AUTO,    0,            0},
    {RK_TOGGLE,   T_S_AUTO,      T_S_AUTO_D,   K_AUTO},
    {RK_INTERVAL, T_S_INT,       T_S_INT_D,    0},
    {RK_TOGGLE,   T_S_HIGH,      T_S_HIGH_D,   K_HIGH},
    {RK_PCT,      T_S_PCT,       T_S_PCT_D,    0},
    {RK_TOGGLE,   T_S_DEFER,     T_S_DEFER_D,  K_DEFER},
    {RK_TOGGLE,   T_S_PAUSE,     T_S_PAUSE_D,  K_PAUSE},
    {RK_RESUME,   T_S_RESUME,    T_S_RESUME_D, 0},
    {RK_HEADER,   T_SEC_WIN,     0,            0},
    {RK_TOGGLE,   T_S_STARTUP,   T_S_STARTUP_D, K_STARTUP},
    {RK_TOGGLE,   T_S_STARTMIN,  T_S_STARTMIN_D, K_STARTMIN},
    {RK_TOGGLE,   T_S_TRAY,      T_S_TRAY_D,   K_TRAY},
    {RK_HEADER,   T_SEC_GEN,     0,            0},
    {RK_LANG,     T_S_LANG,      T_S_LANG_D,   0},
    {RK_TOGGLE,   T_S_LOG,       T_S_LOG_D,    K_LOG},
    {RK_TOGGLE,   T_S_NOTIFY,    T_S_NOTIFY_D, K_NOTIFY},
    {RK_TOGGLE,   T_S_CONFIRM,   T_S_CONFIRM_D, K_CONFIRM},
    {RK_TOGGLE,   T_S_DARK,      T_S_DARK_D,   K_DARK},
    {RK_RESET,    T_S_RESET,     T_S_RESET_D,  0},
};
static void PageSettings(Graphics& g, float x, float y, float w, float h, const Theme& t) {
    PageTitle(g, Tr(T_SET_TITLE), Tr(T_SET_SUB), x, y, w, t);
    RectF vp(x, y + U(68), w, h - U(68)); const int N = (int)(sizeof SROWS / sizeof SROWS[0]);
    const float ROWH = U(68), HDR = U(38), BAN = U(56), GAP = U(8);
    bool warnShort = IntervalSec() < 60, quiet = NotifyMuted();
    float total = U(8);
    for (int i = 0; i < N; i++) {
        if (SROWS[i].kind == RK_HEADER) total += HDR + GAP; else total += ROWH;
        if (SROWS[i].kind == RK_INTERVAL && warnShort) total += BAN;
    }
    g_scrollMax[PG_SET] = std::max(0.f, total - vp.Height); g_scroll[PG_SET] = std::clamp(g_scroll[PG_SET], 0.f, g_scrollMax[PG_SET]);
    g.SetClip(vp); g_clip = vp; float cy = vp.Y - g_scroll[PG_SET]; float rw = w - U(12);
    int i = 0;
    while (i < N) {
        const Row& hr = SROWS[i];
        Text(g, Tr(hr.label), RectF(x + U(4), cy + U(10), rw, U(22)), U(12), true, t.accent);
        cy += HDR; i++;
        int j = i; float ph = 0;
        while (j < N && SROWS[j].kind != RK_HEADER) { ph += ROWH; if (SROWS[j].kind == RK_INTERVAL && warnShort) ph += BAN; j++; }
        Card(g, RectF(x, cy, rw, ph), t);
        float ry = cy;
        for (int k = i; k < j; k++) {
            const Row& r = SROWS[k];
            float mid = ry + ROWH / 2, rx = x + rw - U(18), tw = rw - U(310);
            bool noteRow = r.kind == RK_TOGGLE && r.key == K_NOTIFY;
            bool muted = noteRow && quiet;
            Text(g, Tr(r.label), RectF(x + U(18), ry + U(10), tw, U(22)), U(14), false, t.text);
            Text(g, muted ? Tr(T_NOTIFY_QUIET) : Tr(r.desc), RectF(x + U(18), ry + U(33), tw, U(30)), U(11.5f), false, muted ? t.warn : t.sub, StringAlignmentNear, StringAlignmentNear, true);
            if (r.kind == RK_TOGGLE) Toggle(g, rx, mid, *BoolRef(r.key), r.key, t);
            else if (r.kind == RK_INTERVAL) {
                const wchar_t* un[3] = {Tr(T_UNIT_SEC), Tr(T_UNIT_MIN), Tr(T_UNIT_HOUR)};
                RectF ub(rx - U(96), mid - U(15), U(96), U(30)); Btn(g, ub, un[g_set.unit], H_STEP, ST_UNIT * 2, t, false);
                Stepper(g, rx - U(108), mid, F(L"%d", g_set.ival), ST_INT, U(52), t);
            }
            else if (r.kind == RK_PCT) Stepper(g, rx, mid, F(L"%d%%", g_set.highPct), ST_PCT, U(60), t);
            else if (r.kind == RK_RESUME) Stepper(g, rx, mid, g_set.resumeMin ? F(Tr(T_MIN_FMT), g_set.resumeMin) : std::wstring(Tr(T_OFF)), ST_RESUME, U(76), t);
            else if (r.kind == RK_LANG) {
                Btn(g, RectF(rx - U(100), mid - U(16), U(100), U(32)), Tr(T_BTN_CHANGE), H_LANGOPEN, 0, t, false);
                Text(g, LANG_NAME[g_set.lang], RectF(rx - U(112) - U(170), mid - U(12), U(170), U(24)), U(13), true, t.text, StringAlignmentFar, StringAlignmentCenter, false, FamFor(g_set.lang));
            }
            else if (r.kind == RK_RESET) Btn(g, RectF(rx - U(110), mid - U(16), U(110), U(32)), Tr(T_BTN_RESET), H_RESET, 0, t, false);
            ry += ROWH;
            if (r.kind == RK_INTERVAL && warnShort) {
                RectF wr(x + U(14), ry + U(2), rw - U(28), BAN - U(10));
                FillRR(g, wr, U(7), Alpha(t.warn, 30)); StrokeRR(g, wr, U(7), Alpha(t.warn, 150));
                Text(g, Tr(T_INTERVAL_WARN), RectF(wr.X + U(12), wr.Y + U(2), wr.Width - U(24), wr.Height - U(4)), U(11.5f), false, t.text, StringAlignmentNear, StringAlignmentCenter, true);
                ry += BAN;
            }
            if (k < j - 1) Hairline(g, x + U(16), x + rw - U(16), ry, t);
        }
        cy += ph + GAP; i = j;
    }
    g_clip = RectF(0, 0, 0, 0); g.ResetClip();
    ScrollBar(g, vp, PG_SET, t);
}

static void PageAbout(Graphics& g, float x, float y, float w, float h, const Theme& t) {
    PageTitle(g, Tr(T_ABOUT_TITLE), Tr(T_ABOUT_SUB), x, y, w, t);
    RectF vp(x, y + U(68), w, h - U(68));
    g_scroll[PG_ABOUT] = std::clamp(g_scroll[PG_ABOUT], 0.f, g_scrollMax[PG_ABOUT]);
    g.SetClip(vp); g_clip = vp;
    float rw = w - U(12), cy = vp.Y - g_scroll[PG_ABOUT], start = cy, pad = U(22);

    float dh = TextH(g, Tr(T_ABOUT_DESC), rw - pad * 2, U(13)) + U(6);
    float c1 = U(84) + dh + pad;
    Card(g, RectF(x, cy, rw, c1), t);
    LogoMark(g, x + pad, cy + pad, U(46), t);
    Text(g, APP_NAME, RectF(x + pad + U(60), cy + pad - U(2), rw - pad * 2 - U(60), U(28)), U(22), true, t.text);
    Text(g, std::wstring(Tr(T_ABOUT_VER)) + L"  -  " + Tr(T_APP_TAGLINE), RectF(x + pad + U(60), cy + pad + U(26), rw - pad * 2 - U(60), U(20)), U(12), false, t.sub);
    Text(g, Tr(T_ABOUT_DESC), RectF(x + pad, cy + U(84), rw - pad * 2, dh), U(13), false, t.text, StringAlignmentNear, StringAlignmentNear, true);
    cy += c1 + U(14);

    float c2 = U(104);
    Card(g, RectF(x, cy, rw, c2), t);
    Text(g, Tr(T_CREATOR), RectF(x + pad, cy + U(18), rw / 2 - pad, U(18)), U(11.5f), false, t.sub);
    Text(g, L"FidzZz Try", RectF(x + pad, cy + U(38), rw / 2 - pad, U(34)), U(24), false, t.text);
    float bw = std::min(U(270), rw / 2), bx = x + rw - pad - bw;
    RectF gb(bx, cy + U(22), bw, U(40));
    bool ghv = Hover(H_GITHUB, 0);
    if (ghv) FillRR(g, gb, U(7), t.card2);
    StrokeRR(g, gb, U(7), ghv ? t.accent : t.border);
    Text(g, Tr(T_GITHUB_BTN), gb, U(13), true, t.accent, StringAlignmentCenter);
    AddHit(gb, H_GITHUB, 0);
    Text(g, Tr(T_GITHUB_HINT), RectF(bx, cy + U(66), bw, U(20)), U(11.5f), false, t.sub, StringAlignmentCenter);
    cy += c2 + U(14);

    float qw = U(248), qh = U(330), colw = rw - pad * 2 - qw - U(26);
    float dd = TextH(g, Tr(T_DONATE_DESC), colw, U(13)) + U(6);
    float leftH = pad + U(30) + dd + U(10) + U(22) + pad;
    float rightH = pad + qh + U(8) + U(36) + pad;
    float c3 = std::max(leftH, rightH);
    Card(g, RectF(x, cy, rw, c3), t);
    Text(g, Tr(T_DONATE_TITLE), RectF(x + pad, cy + pad - U(2), colw, U(28)), U(17), true, t.text);
    Text(g, Tr(T_DONATE_DESC), RectF(x + pad, cy + pad + U(30), colw, dd), U(13), false, t.text, StringAlignmentNear, StringAlignmentNear, true);
    Text(g, Tr(T_THANKS), RectF(x + pad, cy + pad + U(30) + dd + U(10), colw, U(22)), U(12), false, t.sub);

    RectF qb(x + rw - pad - qw, cy + pad, qw, qh);
    FillRR(g, qb, U(8), g_qrisState == 2 ? Color(255, 255, 255, 255) : t.card2);
    StrokeRR(g, qb, U(8), t.border);
    if (g_qrisState == 2 && g_qris) {
        float iw = (float)g_qris->GetWidth(), ih = (float)g_qris->GetHeight();
        float sc = std::min((qb.Width - U(16)) / iw, (qb.Height - U(16)) / ih);
        float dw = iw * sc, dhh = ih * sc;
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.DrawImage(g_qris, RectF(qb.X + (qb.Width - dw) / 2, qb.Y + (qb.Height - dhh) / 2, dw, dhh));
    } else if (g_qrisState == 3) {
        Text(g, Tr(T_QRIS_FAILED), RectF(qb.X + U(14), qb.Y + U(20), qb.Width - U(28), U(90)), U(12), false, t.text, StringAlignmentCenter, StringAlignmentNear, true);
        Btn(g, RectF(qb.X + U(24), qb.Y + U(130), qb.Width - U(48), U(36)), Tr(T_QRIS_RETRY), H_QRRETRY, 0, t, true);
        Btn(g, RectF(qb.X + U(24), qb.Y + U(176), qb.Width - U(48), U(36)), Tr(T_QRIS_OPEN), H_QROPEN, 0, t, false);
    } else {
        Text(g, Tr(T_QRIS_LOADING), qb, U(12), false, t.sub, StringAlignmentCenter);
    }
    Text(g, Tr(T_QRIS_LABEL), RectF(qb.X, qb.Y + qh + U(8), qb.Width, U(36)), U(11.5f), false, t.sub, StringAlignmentCenter, StringAlignmentNear, true);
    cy += c3 + U(10);

    float total = cy - start;
    g_scrollMax[PG_ABOUT] = std::max(0.f, total - vp.Height);
    g_clip = RectF(0, 0, 0, 0); g.ResetClip();
    ScrollBar(g, vp, PG_ABOUT, t);
}

static void PageLangPicker(Graphics& g, float W, float H, const Theme& t) {
    float bw = std::min(U(620), W - U(48)), x0 = (W - bw) / 2;
    float colw = (bw - U(14)) / 2, bh = U(48), gapY = U(10);
    float total = U(150) + 5 * (bh + gapY) + U(64);
    float y0 = std::max(U(20), (H - total) / 2);
    LogoMark(g, x0, y0, U(42), t);
    Text(g, Tr(T_LANGSEL_TITLE), RectF(x0, y0 + U(58), bw, U(40)), U(28), false, t.text);
    Text(g, Tr(T_LANGSEL_SUB), RectF(x0, y0 + U(98), bw, U(22)), U(13), false, t.sub);
    float gy = y0 + U(140);
    for (int i = 0; i < LANG_COUNT; i++) {
        RectF r(x0 + (i % 2) * (colw + U(14)), gy + (i / 2) * (bh + gapY), colw, bh);
        bool sel = (i == g_set.lang), hv = Hover(H_LANGPICK, i);
        FillRR(g, r, U(9), sel ? Alpha(t.accent, 26) : hv ? t.card2 : t.card);
        StrokeRR(g, r, U(9), sel ? t.accent : t.border, sel ? U(1.5f) : 1.f);
        float dcx = r.X + U(22), dcy = r.Y + r.Height / 2;
        Pen rp(sel ? t.accent : t.sub, U(1.5f)); g.DrawEllipse(&rp, dcx - U(7), dcy - U(7), U(14), U(14));
        if (sel) { SolidBrush ab(t.accent); g.FillEllipse(&ab, dcx - U(3.5f), dcy - U(3.5f), U(7), U(7)); }
        Text(g, LANG_NAME[i], RectF(r.X + U(42), r.Y, r.Width - U(52), r.Height), U(15), sel, t.text, StringAlignmentNear, StringAlignmentCenter, false, FamFor(i));
        AddHit(r, H_LANGPICK, i);
    }
    float by = gy + 5 * (bh + gapY) + U(8);
    Btn(g, RectF(x0 + bw - U(190), by, U(190), U(44)), Tr(T_LANGSEL_CONT), H_LANGOK, 0, t, true);
}

static void Render(Graphics& g, float W, float H) {
    Theme t = TH(); g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g.Clear(t.bg); g_hits.clear();
    if (g_showPicker) { PageLangPicker(g, W, H, t); return; }
    bool compact = W < U(900); float sbw = compact ? U(70) : U(216);
    SolidBrush sb(t.side); g.FillRectangle(&sb, 0.f, 0.f, sbw, H); Pen bp(t.border, 1.f); g.DrawLine(&bp, sbw, 0.f, sbw, H);
    LogoMark(g, compact ? (sbw - U(34)) / 2 : U(22), U(24), U(34), t);
    if (!compact) { Text(g, APP_NAME, RectF(U(66), U(22), U(140), U(24)), U(18), true, t.text); Text(g, Tr(T_APP_TAGLINE), RectF(U(66), U(44), U(140), U(18)), U(11.5f), false, t.sub); }
    const int navText[PG_COUNT] = { T_NAV_DASH, T_NAV_CLEAN, T_NAV_HIST, T_NAV_SET, T_NAV_ABOUT };
    for (int i = 0; i < PG_COUNT; i++) {
        RectF r(U(10), U(96) + i * U(44), sbw - U(20), U(38)); bool sel = g_page == i, hv = Hover(H_NAV, i);
        if (sel) { FillRR(g, r, U(8), t.card); StrokeRR(g, r, U(8), t.border); FillRR(g, RectF(r.X, r.Y + U(10), U(3), r.Height - U(20)), U(1.5f), t.accent); }
        else if (hv) FillRR(g, r, U(8), t.card);
        Color ic = sel ? t.accent : t.sub;
        NavIcon(g, i, RectF(r.X + U(8), r.Y, U(32), r.Height), ic);
        if (!compact) Text(g, Tr(navText[i]), RectF(r.X + U(48), r.Y, r.Width - U(52), r.Height), U(13.5f), sel, sel ? t.text : t.sub);
        AddHit(r, H_NAV, i);
    }
    if (!compact) {
        Text(g, g_admin ? Tr(T_ADMIN) : Tr(T_STD_USER), RectF(U(22), H - U(56), sbw - U(30), U(20)), U(11.5f), true, g_admin ? t.good : t.warn);
        Text(g, std::wstring(L"v1.1  -  ") + Tr(T_TRAY_NOTE), RectF(U(22), H - U(36), sbw - U(30), U(18)), U(11), false, t.sub);
    }
    float cx = sbw + U(30), cy = U(24), cw = W - cx - U(28), ch = H - cy - U(14);
    switch (g_page) {
        case PG_DASH: PageDashboard(g, cx, cy, cw, ch, t); break;
        case PG_CLEAN: PageCleaner(g, cx, cy, cw, ch, t); break;
        case PG_HIST: PageHistory(g, cx, cy, cw, ch, t); break;
        case PG_SET: PageSettings(g, cx, cy, cw, ch, t); break;
        default: PageAbout(g, cx, cy, cw, ch, t); break;
    }
}

static UINT DpiFor(HWND h) {
    typedef UINT (WINAPI* PFN)(HWND); static PFN f = (PFN)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    return (f && h) ? f(h) : 96;
}
static void PaintWindow(HWND hwnd) {
    PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps); RECT rc; GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    if (W > 0 && H > 0) {
        HDC mdc = CreateCompatibleDC(hdc); HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H); HGDIOBJ old = SelectObject(mdc, bmp);
        { Graphics g(mdc); Render(g, (float)W, (float)H); }
        BitBlt(hdc, 0, 0, W, H, mdc, 0, 0, SRCCOPY); SelectObject(mdc, old); DeleteObject(bmp); DeleteDC(mdc);
    }
    EndPaint(hwnd, &ps);
}
static void OnClick(float x, float y) {
    const Hit* h = HitAt(x, y); if (!h) return; int id = h->id, arg = h->arg;
    switch (id) {
        case H_NAV: g_page = arg; if (arg == PG_ABOUT) EnsureQris(); break;
        case H_CLEAN: RequestClean(TRIG_MANUAL); break;
        case H_TOGGLE: ToggleSetting(arg); break;
        case H_STEP: StepSetting(arg / 2, (arg & 1) ? 1 : -1); break;
        case H_RESET: ResetDefaults(); break;
        case H_ADMIN: RestartAsAdmin(); return;
        case H_OPENDIR: ShellExecuteW(g_hwnd, L"open", g_dir.c_str(), 0, 0, SW_SHOWNORMAL); break;
        case H_CLRHIST:
            if (MessageBoxW(g_hwnd, Tr(T_MSG_CLEAR_HIST), APP_NAME, MB_YESNO | MB_ICONQUESTION) == IDYES) { g_hist.clear(); DeleteFileW(g_histFile.c_str()); }
            break;
        case H_LANGPICK: g_set.lang = arg; ApplyLanguage(); SaveSettings(); break;
        case H_LANGOPEN: g_showPicker = true; break;
        case H_LANGOK: g_set.langChosen = true; g_showPicker = false; SaveSettings(); break;
        case H_GITHUB: ShellExecuteW(g_hwnd, L"open", URL_GITHUB, 0, 0, SW_SHOWNORMAL); break;
        case H_QRRETRY: RetryQris(); break;
        case H_QROPEN: ShellExecuteW(g_hwnd, L"open", URL_QRIS, 0, 0, SW_SHOWNORMAL); break;
    }
    InvalidateRect(g_hwnd, 0, FALSE);
}
static void ShowTrayMenu() {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, ID_TRAY_OPEN, Tr(T_MENU_OPEN));
    AppendMenuW(m, MF_STRING | (g_busy ? MF_GRAYED : 0), ID_TRAY_CLEAN, Tr(T_CLEAN_NOW));
    AppendMenuW(m, MF_STRING, ID_TRAY_PAUSE, g_set.paused ? Tr(T_MENU_RESUME) : Tr(T_MENU_PAUSE));
    AppendMenuW(m, MF_SEPARATOR, 0, 0); AppendMenuW(m, MF_STRING, ID_TRAY_EXIT, Tr(T_MENU_EXIT));
    POINT pt; GetCursorPos(&pt); SetForegroundWindow(g_hwnd);
    TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, g_hwnd, 0); PostMessageW(g_hwnd, WM_NULL, 0, 0); DestroyMenu(m);
}
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == g_msgTaskbar && msg != 0) { TrayAdd(); return 0; }
    switch (msg) {
        case WM_TIMER:
            if (wp == TIMER_SEC) {
                OnSecondTick();
                if (IsWindowVisible(hwnd) && !IsIconic(hwnd)) { InvalidateRect(hwnd, 0, FALSE); if (fabsf(g_gauge - g_gaugeTarget) > 0.2f) SetTimer(hwnd, TIMER_ANIM, 16, 0); }
            } else if (wp == TIMER_ANIM) {
                g_gauge += (g_gaugeTarget - g_gauge) * 0.22f;
                if (fabsf(g_gauge - g_gaugeTarget) <= 0.2f) { g_gauge = g_gaugeTarget; KillTimer(hwnd, TIMER_ANIM); }
                InvalidateRect(hwnd, 0, FALSE);
            }
            return 0;
        case WM_CLEAN_DONE: {
            CleanDetail* d = (CleanDetail*)lp; g_last = *d; g_hasLast = true; g_hist.push_back(d->rec);
            if (g_hist.size() > 1000) g_hist.erase(g_hist.begin());
            if (g_set.logging) AppendHistory(d->rec);
            bool automatic = d->rec.trig != TRIG_MANUAL;
            bool allowed = automatic ? !NotifyMuted() : !IsWindowVisible(hwnd);
            if (g_set.notify && allowed) {
                long long rec = std::max(0LL, (long long)d->rec.wsB - (long long)d->rec.wsA);
                std::wstring s = F(Tr(T_NOTIF_BODY), d->rec.ok, d->rec.skipped, FmtBytes((double)rec).c_str());
                Notify(Tr(T_NOTIF_TITLE), s.c_str());
            }
            delete d; InterlockedExchange(&g_busy, 0); RefreshStats(); InvalidateRect(hwnd, 0, FALSE); SetTimer(hwnd, TIMER_ANIM, 16, 0); return 0;
        }
        case WM_QRIS_DONE:
            g_qrisState = (wp && LoadQrisFile()) ? 2 : 3;
            if (g_qrisState == 3) DeleteFileW(g_qrisPath.c_str());
            InvalidateRect(hwnd, 0, FALSE); return 0;
        case WM_PAINT: PaintWindow(hwnd); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_LBUTTONDOWN: OnClick((float)GET_X_LPARAM(lp), (float)GET_Y_LPARAM(lp)); return 0;
        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tme = { sizeof tme, TME_LEAVE, hwnd, 0 }; TrackMouseEvent(&tme);
            const Hit* h = HitAt((float)GET_X_LPARAM(lp), (float)GET_Y_LPARAM(lp)); int id = h ? h->id : 0, arg = h ? h->arg : -1;
            if (id != g_hoverId || arg != g_hoverArg) { g_hoverId = id; g_hoverArg = arg; InvalidateRect(hwnd, 0, FALSE); }
            return 0;
        }
        case WM_MOUSELEAVE: g_hoverId = 0; g_hoverArg = -1; InvalidateRect(hwnd, 0, FALSE); return 0;
        case WM_SETCURSOR: if (LOWORD(lp) == HTCLIENT && g_hoverId) { SetCursor(LoadCursorW(0, IDC_HAND)); return TRUE; } break;
        case WM_MOUSEWHEEL: { g_scroll[g_page] = std::max(0.f, g_scroll[g_page] - GET_WHEEL_DELTA_WPARAM(wp) / 120.f * U(48)); InvalidateRect(hwnd, 0, FALSE); return 0; }
        case WM_GETMINMAXINFO: {
            RECT rc = {0, 0, (LONG)U(860), (LONG)U(660)}; AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);
            MINMAXINFO* mm = (MINMAXINFO*)lp; mm->ptMinTrackSize.x = rc.right - rc.left; mm->ptMinTrackSize.y = rc.bottom - rc.top; return 0;
        }
        case WM_DPICHANGED: {
            g_scale = HIWORD(wp) / 96.0f; RECT* r = (RECT*)lp;
            SetWindowPos(hwnd, 0, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE); return 0;
        }
        case WM_SIZE: InvalidateRect(hwnd, 0, FALSE); return 0;
        case WM_SYSCOMMAND:
            if ((wp & 0xFFF0) == SC_MINIMIZE && g_set.toTray) { ShowWindow(hwnd, SW_HIDE); return 0; }
            break;
        case WM_CLOSE:
            if (g_set.toTray && !g_exiting) { ShowWindow(hwnd, SW_HIDE); return 0; }
            DestroyWindow(hwnd); return 0;
        case WM_TRAY:
            if (LOWORD(lp) == WM_LBUTTONDBLCLK || LOWORD(lp) == WM_LBUTTONUP) ShowMain();
            else if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU) ShowTrayMenu();
            return 0;
        case WM_SHOWMAIN: ShowMain(); return 0;
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case ID_TRAY_OPEN: ShowMain(); break;
                case ID_TRAY_CLEAN: StartClean(TRIG_MANUAL); break;
                case ID_TRAY_PAUSE: SetPaused(!g_set.paused); InvalidateRect(hwnd, 0, FALSE); break;
                case ID_TRAY_EXIT: g_exiting = true; DestroyWindow(hwnd); break;
            } return 0;
        case WM_DESTROY:
            Shell_NotifyIconW(NIM_DELETE, &g_nid); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int SelfTest() {
    InitPaths(); EnablePriv(L"SeDebugPrivilege"); EnablePriv(L"SeIncreaseQuotaPrivilege");
    CleanDetail* d = DoClean(TRIG_MANUAL);
    std::wstring p = g_dir + L"\\selftest.txt"; FILE* f = _wfopen(p.c_str(), L"w");
    if (f) {
        fprintf(f, "admin=%d\ncleaned=%d\nskipped=%d\nws_before=%llu\nws_after=%llu\nram_used_before=%llu\nram_used_after=%llu\nms=%lu\n", CheckElevated() ? 1 : 0, d->rec.ok, d->rec.skipped,
                (unsigned long long)d->rec.wsB, (unsigned long long)d->rec.wsA, (unsigned long long)d->rec.usedB, (unsigned long long)d->rec.usedA, (unsigned long)d->rec.ms);
        for (auto& s : d->skips) fprintf(f, "skip pid=%lu %ls: error %lu\n", (unsigned long)s.pid, s.name.c_str(), (unsigned long)s.err);
        fclose(f);
    }
    delete d; return 0;
}
int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, LPWSTR cmd, int) {
    std::wstring cl = cmd ? cmd : L"";
    if (cl.find(L"--selftest") != std::wstring::npos) return SelfTest();
    bool fromStartup = cl.find(L"--startup") != std::wstring::npos, forceMin = cl.find(L"--minimized") != std::wstring::npos;
    g_mutex = CreateMutexW(0, TRUE, L"Local\\RamPulse.SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) { HWND o = FindWindowW(APP_CLASS, 0); if (o) PostMessageW(o, WM_SHOWMAIN, 0, 0); return 0; }
    GdiplusStartupInput gsi; ULONG_PTR gtok; GdiplusStartup(&gtok, &gsi, 0);
    InitPaths(); LoadSettings(); LoadHistory();
    g_showPicker = !g_set.langChosen;
    EnablePriv(L"SeDebugPrivilege"); EnablePriv(L"SeIncreaseQuotaPrivilege"); g_admin = CheckElevated();
    g_icon = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    g_iconSmall = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    g_msgTaskbar = RegisterWindowMessageW(L"TaskbarCreated");
    HDC sdc = GetDC(0); g_scale = GetDeviceCaps(sdc, LOGPIXELSX) / 96.0f; ReleaseDC(0, sdc);
    WNDCLASSEXW wc = {sizeof wc}; wc.lpfnWndProc = WndProc; wc.hInstance = hi; wc.lpszClassName = APP_CLASS; wc.hCursor = LoadCursorW(0, IDC_ARROW);
    wc.hIcon = g_icon; wc.hIconSm = g_iconSmall; RegisterClassExW(&wc);
    RECT rc = {0, 0, (LONG)U(1000), (LONG)U(720)}; AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowExW(0, APP_CLASS, Tr(T_WIN_TITLE), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             rc.right - rc.left, rc.bottom - rc.top, 0, 0, hi, 0);
    g_scale = DpiFor(g_hwnd) / 96.0f; ApplyTheme();
    RefreshStats(); g_gauge = g_gaugeTarget; ResetSchedule();
    if (g_set.paused && g_set.resumeMin > 0) g_resumeAt = GetTickCount64() + (ULONGLONG)g_set.resumeMin * 60000ULL;
    TrayAdd(); SetTimer(g_hwnd, TIMER_SEC, 1000, 0);
    if ((fromStartup && g_set.startMin && g_set.langChosen) || forceMin) ShowWindow(g_hwnd, g_set.toTray ? SW_HIDE : SW_SHOWMINNOACTIVE);
    else ShowWindow(g_hwnd, SW_SHOW);
    MSG m; while (GetMessageW(&m, 0, 0, 0) > 0) { TranslateMessage(&m); DispatchMessageW(&m); }
    if (g_qris) { delete g_qris; g_qris = nullptr; }
    GdiplusShutdown(gtok); return 0;
}
