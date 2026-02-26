#if !defined(_WIN32)
#error "V2RayPort builds only on Windows. Use Visual Studio/MSVC on Windows 10/11."
#endif

#if defined(__has_include)
#if !__has_include(<windows.h>)
#error "windows.h not found. Install Windows SDK (10/11) and build from Visual Studio Developer PowerShell."
#endif
#endif

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <atomic>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Msimg32.lib")

namespace {
constexpr int ID_XRAY_PATH = 101;
constexpr int ID_CONFIG_PATH = 102;
constexpr int ID_WINTUN_PATH = 103;
constexpr int ID_BROWSE_XRAY = 104;
constexpr int ID_BROWSE_CONFIG = 105;
constexpr int ID_BROWSE_WINTUN = 106;
constexpr int ID_START = 107;
constexpr int ID_STOP = 108;
constexpr int ID_LOG = 109;
constexpr int ID_STATUS = 110;
constexpr int ID_OPEN_RUNTIME = 111;
constexpr int ID_OPEN_CONFIG_DIR = 112;
constexpr int ID_CLEAR_LOG = 113;
constexpr int ID_PROGRESS = 114;

constexpr UINT WM_APPEND_LOG = WM_APP + 1;
constexpr UINT WM_PROCESS_EXITED = WM_APP + 2;
constexpr UINT TIMER_ANIMATION = 1;

constexpr COLORREF kBgTop = RGB(18, 22, 33);
constexpr COLORREF kBgBottom = RGB(11, 15, 24);
constexpr COLORREF kPanel = RGB(22, 30, 45);
constexpr COLORREF kAccent = RGB(0, 177, 255);
constexpr COLORREF kTextMain = RGB(235, 242, 252);
constexpr COLORREF kTextMuted = RGB(162, 178, 200);

struct UiState {
    HWND mainWindow = nullptr;
    HWND logBox = nullptr;
    HWND statusLabel = nullptr;
    HWND statusDot = nullptr;
    HWND startBtn = nullptr;
    HWND stopBtn = nullptr;
    HWND progress = nullptr;
    HWND subtitle = nullptr;

    HWND xrayEdit = nullptr;
    HWND configEdit = nullptr;
    HWND wintunEdit = nullptr;

    HFONT fontTitle = nullptr;
    HFONT fontBody = nullptr;
    HFONT fontSmall = nullptr;

    HBRUSH panelBrush = nullptr;
    HBRUSH editBrush = nullptr;

    int animPhase = 0;
    bool running = false;
} g_ui;

HANDLE g_processHandle = nullptr;
HANDLE g_pipeRead = nullptr;
HANDLE g_pipeWrite = nullptr;
std::thread g_readerThread;
std::atomic<bool> g_reading{false};

std::wstring SettingsPath() {
    wchar_t modulePath[MAX_PATH] = L"";
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    std::wstring path(modulePath);
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        path = path.substr(0, pos + 1);
    }
    path += L"v2rayport.ini";
    return path;
}

void SaveSetting(const wchar_t* key, const std::wstring& value) {
    WritePrivateProfileStringW(L"paths", key, value.c_str(), SettingsPath().c_str());
}

std::wstring LoadSetting(const wchar_t* key, const std::wstring& fallback = L"") {
    wchar_t buffer[2048] = L"";
    GetPrivateProfileStringW(L"paths", key, fallback.c_str(), buffer, 2048, SettingsPath().c_str());
    return buffer;
}

std::wstring QuoteArg(const std::wstring& value) {
    std::wstring escaped = L"\"";
    for (wchar_t ch : value) {
        if (ch == L'"') {
            escaped += L"\\\"";
        } else {
            escaped += ch;
        }
    }
    escaped += L"\"";
    return escaped;
}

bool FileExists(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring GetText(HWND field) {
    int len = GetWindowTextLengthW(field);
    std::wstring text(static_cast<size_t>(len), L'\0');
    GetWindowTextW(field, text.data(), len + 1);
    return text;
}

void SetStatus(const std::wstring& status, bool running) {
    SetWindowTextW(g_ui.statusLabel, status.c_str());
    g_ui.running = running;
    InvalidateRect(g_ui.statusDot, nullptr, TRUE);
}

void AppendLogText(const std::wstring& text) {
    int length = GetWindowTextLengthW(g_ui.logBox);
    SendMessageW(g_ui.logBox, EM_SETSEL, length, length);
    SendMessageW(g_ui.logBox, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
    SendMessageW(g_ui.logBox, EM_SCROLLCARET, 0, 0);
}

std::wstring PickFile(HWND owner, const wchar_t* title, const wchar_t* filter) {
    OPENFILENAMEW ofn{};
    wchar_t file[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.lpstrFilter = filter;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        return file;
    }
    return L"";
}

void CleanupProcessHandles() {
    if (g_pipeRead) {
        CloseHandle(g_pipeRead);
        g_pipeRead = nullptr;
    }
    if (g_pipeWrite) {
        CloseHandle(g_pipeWrite);
        g_pipeWrite = nullptr;
    }
    if (g_processHandle) {
        CloseHandle(g_processHandle);
        g_processHandle = nullptr;
    }
}

void SetControlsRunning(bool running) {
    EnableWindow(g_ui.startBtn, !running);
    EnableWindow(g_ui.stopBtn, running);
    SendMessageW(g_ui.progress, PBM_SETMARQUEE, running ? TRUE : FALSE, 40);
    ShowWindow(g_ui.progress, running ? SW_SHOW : SW_HIDE);
}

void ReaderLoop() {
    constexpr DWORD bufSize = 2048;
    std::vector<char> buffer(bufSize);

    while (g_reading) {
        DWORD bytesRead = 0;
        BOOL ok = ReadFile(g_pipeRead, buffer.data(), bufSize - 1, &bytesRead, nullptr);
        if (!ok || bytesRead == 0) {
            break;
        }

        buffer[bytesRead] = '\0';
        int needed = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), static_cast<int>(bytesRead), nullptr, 0);
        UINT cp = CP_UTF8;
        if (needed <= 0) {
            cp = CP_ACP;
            needed = MultiByteToWideChar(CP_ACP, 0, buffer.data(), static_cast<int>(bytesRead), nullptr, 0);
        }

        if (needed > 0) {
            std::wstring wide(static_cast<size_t>(needed), L'\0');
            MultiByteToWideChar(cp, 0, buffer.data(), static_cast<int>(bytesRead), wide.data(), needed);
            auto* copy = new std::wstring(wide);
            PostMessageW(g_ui.mainWindow, WM_APPEND_LOG, 0, reinterpret_cast<LPARAM>(copy));
        }
    }

    PostMessageW(g_ui.mainWindow, WM_PROCESS_EXITED, 0, 0);
}

bool StartXray() {
    std::wstring xrayPath = GetText(g_ui.xrayEdit);
    std::wstring configPath = GetText(g_ui.configEdit);
    std::wstring wintunPath = GetText(g_ui.wintunEdit);

    if (!FileExists(xrayPath)) {
        MessageBoxW(g_ui.mainWindow, L"Укажи корректный путь к xray.exe", L"Валидация", MB_ICONWARNING);
        return false;
    }
    if (!FileExists(configPath)) {
        MessageBoxW(g_ui.mainWindow, L"Укажи корректный путь к config.json", L"Валидация", MB_ICONWARNING);
        return false;
    }
    if (!wintunPath.empty() && !FileExists(wintunPath)) {
        MessageBoxW(g_ui.mainWindow, L"wintun.dll по указанному пути не найден", L"Валидация", MB_ICONWARNING);
        return false;
    }

    SaveSetting(L"xray", xrayPath);
    SaveSetting(L"config", configPath);
    SaveSetting(L"wintun", wintunPath);

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&g_pipeRead, &g_pipeWrite, &sa, 0)) {
        MessageBoxW(g_ui.mainWindow, L"Не удалось создать канал логов", L"Ошибка", MB_ICONERROR);
        return false;
    }
    SetHandleInformation(g_pipeRead, HANDLE_FLAG_INHERIT, 0);

    std::wstring command = QuoteArg(xrayPath) + L" run -config " + QuoteArg(configPath);
    std::wstring mutableCommand = command;

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = g_pipeWrite;
    si.hStdError = g_pipeWrite;

    PROCESS_INFORMATION pi{};

    std::wstring workingDir = xrayPath;
    size_t pos = workingDir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        workingDir = workingDir.substr(0, pos);
    }

    std::wstring previousPath;
    if (!wintunPath.empty()) {
        std::wstring currentPath(32768, L'\0');
        DWORD len = GetEnvironmentVariableW(L"PATH", currentPath.data(), static_cast<DWORD>(currentPath.size()));
        if (len > 0 && len < currentPath.size()) {
            currentPath.resize(len);
            previousPath = currentPath;
        }

        std::wstring dllDir = wintunPath;
        size_t dllPos = dllDir.find_last_of(L"\\/");
        if (dllPos != std::wstring::npos) {
            dllDir = dllDir.substr(0, dllPos);
            std::wstring merged = dllDir + L";" + previousPath;
            SetEnvironmentVariableW(L"PATH", merged.c_str());
        }
    }

    BOOL ok = CreateProcessW(
        nullptr,
        mutableCommand.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        workingDir.c_str(),
        &si,
        &pi);

    if (!wintunPath.empty() && !previousPath.empty()) {
        SetEnvironmentVariableW(L"PATH", previousPath.c_str());
    }

    if (!ok) {
        CleanupProcessHandles();
        MessageBoxW(g_ui.mainWindow, L"Не удалось запустить xray.exe", L"Ошибка запуска", MB_ICONERROR);
        return false;
    }

    CloseHandle(pi.hThread);
    g_processHandle = pi.hProcess;
    g_reading = true;
    g_readerThread = std::thread(ReaderLoop);

    SetControlsRunning(true);
    SetStatus(L"Статус: запущен", true);
    AppendLogText(L"[V2RayPort] Запуск xray.exe выполнен.\r\n");
    return true;
}

void StopXray(bool fromDestroy = false) {
    if (!g_processHandle) {
        return;
    }

    TerminateProcess(g_processHandle, 0);
    WaitForSingleObject(g_processHandle, 2500);

    g_reading = false;
    if (g_readerThread.joinable()) {
        g_readerThread.join();
    }

    CleanupProcessHandles();
    SetControlsRunning(false);
    SetStatus(L"Статус: остановлен", false);
    if (!fromDestroy) {
        AppendLogText(L"[V2RayPort] Процесс остановлен пользователем.\r\n");
    }
}

void OpenFolderForPath(const std::wstring& path) {
    if (path.empty()) {
        return;
    }
    std::wstring folder = path;
    size_t pos = folder.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        folder = folder.substr(0, pos);
    }
    ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void DrawGradientBackground(HDC hdc, RECT rc) {
    TRIVERTEX vert[2] = {
        {rc.left, rc.top, static_cast<COLOR16>(GetRValue(kBgTop) << 8), static_cast<COLOR16>(GetGValue(kBgTop) << 8), static_cast<COLOR16>(GetBValue(kBgTop) << 8), 0x0000},
        {rc.right, rc.bottom, static_cast<COLOR16>(GetRValue(kBgBottom) << 8), static_cast<COLOR16>(GetGValue(kBgBottom) << 8), static_cast<COLOR16>(GetBValue(kBgBottom) << 8), 0x0000},
    };
    GRADIENT_RECT gRect = {0, 1};
    GradientFill(hdc, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_V);
}

LRESULT CALLBACK StatusDotProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);

        HBRUSH bg = CreateSolidBrush(kPanel);
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        int pulse = g_ui.running ? (100 + (g_ui.animPhase % 80) * 2) : 120;
        COLORREF dotColor = g_ui.running ? RGB(0, static_cast<BYTE>(pulse), 120) : RGB(130, 130, 130);

        HBRUSH dot = CreateSolidBrush(dotColor);
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(20, 20, 20));
        HGDIOBJ oldBrush = SelectObject(hdc, dot);
        HGDIOBJ oldPen = SelectObject(hdc, pen);

        Ellipse(hdc, 2, 2, 18, 18);

        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(dot);
        DeleteObject(pen);
        EndPaint(hwnd, &ps);
        return 0;
    }
    default:
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }
}

void CreateLabeledField(HWND parent, const wchar_t* label, int y, HWND& editOut, int editId, int browseId, HINSTANCE instance) {
    HWND lbl = CreateWindowW(L"STATIC", label, WS_CHILD | WS_VISIBLE,
                             24, y + 4, 170, 22,
                             parent, nullptr, instance, nullptr);
    SendMessageW(lbl, WM_SETFONT, reinterpret_cast<WPARAM>(g_ui.fontBody), TRUE);

    editOut = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                              WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                              194, y, 500, 30,
                              parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(editId)), instance, nullptr);
    SendMessageW(editOut, WM_SETFONT, reinterpret_cast<WPARAM>(g_ui.fontBody), TRUE);

    HWND btn = CreateWindowW(L"BUTTON", L"Обзор", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                             706, y, 130, 30,
                             parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(browseId)), instance, nullptr);
    SendMessageW(btn, WM_SETFONT, reinterpret_cast<WPARAM>(g_ui.fontBody), TRUE);
}

void BuildUi(HINSTANCE instance) {
    g_ui.fontTitle = CreateFontW(34, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_ui.fontBody = CreateFontW(19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_ui.fontSmall = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    HWND header = CreateWindowW(L"STATIC", L"V2RayPort", WS_CHILD | WS_VISIBLE,
                                22, 14, 300, 44, g_ui.mainWindow, nullptr, instance, nullptr);
    SendMessageW(header, WM_SETFONT, reinterpret_cast<WPARAM>(g_ui.fontTitle), TRUE);

    g_ui.subtitle = CreateWindowW(L"STATIC", L"Красивый Windows-клиент для xray.exe с live логами", WS_CHILD | WS_VISIBLE,
                                  24, 56, 620, 24, g_ui.mainWindow, nullptr, instance, nullptr);
    SendMessageW(g_ui.subtitle, WM_SETFONT, reinterpret_cast<WPARAM>(g_ui.fontSmall), TRUE);

    CreateLabeledField(g_ui.mainWindow, L"Путь к xray.exe:", 96, g_ui.xrayEdit, ID_XRAY_PATH, ID_BROWSE_XRAY, instance);
    CreateLabeledField(g_ui.mainWindow, L"Путь к config.json:", 136, g_ui.configEdit, ID_CONFIG_PATH, ID_BROWSE_CONFIG, instance);
    CreateLabeledField(g_ui.mainWindow, L"Путь к wintun.dll:", 176, g_ui.wintunEdit, ID_WINTUN_PATH, ID_BROWSE_WINTUN, instance);

    g_ui.startBtn = CreateWindowW(L"BUTTON", L"▶ Запустить", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                  194, 222, 150, 34,
                                  g_ui.mainWindow, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_START)), instance, nullptr);
    SendMessageW(g_ui.startBtn, WM_SETFONT, reinterpret_cast<WPARAM>(g_ui.fontBody), TRUE);

    g_ui.stopBtn = CreateWindowW(L"BUTTON", L"■ Остановить", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 354, 222, 160, 34,
                                 g_ui.mainWindow, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_STOP)), instance, nullptr);
    SendMessageW(g_ui.stopBtn, WM_SETFONT, reinterpret_cast<WPARAM>(g_ui.fontBody), TRUE);
    EnableWindow(g_ui.stopBtn, FALSE);

    HWND clearLogBtn = CreateWindowW(L"BUTTON", L"Очистить лог", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     524, 222, 150, 34,
                                     g_ui.mainWindow, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CLEAR_LOG)), instance, nullptr);
    SendMessageW(clearLogBtn, WM_SETFONT, reinterpret_cast<WPARAM>(g_ui.fontBody), TRUE);

    HWND openRuntimeBtn = CreateWindowW(L"BUTTON", L"Открыть папку runtime", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        684, 222, 190, 34,
                                        g_ui.mainWindow, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_OPEN_RUNTIME)), instance, nullptr);
    SendMessageW(openRuntimeBtn, WM_SETFONT, reinterpret_cast<WPARAM>(g_ui.fontSmall), TRUE);

    HWND openConfigBtn = CreateWindowW(L"BUTTON", L"Открыть папку config", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                       684, 262, 190, 30,
                                       g_ui.mainWindow, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_OPEN_CONFIG_DIR)), instance, nullptr);
    SendMessageW(openConfigBtn, WM_SETFONT, reinterpret_cast<WPARAM>(g_ui.fontSmall), TRUE);

    g_ui.statusDot = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                   24, 230, 20, 20,
                                   g_ui.mainWindow, nullptr, instance, nullptr);
    SetWindowSubclass(g_ui.statusDot, StatusDotProc, 1, 0);

    g_ui.statusLabel = CreateWindowW(L"STATIC", L"Статус: готов к запуску", WS_CHILD | WS_VISIBLE,
                                     50, 230, 260, 24,
                                     g_ui.mainWindow, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_STATUS)), instance, nullptr);
    SendMessageW(g_ui.statusLabel, WM_SETFONT, reinterpret_cast<WPARAM>(g_ui.fontSmall), TRUE);

    g_ui.progress = CreateWindowExW(0, PROGRESS_CLASSW, L"",
                                    WS_CHILD | PBS_MARQUEE,
                                    24, 268, 650, 12,
                                    g_ui.mainWindow, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PROGRESS)), instance, nullptr);
    ShowWindow(g_ui.progress, SW_HIDE);

    g_ui.logBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                  WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                                      ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
                                  24, 300, 850, 300,
                                  g_ui.mainWindow, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_LOG)), instance, nullptr);
    SendMessageW(g_ui.logBox, WM_SETFONT, reinterpret_cast<WPARAM>(g_ui.fontSmall), TRUE);

    SetWindowTextW(g_ui.xrayEdit, LoadSetting(L"xray", L"runtime\\xray.exe").c_str());
    SetWindowTextW(g_ui.configEdit, LoadSetting(L"config", L"config.json").c_str());
    SetWindowTextW(g_ui.wintunEdit, LoadSetting(L"wintun", L"runtime\\wintun.dll").c_str());

    AppendLogText(L"[V2RayPort] Готово. Укажи пути и нажми «Запустить».\r\n");
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        SetTextColor(hdc, (control == g_ui.subtitle) ? kTextMuted : kTextMain);
        SetBkColor(hdc, kPanel);
        return reinterpret_cast<LRESULT>(g_ui.panelBrush);
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, kTextMain);
        SetBkColor(hdc, RGB(15, 22, 34));
        return reinterpret_cast<LRESULT>(g_ui.editBrush);
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        DrawGradientBackground(hdc, rc);

        HBRUSH panel = CreateSolidBrush(kPanel);
        RECT panelRc{12, 84, rc.right - 12, rc.bottom - 12};
        FillRect(hdc, &panelRc, panel);
        DeleteObject(panel);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_TIMER:
        if (wParam == TIMER_ANIMATION) {
            g_ui.animPhase = (g_ui.animPhase + 1) % 80;
            InvalidateRect(g_ui.statusDot, nullptr, TRUE);
        }
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == ID_BROWSE_XRAY) {
            std::wstring path = PickFile(hwnd, L"Выбери xray.exe", L"Executable\0*.exe\0All\0*.*\0");
            if (!path.empty()) SetWindowTextW(g_ui.xrayEdit, path.c_str());
        } else if (id == ID_BROWSE_CONFIG) {
            std::wstring path = PickFile(hwnd, L"Выбери config.json", L"JSON\0*.json\0All\0*.*\0");
            if (!path.empty()) SetWindowTextW(g_ui.configEdit, path.c_str());
        } else if (id == ID_BROWSE_WINTUN) {
            std::wstring path = PickFile(hwnd, L"Выбери wintun.dll", L"Dynamic Library\0*.dll\0All\0*.*\0");
            if (!path.empty()) SetWindowTextW(g_ui.wintunEdit, path.c_str());
        } else if (id == ID_START) {
            StartXray();
        } else if (id == ID_STOP) {
            StopXray();
        } else if (id == ID_CLEAR_LOG) {
            SetWindowTextW(g_ui.logBox, L"");
        } else if (id == ID_OPEN_RUNTIME) {
            OpenFolderForPath(GetText(g_ui.xrayEdit));
        } else if (id == ID_OPEN_CONFIG_DIR) {
            OpenFolderForPath(GetText(g_ui.configEdit));
        }
        return 0;
    }
    case WM_APPEND_LOG: {
        auto* text = reinterpret_cast<std::wstring*>(lParam);
        if (text) {
            AppendLogText(*text);
            delete text;
        }
        return 0;
    }
    case WM_PROCESS_EXITED:
        g_reading = false;
        if (g_readerThread.joinable()) {
            g_readerThread.join();
        }
        CleanupProcessHandles();
        SetControlsRunning(false);
        SetStatus(L"Статус: остановлен", false);
        AppendLogText(L"[V2RayPort] Процесс завершён.\r\n");
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ANIMATION);
        StopXray(true);
        if (g_readerThread.joinable()) {
            g_readerThread.join();
        }
        CleanupProcessHandles();
        DeleteObject(g_ui.fontTitle);
        DeleteObject(g_ui.fontBody);
        DeleteObject(g_ui.fontSmall);
        DeleteObject(g_ui.panelBrush);
        DeleteObject(g_ui.editBrush);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int cmdShow) {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"V2RayPortMainWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

    if (!RegisterClassW(&wc)) {
        return 1;
    }

    g_ui.mainWindow = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"V2RayPort — Windows desktop client",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        920,
        680,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!g_ui.mainWindow) {
        return 1;
    }

    g_ui.panelBrush = CreateSolidBrush(kPanel);
    g_ui.editBrush = CreateSolidBrush(RGB(15, 22, 34));

    BuildUi(instance);

    SetTimer(g_ui.mainWindow, TIMER_ANIMATION, 45, nullptr);

    ShowWindow(g_ui.mainWindow, cmdShow);
    UpdateWindow(g_ui.mainWindow);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
