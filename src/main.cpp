#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <sstream>

#pragma comment(lib, "Comctl32.lib")

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

constexpr UINT WM_APPEND_LOG = WM_APP + 1;
constexpr UINT WM_PROCESS_EXITED = WM_APP + 2;

HWND g_mainWindow = nullptr;
HWND g_logBox = nullptr;
HWND g_statusLabel = nullptr;
HWND g_startBtn = nullptr;
HWND g_stopBtn = nullptr;

HANDLE g_processHandle = nullptr;
HANDLE g_pipeRead = nullptr;
HANDLE g_pipeWrite = nullptr;
std::thread g_readerThread;
std::atomic<bool> g_running{false};

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

void SetStatus(const std::wstring& status) {
    SetWindowTextW(g_statusLabel, status.c_str());
}

void AppendLogText(const std::wstring& text) {
    int length = GetWindowTextLengthW(g_logBox);
    SendMessageW(g_logBox, EM_SETSEL, length, length);
    SendMessageW(g_logBox, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
    SendMessageW(g_logBox, EM_SCROLLCARET, 0, 0);
}

std::wstring GetText(HWND parent, int id) {
    HWND field = GetDlgItem(parent, id);
    int len = GetWindowTextLengthW(field);
    std::wstring result(static_cast<size_t>(len), L'\0');
    GetWindowTextW(field, result.data(), len + 1);
    return result;
}

bool FileExists(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
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

void CleanupProcess() {
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
    g_running = false;
}

void StopProcess(bool force = false) {
    if (!g_processHandle) {
        return;
    }

    if (!TerminateProcess(g_processHandle, 0) && !force) {
        MessageBoxW(g_mainWindow, L"Не удалось остановить xray.exe", L"Ошибка", MB_ICONERROR);
        return;
    }

    WaitForSingleObject(g_processHandle, 3000);
    CleanupProcess();
    EnableWindow(g_startBtn, TRUE);
    EnableWindow(g_stopBtn, FALSE);
    SetStatus(L"Статус: остановлен");
    AppendLogText(L"[V2RayPort] Процесс остановлен.\r\n");
}

void ReaderLoop() {
    constexpr DWORD bufSize = 2048;
    std::vector<char> buffer(bufSize);

    while (g_running) {
        DWORD bytesRead = 0;
        BOOL ok = ReadFile(g_pipeRead, buffer.data(), bufSize - 1, &bytesRead, nullptr);
        if (!ok || bytesRead == 0) {
            break;
        }

        buffer[bytesRead] = '\0';
        int needed = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), static_cast<int>(bytesRead), nullptr, 0);
        if (needed <= 0) {
            needed = MultiByteToWideChar(CP_ACP, 0, buffer.data(), static_cast<int>(bytesRead), nullptr, 0);
        }

        if (needed > 0) {
            std::wstring wide(static_cast<size_t>(needed), L'\0');
            if (MultiByteToWideChar(CP_UTF8, 0, buffer.data(), static_cast<int>(bytesRead), wide.data(), needed) <= 0) {
                MultiByteToWideChar(CP_ACP, 0, buffer.data(), static_cast<int>(bytesRead), wide.data(), needed);
            }
            auto* copy = new std::wstring(wide);
            PostMessageW(g_mainWindow, WM_APPEND_LOG, 0, reinterpret_cast<LPARAM>(copy));
        }
    }

    PostMessageW(g_mainWindow, WM_PROCESS_EXITED, 0, 0);
}

bool StartProcess(HWND window) {
    std::wstring xrayPath = GetText(window, ID_XRAY_PATH);
    std::wstring configPath = GetText(window, ID_CONFIG_PATH);
    std::wstring wintunPath = GetText(window, ID_WINTUN_PATH);

    if (!FileExists(xrayPath)) {
        MessageBoxW(window, L"Укажите корректный путь к xray.exe", L"Валидация", MB_ICONWARNING);
        return false;
    }

    if (!FileExists(configPath)) {
        MessageBoxW(window, L"Укажите корректный путь к JSON-конфигу", L"Валидация", MB_ICONWARNING);
        return false;
    }

    if (!wintunPath.empty() && !FileExists(wintunPath)) {
        MessageBoxW(window, L"Указанный .dll файл не найден", L"Валидация", MB_ICONWARNING);
        return false;
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&g_pipeRead, &g_pipeWrite, &sa, 0)) {
        MessageBoxW(window, L"Не удалось создать канал логов", L"Ошибка", MB_ICONERROR);
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
    std::wstring workingDir = xrayPath.substr(0, xrayPath.find_last_of(L"\\/"));

    if (!wintunPath.empty()) {
        std::wstring currentPath(32768, L'\0');
        DWORD len = GetEnvironmentVariableW(L"PATH", currentPath.data(), static_cast<DWORD>(currentPath.size()));
        if (len > 0 && len < currentPath.size()) {
            currentPath.resize(len);
        } else {
            currentPath.clear();
        }
        std::wstring dllDir = wintunPath.substr(0, wintunPath.find_last_of(L"\\/"));
        std::wstring merged = dllDir + L";" + currentPath;
        SetEnvironmentVariableW(L"PATH", merged.c_str());
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

    if (!ok) {
        CleanupProcess();
        MessageBoxW(window, L"Не удалось запустить xray.exe", L"Ошибка запуска", MB_ICONERROR);
        return false;
    }

    CloseHandle(pi.hThread);
    g_processHandle = pi.hProcess;
    g_running = true;
    g_readerThread = std::thread(ReaderLoop);

    EnableWindow(g_startBtn, FALSE);
    EnableWindow(g_stopBtn, TRUE);
    SetStatus(L"Статус: запущен");
    AppendLogText(L"[V2RayPort] Процесс запущен.\r\n");
    return true;
}

void CreateLabeledField(HWND parent, const wchar_t* label, int y, int editId, int browseId) {
    CreateWindowW(L"STATIC", label, WS_CHILD | WS_VISIBLE,
                  20, y + 4, 170, 24,
                  parent, nullptr, nullptr, nullptr);

    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                    190, y, 500, 28,
                    parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(editId)), nullptr, nullptr);

    CreateWindowW(L"BUTTON", L"Обзор", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  700, y, 120, 28,
                  parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(browseId)), nullptr, nullptr);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == ID_BROWSE_XRAY) {
            std::wstring path = PickFile(hwnd, L"Выберите xray.exe", L"Executable\0*.exe\0All\0*.*\0");
            if (!path.empty()) SetWindowTextW(GetDlgItem(hwnd, ID_XRAY_PATH), path.c_str());
        } else if (id == ID_BROWSE_CONFIG) {
            std::wstring path = PickFile(hwnd, L"Выберите JSON-конфиг", L"JSON\0*.json\0All\0*.*\0");
            if (!path.empty()) SetWindowTextW(GetDlgItem(hwnd, ID_CONFIG_PATH), path.c_str());
        } else if (id == ID_BROWSE_WINTUN) {
            std::wstring path = PickFile(hwnd, L"Выберите wintun.dll (опционально)", L"Dynamic Library\0*.dll\0All\0*.*\0");
            if (!path.empty()) SetWindowTextW(GetDlgItem(hwnd, ID_WINTUN_PATH), path.c_str());
        } else if (id == ID_START) {
            StartProcess(hwnd);
        } else if (id == ID_STOP) {
            StopProcess();
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
        if (g_readerThread.joinable()) {
            g_readerThread.join();
        }
        CleanupProcess();
        EnableWindow(g_startBtn, TRUE);
        EnableWindow(g_stopBtn, FALSE);
        SetStatus(L"Статус: остановлен");
        AppendLogText(L"[V2RayPort] Процесс завершён.\r\n");
        return 0;
    case WM_DESTROY:
        g_running = false;
        StopProcess(true);
        if (g_readerThread.joinable()) {
            g_readerThread.join();
        }
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
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"V2RayPortMainWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassW(&wc)) {
        return 1;
    }

    g_mainWindow = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"V2RayPort — Desktop client for Xray",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        880, 620,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!g_mainWindow) {
        return 1;
    }

    HFONT titleFont = CreateFontW(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    HWND header = CreateWindowW(L"STATIC", L"V2RayPort", WS_CHILD | WS_VISIBLE,
                                20, 12, 300, 40,
                                g_mainWindow, nullptr, instance, nullptr);
    SendMessageW(header, WM_SETFONT, reinterpret_cast<WPARAM>(titleFont), TRUE);

    HWND subtitle = CreateWindowW(L"STATIC", L"Desktop-оболочка для xray.exe / wintun.dll", WS_CHILD | WS_VISIBLE,
                                  20, 52, 500, 22,
                                  g_mainWindow, nullptr, instance, nullptr);

    CreateLabeledField(g_mainWindow, L"Путь к xray.exe:", 90, ID_XRAY_PATH, ID_BROWSE_XRAY);
    CreateLabeledField(g_mainWindow, L"Путь к config.json:", 130, ID_CONFIG_PATH, ID_BROWSE_CONFIG);
    CreateLabeledField(g_mainWindow, L"Путь к wintun.dll:", 170, ID_WINTUN_PATH, ID_BROWSE_WINTUN);

    g_startBtn = CreateWindowW(L"BUTTON", L"Запустить", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                               190, 215, 150, 32,
                               g_mainWindow, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_START)), instance, nullptr);

    g_stopBtn = CreateWindowW(L"BUTTON", L"Остановить", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                              350, 215, 150, 32,
                              g_mainWindow, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_STOP)), instance, nullptr);
    EnableWindow(g_stopBtn, FALSE);

    g_statusLabel = CreateWindowW(L"STATIC", L"Статус: остановлен", WS_CHILD | WS_VISIBLE,
                                  520, 222, 300, 22,
                                  g_mainWindow, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_STATUS)), instance, nullptr);

    g_logBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                               WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                                   ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
                               20, 265, 820, 300,
                               g_mainWindow, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_LOG)), instance, nullptr);

    SendMessageW(subtitle, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);

    ShowWindow(g_mainWindow, cmdShow);
    UpdateWindow(g_mainWindow);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(titleFont);
    return static_cast<int>(msg.wParam);
}
