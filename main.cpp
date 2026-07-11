// main.cpp — MCPE UWP GUI DLL Injector  v3  (by anx1ous)

#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#  define UNICODE
#endif
#ifndef _UNICODE
#  define _UNICODE
#endif
#include <windows.h>
#include <commdlg.h>
#include <tlhelp32.h>
#include <aclapi.h>
#include <sddl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#  define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

// ─── Control IDs ─────────────────────────────────────────────────────────────
#define ID_BTN_SELECT  101
#define ID_BTN_INJECT  102
#define ID_EDIT_LOG    103
#define ID_EDIT_PATH   104
#define ID_BTN_THEME   105

#define WM_APP_SETSTATUS (WM_APP + 1)

// ─── Separator Y positions (drawn manually in WM_PAINT) ──────────────────────
static const int SEP_Y[] = { 40, 122, 156 };

// ─── Theme palette ────────────────────────────────────────────────────────────
struct Palette {
    COLORREF bg, editBg, text, dim,
             statusOk, statusWarn,
             btnBg, btnBgPress, btnBorder, btnText, btnTextDis,
             sep;
};
static constexpr Palette LIGHT = {
    RGB(245,245,245), RGB(255,255,255), RGB(15,15,15),   RGB(120,120,120),
    RGB(0,140,0),     RGB(180,60,0),
    RGB(230,230,230), RGB(210,210,210), RGB(200,200,200), RGB(20,20,20),  RGB(150,150,150),
    RGB(220,220,220),
};
static constexpr Palette DARK = {
    RGB(18,18,18),   RGB(28,28,28),   RGB(240,240,240), RGB(130,130,130),
    RGB(0,210,120),  RGB(240,110,80),
    RGB(45,45,45),   RGB(60,60,60),   RGB(75,75,75),    RGB(240,240,240), RGB(90,90,90),
    RGB(35,35,35),
};

// ─── Globals ──────────────────────────────────────────────────────────────────
static HWND   g_hWnd       = nullptr;
static HWND   g_hLog       = nullptr;
static HWND   g_hPath      = nullptr;
static HWND   g_hBtnSelect = nullptr;
static HWND   g_hBtnInject = nullptr;
static HWND   g_hBtnTheme  = nullptr;
static HWND   g_hStatus    = nullptr;
static HWND   g_hCredit    = nullptr;

static HBRUSH g_brBg   = nullptr;
static HBRUSH g_brEdit = nullptr;

static HFONT  g_fUI    = nullptr;   // Segoe UI 9pt
static HFONT  g_fBold  = nullptr;   // Segoe UI 10pt semibold
static HFONT  g_fSmall = nullptr;   // Segoe UI 8pt
static HFONT  g_fMono  = nullptr;   // Consolas 9pt

static wchar_t g_dll[MAX_PATH]   = {};
static wchar_t g_stBuf[256]      = L"Ожидание...";
static bool    g_isAdmin         = false;
static bool    g_mcOk            = false;
static bool    g_dark            = false;

static const Palette& P() { return g_dark ? DARK : LIGHT; }

// ─── Helpers ──────────────────────────────────────────────────────────────────
static bool IsElevated() {
    HANDLE tok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) return false;
    TOKEN_ELEVATION e{}; DWORD sz = sizeof(e);
    bool ok = GetTokenInformation(tok, TokenElevation, &e, sz, &sz) && e.TokenIsElevated;
    CloseHandle(tok); return ok;
}

static void Log(const wchar_t* fmt, ...) {
    wchar_t buf[1024]; va_list v; va_start(v, fmt);
    vswprintf(buf, 1024, fmt, v); va_end(v);
    int n = (int)SendMessageW(g_hLog, WM_GETTEXTLENGTH, 0, 0);
    SendMessageW(g_hLog, EM_SETSEL, n, n);
    SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)buf);
    SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
}

static void SetStatus(bool ok, const wchar_t* text) {
    g_mcOk = ok;
    wcsncpy(g_stBuf, text, 255); g_stBuf[255] = L'\0';
    PostMessageW(g_hWnd, WM_APP_SETSTATUS, 0, 0);
}

static void RebuildBrushes() {
    if (g_brBg)   { DeleteObject(g_brBg);   g_brBg   = nullptr; }
    if (g_brEdit) { DeleteObject(g_brEdit); g_brEdit = nullptr; }
    g_brBg   = CreateSolidBrush(P().bg);
    g_brEdit = CreateSolidBrush(P().editBg);
}

static void SetupDarkScrollbar(HWND hwnd, bool dark) {
    HMODULE hUxTheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hUxTheme) {
        typedef int (WINAPI* fnSetPreferredAppMode)(int mode);
        auto pSetPreferredAppMode = (fnSetPreferredAppMode)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135));
        if (pSetPreferredAppMode) {
            pSetPreferredAppMode(dark ? 2 : 0); // 2 = ForceDark, 0 = Default
        }
        
        typedef bool (WINAPI* fnAllowDarkModeForWindow)(HWND hwnd, bool allow);
        auto pAllowDarkModeForWindow = (fnAllowDarkModeForWindow)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(133));
        if (pAllowDarkModeForWindow) {
            pAllowDarkModeForWindow(hwnd, dark);
            pAllowDarkModeForWindow(g_hWnd, dark);
        }
        FreeLibrary(hUxTheme);
    }
    SetWindowTheme(hwnd, dark ? L"Explorer" : nullptr, nullptr);
}

static void ApplyTheme() {
    RebuildBrushes();
    BOOL dark = g_dark ? TRUE : FALSE;
    DwmSetWindowAttribute(g_hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DwmSetWindowAttribute(g_hWnd, 19, &dark, sizeof(dark)); // older Win10
    
    const wchar_t* et = g_dark ? L"DarkMode_CFD" : L"";
    SetWindowTheme(g_hPath, et, nullptr);
    
    // Окрашиваем ползунок в темный цвет (для Win10/11)
    SetupDarkScrollbar(g_hLog, g_dark);
    
    SetWindowTextW(g_hBtnTheme, g_dark ? L"☀  Светлая" : L"☾  Тёмная");
    InvalidateRect(g_hWnd, nullptr, TRUE);
    // Force child controls to repaint
    EnumChildWindows(g_hWnd, [](HWND h, LPARAM) -> BOOL {
        InvalidateRect(h, nullptr, TRUE); return TRUE;
    }, 0);
}

// ─── Core injection logic ─────────────────────────────────────────────────────
static DWORD FindProc(const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{ sizeof(pe) }; DWORD pid = 0;
    if (Process32FirstW(snap, &pe))
        do { if (!_wcsicmp(pe.szExeFile, name)) { pid = pe.th32ProcessID; break; } }
        while (Process32NextW(snap, &pe));
    CloseHandle(snap); return pid;
}

static bool GrantUWP(const wchar_t* path) {
    PSID sid = nullptr;
    if (!ConvertStringSidToSidW(L"S-1-15-2-1", &sid)) {
        Log(L"[!] ConvertStringSidToSid: %lu", GetLastError()); return false;
    }
    EXPLICIT_ACCESS_W ea{};
    ea.grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE;
    ea.grfAccessMode        = SET_ACCESS;
    ea.grfInheritance       = NO_INHERITANCE;
    ea.Trustee.TrusteeForm  = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType  = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea.Trustee.ptstrName    = (LPWSTR)sid;
    PACL old = nullptr, fresh = nullptr; PSECURITY_DESCRIPTOR sd = nullptr;
    DWORD e = GetNamedSecurityInfoW(path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                    nullptr, nullptr, &old, nullptr, &sd);
    if (e) { Log(L"[!] GetNamedSecurityInfo: %lu", e); LocalFree(sid); return false; }
    e = SetEntriesInAclW(1, &ea, old, &fresh);
    if (e) { Log(L"[!] SetEntriesInAcl: %lu", e); LocalFree(sd); LocalFree(sid); return false; }
    e = SetNamedSecurityInfoW((LPWSTR)path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                               nullptr, nullptr, fresh, nullptr);
    LocalFree(fresh); LocalFree(sd); LocalFree(sid);
    if (e) { Log(L"[!] SetNamedSecurityInfo: %lu", e); return false; }
    return true;
}

static bool InjectDLL(DWORD pid, const wchar_t* dll) {
    size_t sz = (wcslen(dll) + 1) * sizeof(wchar_t);
    HANDLE h = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
                           PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!h) { Log(L"[!] OpenProcess: %lu", GetLastError()); return false; }
    LPVOID rm = VirtualAllocEx(h, nullptr, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!rm) { Log(L"[!] VirtualAllocEx: %lu", GetLastError()); CloseHandle(h); return false; }
    if (!WriteProcessMemory(h, rm, dll, sz, nullptr)) {
        Log(L"[!] WriteProcessMemory: %lu", GetLastError());
        VirtualFreeEx(h, rm, 0, MEM_RELEASE); CloseHandle(h); return false;
    }
    auto pLL = (LPTHREAD_START_ROUTINE)GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HANDLE t = pLL ? CreateRemoteThread(h, nullptr, 0, pLL, rm, 0, nullptr) : nullptr;
    if (!t) {
        Log(L"[!] CreateRemoteThread: %lu", GetLastError());
        VirtualFreeEx(h, rm, 0, MEM_RELEASE); CloseHandle(h); return false;
    }
    WaitForSingleObject(t, 8000);
    DWORD code = 0; GetExitCodeThread(t, &code);
    CloseHandle(t); VirtualFreeEx(h, rm, 0, MEM_RELEASE); CloseHandle(h);
    return code != 0;
}

static DWORD WINAPI Worker(LPVOID param) {
    wchar_t* dll = static_cast<wchar_t*>(param);
    SetStatus(false, L"Поиск Minecraft.Windows.exe...");
    Log(L"[*] Ожидание Minecraft.Windows.exe...");
    DWORD pid = 0; bool warned = false;
    for (int i = 0; i < 120 && !pid; ++i) {
        pid = FindProc(L"Minecraft.Windows.exe");
        if (!pid) {
            if (!warned) { SetStatus(false, L"Minecraft не запущен — откройте игру"); warned = true; }
            Sleep(500);
        }
    }
    if (!pid) {
        SetStatus(false, L"Ошибка: таймаут — Minecraft не найден");
        Log(L"[!] Процесс не найден за 60 сек.");
        EnableWindow(g_hBtnInject, TRUE); delete[] dll; return 1;
    }
    wchar_t s[256];
    swprintf(s, 256, L"Minecraft обнаружен  (PID: %lu)", pid);
    SetStatus(true, s);
    Log(L"[+] Minecraft.Windows.exe  PID: %lu", pid);
    Log(L"[*] Пауза 4 с...");
    Sleep(4000);
    if (DWORD p2 = FindProc(L"Minecraft.Windows.exe")) pid = p2;
    swprintf(s, 256, L"Подключено к Minecraft  (PID: %lu)", pid);
    SetStatus(true, s);
    Log(L"[+] Подключено. PID: %lu", pid);
    Log(L"[*] Выдача прав ALL_APPLICATION_PACKAGES...");
    if (!GrantUWP(dll)) {
        SetStatus(false, L"Ошибка прав — нужен Администратор");
        EnableWindow(g_hBtnInject, TRUE); delete[] dll; return 1;
    }
    Log(L"[+] Права выданы (S-1-15-2-1).");
    Log(L"[*] Внедрение DLL...");
    if (InjectDLL(pid, dll)) {
        swprintf(s, 256, L"Внедрено успешно  (PID: %lu)", pid);
        SetStatus(true, s);
        Log(L"");
        Log(L"[+] ╔══════════════════════════╗");
        Log(L"[+] ║   DLL внедрена!   ✓      ║");
        Log(L"[+] ╚══════════════════════════╝");
    } else {
        SetStatus(false, L"Инъекция не удалась — см. лог");
        Log(L"[!] Инъекция не удалась.");
        Log(L"    1. Запустите от имени Администратора");
        Log(L"    2. Дождитесь главного меню Minecraft");
        Log(L"    3. Нажмите Inject ещё раз");
    }
    EnableWindow(g_hBtnInject, TRUE); delete[] dll; return 0;
}

static void HandleDllSelection(HWND hwnd, const wchar_t* path) {
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) {
        Log(L"[!] Файл не найден: %s", path);
        return;
    }
    const wchar_t* filename = wcsrchr(path, L'\\') ? wcsrchr(path, L'\\') + 1 : path;
    wcscpy(g_dll, path);
    SetWindowTextW(g_hPath, g_dll);
    EnableWindow(g_hBtnInject, TRUE);
    Log(L"[+] Выбрана DLL: %s", filename);
}

// ─── UI actions ───────────────────────────────────────────────────────────────
static void OnSelect(HWND hwnd) {
    OPENFILENAMEW ofn{}; wchar_t buf[MAX_PATH]{};
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"DLL-библиотеки\0*.dll\0Все файлы\0*.*\0";
    ofn.lpstrFile = buf; ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Выберите DLL"; ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"dll";
    if (!GetOpenFileNameW(&ofn)) return;
    HandleDllSelection(hwnd, buf);
}

static void OnInject() {
    if (!g_dll[0]) { Log(L"[!] Сначала выберите DLL."); return; }
    if (GetFileAttributesW(g_dll) == INVALID_FILE_ATTRIBUTES) {
        Log(L"[!] Файл не найден: %s", g_dll); return;
    }
    if (!g_isAdmin && MessageBoxW(g_hWnd,
        L"Нет прав Администратора.\nOpenProcess и ACL провалятся.\nВсё равно продолжить?",
        L"Предупреждение", MB_YESNO | MB_ICONWARNING) != IDYES) return;

    EnableWindow(g_hBtnInject, FALSE);
    SetStatus(false, L"Запуск...");
    wchar_t* copy = new wchar_t[MAX_PATH]; wcscpy(copy, g_dll);
    HANDLE t = CreateThread(nullptr, 0, Worker, copy, 0, nullptr);
    if (t) CloseHandle(t);
    else { Log(L"[!] CreateThread: %lu", GetLastError()); EnableWindow(g_hBtnInject, TRUE); delete[] copy; }
}

// ─── Owner-draw button ────────────────────────────────────────────────────────
static void DrawBtn(DRAWITEMSTRUCT* di) {
    bool pressed  = (di->itemState & ODS_SELECTED) != 0;
    bool disabled = (di->itemState & ODS_DISABLED)  != 0;
    RECT rc = di->rcItem; HDC dc = di->hDC;

    // Clear entire rect with window bg (fixes rounded-corner bleed)
    HBRUSH bgBr = CreateSolidBrush(P().bg);
    FillRect(dc, &rc, bgBr); DeleteObject(bgBr);

    // Button fill + border in one call via Rectangle (GDI)
    COLORREF clrBg = disabled ? P().bg : (pressed ? P().btnBgPress : P().btnBg);
    HPEN   pen  = CreatePen(PS_SOLID, 1, disabled ? P().btnBorder : P().btnBorder);
    HBRUSH fill = CreateSolidBrush(clrBg);
    HPEN   op   = (HPEN)SelectObject(dc, pen);
    HBRUSH ob   = (HBRUSH)SelectObject(dc, fill);
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, 5, 5);
    SelectObject(dc, op); SelectObject(dc, ob);
    DeleteObject(pen); DeleteObject(fill);

    if (di->itemState & ODS_FOCUS) {
        RECT fr = { rc.left+3, rc.top+3, rc.right-3, rc.bottom-3 };
        DrawFocusRect(dc, &fr);
    }
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, disabled ? P().btnTextDis : P().btnText);
    HFONT of = (HFONT)SelectObject(dc, g_fUI);
    wchar_t txt[128]; GetWindowTextW(di->hwndItem, txt, 128);
    DrawTextW(dc, txt, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);
}

#define ID_CREDIT      106

// ─── WndProc ──────────────────────────────────────────────────────────────────
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_CREATE: {
        g_hWnd = hwnd; g_isAdmin = IsElevated(); g_dark = true;

        HDC hdc = GetDC(hwnd);
        int dy = GetDeviceCaps(hdc, LOGPIXELSY); ReleaseDC(hwnd, hdc);
        auto mkF = [&](int pt, int w, const wchar_t* face) {
            return CreateFontW(-MulDiv(pt, dy, 72), 0, 0, 0, w, 0, 0, 0,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
        };
        g_fUI    = mkF(9,  FW_NORMAL,   L"Segoe UI");
        g_fBold  = mkF(10, FW_SEMIBOLD, L"Segoe UI");
        g_fSmall = mkF(8,  FW_NORMAL,   L"Segoe UI");
        g_fMono  = mkF(9,  FW_NORMAL,   L"Consolas");

        RebuildBrushes();

        auto mk = [&](const wchar_t* cls, const wchar_t* txt, DWORD sty,
                      int x, int y, int w, int h, UINT id, HFONT f = nullptr) {
            HWND c = CreateWindowW(cls, txt, WS_CHILD | WS_VISIBLE | sty,
                                   x, y, w, h, hwnd, (HMENU)(UINT_PTR)id, nullptr, nullptr);
            SendMessageW(c, WM_SETFONT, (WPARAM)(f ? f : g_fUI), TRUE);
            return c;
        };

        // Header
        mk(L"STATIC",  L"MCPE DLL Injector", SS_LEFT,
           10, 10, 220, 20, 0, g_fBold);
        
        g_hBtnTheme = mk(L"BUTTON", L"☀  Светлая", BS_PUSHBUTTON | BS_OWNERDRAW,
           418, 7, 80, 26, ID_BTN_THEME);

        // Credit link (at the bottom)
        g_hCredit = mk(L"STATIC", L"anx1ous", SS_RIGHT | SS_NOTIFY,
           440, 365, 60, 16, ID_CREDIT, g_fUI);

        // DLL row
        mk(L"STATIC", L"DLL:", 0, 10, 53, 32, 18, 0);
        g_hPath = mk(L"EDIT", L"(не выбрана)",
            WS_BORDER | ES_READONLY | ES_AUTOHSCROLL,
            46, 50, 452, 22, ID_EDIT_PATH);

        // Buttons
        g_hBtnSelect = mk(L"BUTTON", L"Import DLL",
            BS_PUSHBUTTON | BS_OWNERDRAW, 10, 82, 150, 30, ID_BTN_SELECT);
        g_hBtnInject = mk(L"BUTTON", L"Inject",
            BS_PUSHBUTTON | BS_OWNERDRAW | WS_DISABLED, 170, 82, 150, 30, ID_BTN_INJECT);

        // Status
        g_hStatus = mk(L"STATIC", g_stBuf, SS_CENTER, 10, 130, 490, 20, 0);

        // Log
        mk(L"STATIC", L"Лог:", 0, 10, 164, 60, 16, 0);
        g_hLog = mk(L"EDIT", L"",
            WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            10, 182, 490, 178, ID_EDIT_LOG, g_fMono);

        ApplyTheme();
        
        // Разрешаем Drag & Drop сообщений через фильтр UIPI (когда инжектор запущен от админа)
        #ifndef MSGFLT_ADD
        #  define MSGFLT_ADD 1
        #endif
        ChangeWindowMessageFilter(WM_DROPFILES, MSGFLT_ADD);
        ChangeWindowMessageFilter(0x0049, MSGFLT_ADD); // WM_COPYGLOBALDATA

        DragAcceptFiles(hwnd, TRUE);

        if (g_isAdmin) Log(L"[✓] Запущен с правами Администратора.");
        else           Log(L"[!] Нет прав Администратора — инъекция не удастся.");
        Log(L"Выберите DLL и нажмите Inject (или просто перетащите её сюда).");
        return 0;
    }

    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wp;
        wchar_t buf[MAX_PATH] = {};
        if (DragQueryFileW(hDrop, 0, buf, MAX_PATH)) {
            HandleDllSelection(hwnd, buf);
        }
        DragFinish(hDrop);
        return 0;
    }

    case WM_APP_SETSTATUS:
        SetWindowTextW(g_hStatus, g_stBuf);
        InvalidateRect(g_hStatus, nullptr, TRUE);
        return 0;

    case WM_DRAWITEM:
        if (((DRAWITEMSTRUCT*)lp)->CtlType == ODT_BUTTON) {
            DrawBtn((DRAWITEMSTRUCT*)lp); return TRUE;
        }
        break;

    // Draw separator lines
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps);
        HPEN pen = CreatePen(PS_SOLID, 1, P().sep);
        HPEN op  = (HPEN)SelectObject(dc, pen);
        for (int y : SEP_Y) { MoveToEx(dc, 10, y, nullptr); LineTo(dc, 500, y); }
        SelectObject(dc, op); DeleteObject(pen);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, g_brBg);
        return 1;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp; HWND ctrl = (HWND)lp;
        SetBkMode(dc, TRANSPARENT);
        if (ctrl == g_hStatus) {
            SetTextColor(dc, g_mcOk ? P().statusOk : P().statusWarn);
        } else if (ctrl == g_hCredit) {
            SetTextColor(dc, P().dim); // Или можно сделать синим/зеленым (P().statusOk)
        } else {
            SetTextColor(dc, P().text);
        }
        return (LRESULT)g_brBg;
    }

    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, P().text);
        SetBkColor(dc, P().editBg);
        return (LRESULT)g_brEdit;
    }

    case WM_SETCURSOR: {
        if ((HWND)wp == g_hCredit) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        break; // Продолжаем стандартную обработку
    }

    case WM_COMMAND:
        if (LOWORD(wp) == ID_CREDIT && HIWORD(wp) == STN_CLICKED) {
            ShellExecuteW(hwnd, L"open", L"https://t.me/anx1ous", nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        }
        switch (LOWORD(wp)) {
        case ID_BTN_SELECT: OnSelect(hwnd); break;
        case ID_BTN_INJECT: OnInject();     break;
        case ID_BTN_THEME:  g_dark = !g_dark; ApplyTheme(); break;
        }
        return 0;

    case WM_DESTROY:
        if (g_brBg)   DeleteObject(g_brBg);
        if (g_brEdit) DeleteObject(g_brEdit);
        if (g_fUI)    DeleteObject(g_fUI);
        if (g_fBold)  DeleteObject(g_fBold);
        if (g_fSmall) DeleteObject(g_fSmall);
        if (g_fMono)  DeleteObject(g_fMono);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ─── Entry point ──────────────────────────────────────────────────────────────
int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int show) {
    INITCOMMONCONTROLSEX icex{ sizeof(icex), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hi;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"MCPEInjWnd";
    wc.hIcon = wc.hIconSm = LoadIconW(nullptr, IDI_SHIELD);
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowW(L"MCPEInjWnd", L"MCPE UWP DLL Injector",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 516, 410,
        nullptr, nullptr, hi, nullptr);
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        TranslateMessage(&m); DispatchMessageW(&m);
    }
    return (int)m.wParam;
}
