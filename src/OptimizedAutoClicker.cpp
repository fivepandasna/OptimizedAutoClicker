#include <windows.h>
#include <commctrl.h>
#include <thread>
#include <atomic>
#include <random>
#include <string>
#include <chrono>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define IDI_APPICON 101

// Global variables
HWND g_hwnd;
HWND g_hoursEdit, g_minsEdit, g_secsEdit, g_msEdit;
HWND g_offsetEdit, g_hotkeyBtn;
HWND g_startBtn, g_stopBtn;
std::atomic<bool> g_running(false);
std::atomic<bool> g_recordingKey(false);
std::thread g_clickThread;
int g_startStopKey = VK_F6;
std::wstring g_keyName = L"F6";
std::mt19937 g_rng(std::chrono::steady_clock::now().time_since_epoch().count());

// Modern colors
const COLORREF BG_COLOR = RGB(24, 24, 27);
const COLORREF INPUT_BG = RGB(39, 39, 42);
const COLORREF TEXT_COLOR = RGB(250, 250, 250);
const COLORREF ACCENT_COLOR = RGB(99, 102, 241);
const COLORREF BUTTON_HOVER = RGB(79, 82, 221);
const COLORREF STOP_COLOR = RGB(239, 68, 68);

HBRUSH g_bgBrush = CreateSolidBrush(BG_COLOR);
HBRUSH g_inputBrush = CreateSolidBrush(INPUT_BG);
HFONT g_font;

// Get key name from virtual key code
std::wstring GetKeyName(int vk) {
    switch (vk) {
        case VK_F1: return L"F1";
        case VK_F2: return L"F2";
        case VK_F3: return L"F3";
        case VK_F4: return L"F4";
        case VK_F5: return L"F5";
        case VK_F6: return L"F6";
        case VK_F7: return L"F7";
        case VK_F8: return L"F8";
        case VK_F9: return L"F9";
        case VK_F10: return L"F10";
        case VK_F11: return L"F11";
        case VK_F12: return L"F12";
        case VK_SPACE: return L"Space";
        case VK_RETURN: return L"Enter";
        case VK_ESCAPE: return L"Esc";
        case VK_TAB: return L"Tab";
        case VK_BACK: return L"Backspace";
        default:
            if (vk >= 'A' && vk <= 'Z') {
                return std::wstring(1, (wchar_t)vk);
            }
            if (vk >= '0' && vk <= '9') {
                return std::wstring(1, (wchar_t)vk);
            }
            return L"Unknown";
    }
}

// Update button text with current key
void UpdateButtonText() {
    std::wstring startText = L"Start (" + g_keyName + L")";
    std::wstring stopText = L"Stop (" + g_keyName + L")";
    SetWindowText(g_startBtn, startText.c_str());
    SetWindowText(g_stopBtn, stopText.c_str());
    InvalidateRect(g_startBtn, NULL, TRUE);
    InvalidateRect(g_stopBtn, NULL, TRUE);
}

// Get interval in milliseconds
int GetIntervalMs() {
    wchar_t buf[32];
    int hours = 0, mins = 0, secs = 0, ms = 0;
    
    GetWindowText(g_hoursEdit, buf, 32);
    hours = _wtoi(buf);
    GetWindowText(g_minsEdit, buf, 32);
    mins = _wtoi(buf);
    GetWindowText(g_secsEdit, buf, 32);
    secs = _wtoi(buf);
    GetWindowText(g_msEdit, buf, 32);
    ms = _wtoi(buf);
    
    return (hours * 3600000) + (mins * 60000) + (secs * 1000) + ms;
}

// Get random offset
int GetRandomOffset() {
    wchar_t buf[32];
    GetWindowText(g_offsetEdit, buf, 32);
    int maxOffset = _wtoi(buf);
    if (maxOffset <= 0) return 0;
    
    std::uniform_int_distribution<int> dist(0, maxOffset);
    return dist(g_rng);
}

// Click thread function
void ClickThreadFunc() {
    while (g_running) {
        int interval = GetIntervalMs();
        int offset = GetRandomOffset();
        int totalDelay = interval + offset;
        
        if (totalDelay < 1) totalDelay = 1;
        
        // Perform click
        INPUT input = {0};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        SendInput(1, &input, sizeof(INPUT));
        
        ZeroMemory(&input, sizeof(INPUT));
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &input, sizeof(INPUT));
        
        // High precision sleep
        auto start = std::chrono::high_resolution_clock::now();
        while (g_running) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            if (elapsed >= totalDelay) break;
            
            if (totalDelay - elapsed > 10)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            else
                std::this_thread::yield();
        }
    }
}

// Start clicking
void StartClicking() {
    if (!g_running) {
        g_running = true;
        if (g_clickThread.joinable()) g_clickThread.join();
        g_clickThread = std::thread(ClickThreadFunc);
        EnableWindow(g_startBtn, FALSE);
        EnableWindow(g_stopBtn, TRUE);
        InvalidateRect(g_startBtn, NULL, TRUE);
        InvalidateRect(g_stopBtn, NULL, TRUE);
    }
}

// Stop clicking
void StopClicking() {
    if (g_running) {
        g_running = false;
        if (g_clickThread.joinable()) g_clickThread.join();
        EnableWindow(g_startBtn, TRUE);
        EnableWindow(g_stopBtn, FALSE);
        InvalidateRect(g_startBtn, NULL, TRUE);
        InvalidateRect(g_stopBtn, NULL, TRUE);
    }
}

// Hotkey hook
HHOOK g_hook;
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* kbs = (KBDLLHOOKSTRUCT*)lParam;
        
        if (g_recordingKey) {
            g_startStopKey = kbs->vkCode;
            g_keyName = GetKeyName(kbs->vkCode);
            std::wstring btnText = L"Press a key... [" + g_keyName + L"]";
            SetWindowText(g_hotkeyBtn, btnText.c_str());
            g_recordingKey = false;
            
            // Schedule update for next message loop iteration
            PostMessage(g_hwnd, WM_USER + 1, 0, 0);
            return 1;
        }
        
        if (kbs->vkCode == g_startStopKey) {
            if (g_running) StopClicking();
            else StartClicking();
            return 1;
        }
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

// Custom button procedure
WNDPROC g_oldBtnProc;
LRESULT CALLBACK ButtonProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            bool isStop = (hwnd == g_stopBtn);
            bool isStart = (hwnd == g_startBtn);
            bool isHotkey = (hwnd == g_hotkeyBtn);
            bool isEnabled = IsWindowEnabled(hwnd);
            
            COLORREF color;
            if (isHotkey) {
                color = INPUT_BG;
            } else if (isStop) {
                // Stop button: red when enabled (clicking is active), gray when disabled
                color = isEnabled ? STOP_COLOR : RGB(60, 60, 65);
            } else if (isStart) {
                // Start button: blue when enabled (not clicking), gray when disabled
                color = isEnabled ? ACCENT_COLOR : RGB(60, 60, 65);
            } else {
                color = ACCENT_COLOR;
            }
            
            HBRUSH brush = CreateSolidBrush(color);
            FillRect(hdc, &rc, brush);
            DeleteObject(brush);
            
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, TEXT_COLOR);
            SelectObject(hdc, g_font);
            
            wchar_t text[64];
            GetWindowText(hwnd, text, 64);
            DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return CallWindowProc(g_oldBtnProc, hwnd, msg, wParam, lParam);
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_font = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            
            // Create labels and inputs
            int y = 20;
            CreateWindowEx(0, L"STATIC", L"Click Interval", WS_CHILD | WS_VISIBLE,
                20, y, 120, 20, hwnd, NULL, NULL, NULL);
            
            y += 30;
            CreateWindowEx(0, L"STATIC", L"Hours:", WS_CHILD | WS_VISIBLE,
                20, y, 50, 20, hwnd, NULL, NULL, NULL);
            g_hoursEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"0",
                WS_CHILD | WS_VISIBLE | ES_NUMBER, 75, y-2, 60, 24, hwnd, NULL, NULL, NULL);
            
            CreateWindowEx(0, L"STATIC", L"Mins:", WS_CHILD | WS_VISIBLE,
                145, y, 40, 20, hwnd, NULL, NULL, NULL);
            g_minsEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"0",
                WS_CHILD | WS_VISIBLE | ES_NUMBER, 190, y-2, 60, 24, hwnd, NULL, NULL, NULL);
            
            y += 35;
            CreateWindowEx(0, L"STATIC", L"Secs:", WS_CHILD | WS_VISIBLE,
                20, y, 50, 20, hwnd, NULL, NULL, NULL);
            g_secsEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"0",
                WS_CHILD | WS_VISIBLE | ES_NUMBER, 75, y-2, 60, 24, hwnd, NULL, NULL, NULL);
            
            CreateWindowEx(0, L"STATIC", L"Ms:", WS_CHILD | WS_VISIBLE,
                145, y, 40, 20, hwnd, NULL, NULL, NULL);
            g_msEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"100",
                WS_CHILD | WS_VISIBLE | ES_NUMBER, 190, y-2, 60, 24, hwnd, NULL, NULL, NULL);
            
            y += 45;
            CreateWindowEx(0, L"STATIC", L"Random Offset (0-N ms)", WS_CHILD | WS_VISIBLE,
                20, y, 200, 20, hwnd, NULL, NULL, NULL);
            
            y += 25;
            g_offsetEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"0",
                WS_CHILD | WS_VISIBLE | ES_NUMBER, 20, y, 230, 24, hwnd, NULL, NULL, NULL);
            
            y += 45;
            CreateWindowEx(0, L"STATIC", L"Hotkey Settings", WS_CHILD | WS_VISIBLE,
                20, y, 200, 20, hwnd, NULL, NULL, NULL);
            
            y += 25;
            CreateWindowEx(0, L"STATIC", L"Start/Stop Key:", WS_CHILD | WS_VISIBLE,
                20, y, 110, 20, hwnd, NULL, NULL, NULL);
            g_hotkeyBtn = CreateWindowEx(0, L"BUTTON", L"F6",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 135, y-2, 115, 28, hwnd, (HMENU)3, NULL, NULL);
            
            y += 45;
            g_startBtn = CreateWindowEx(0, L"BUTTON", L"Start (F6)",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 20, y, 110, 35, hwnd, (HMENU)1, NULL, NULL);
            
            g_stopBtn = CreateWindowEx(0, L"BUTTON", L"Stop (F6)",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 140, y, 110, 35, hwnd, (HMENU)2, NULL, NULL);
            
            EnableWindow(g_stopBtn, FALSE);
            
            g_oldBtnProc = (WNDPROC)SetWindowLongPtr(g_startBtn, GWLP_WNDPROC, (LONG_PTR)ButtonProc);
            SetWindowLongPtr(g_stopBtn, GWLP_WNDPROC, (LONG_PTR)ButtonProc);
            SetWindowLongPtr(g_hotkeyBtn, GWLP_WNDPROC, (LONG_PTR)ButtonProc);
            
            // Apply font to all children
            EnumChildWindows(hwnd, [](HWND child, LPARAM lParam) -> BOOL {
                SendMessage(child, WM_SETFONT, (WPARAM)g_font, TRUE);
                return TRUE;
            }, 0);
            
            g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
            break;
        }
        
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, TEXT_COLOR);
            SetBkColor(hdcStatic, msg == WM_CTLCOLOREDIT ? INPUT_BG : BG_COLOR);
            return (INT_PTR)(msg == WM_CTLCOLOREDIT ? g_inputBrush : g_bgBrush);
        }
        
        case WM_COMMAND:
            if (LOWORD(wParam) == 1) StartClicking();
            else if (LOWORD(wParam) == 2) StopClicking();
            else if (LOWORD(wParam) == 3) {
                g_recordingKey = true;
                SetWindowText(g_hotkeyBtn, L"Press a key...");
                InvalidateRect(g_hotkeyBtn, NULL, TRUE);
            }
            break;
        
        case WM_USER + 1:
            SetWindowText(g_hotkeyBtn, g_keyName.c_str());
            UpdateButtonText();
            InvalidateRect(g_hotkeyBtn, NULL, TRUE);
            break;
        
        case WM_DESTROY:
            StopClicking();
            if (g_hook) UnhookWindowsHookEx(g_hook);
            DeleteObject(g_bgBrush);
            DeleteObject(g_inputBrush);
            DeleteObject(g_font);
            PostQuitMessage(0);
            break;
        
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    WNDCLASSEX wc = {sizeof(WNDCLASSEX)};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"AutoClickerClass";
    wc.hbrBackground = g_bgBrush;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));
    
    RegisterClassEx(&wc);
    
    g_hwnd = CreateWindowEx(0, L"AutoClickerClass", L"Optimized AutoClicker",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 290, 380,
        NULL, NULL, hInstance, NULL);
    
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}