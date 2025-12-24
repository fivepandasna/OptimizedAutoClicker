#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ID_START_BTN 1001
#define ID_STOP_BTN 1002
#define ID_HOURS 1003
#define ID_MINS 1004
#define ID_SECS 1005
#define ID_MS 1006
#define ID_OFFSET 1007
#define ID_HOTKEY_INPUT 1008
#define ID_HOTKEY_BTN 1009

// Global variables
HWND g_hWnd;
HWND g_hHours, g_hMins, g_hSecs, g_hMs, g_hOffset, g_hHotkeyInput;
HWND g_hStartBtn, g_hStopBtn;
HANDLE g_hClickThread = NULL;
BOOL g_bRunning = FALSE;
DWORD g_dwInterval = 100;
DWORD g_dwOffset = 0;
int g_nHotkey = VK_F6;
HHOOK g_hHook = NULL;

// Dark theme colors
#define BG_COLOR RGB(28, 28, 35)
#define PANEL_COLOR RGB(35, 35, 45)
#define TEXT_COLOR RGB(220, 220, 230)
#define ACCENT_COLOR RGB(100, 120, 255)
#define BTN_HOVER RGB(120, 140, 255)
#define INPUT_BG RGB(45, 45, 55)

HBRUSH g_hBgBrush, g_hPanelBrush, g_hInputBrush;
HFONT g_hFont, g_hFontBold;

// Click thread function
DWORD WINAPI ClickThread(LPVOID lpParam) {
    srand((unsigned int)time(NULL));
    
    while (g_bRunning) {
        INPUT input = {0};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        SendInput(1, &input, sizeof(INPUT));
        
        ZeroMemory(&input, sizeof(INPUT));
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &input, sizeof(INPUT));
        
        DWORD offset = g_dwOffset > 0 ? (rand() % g_dwOffset) : 0;
        Sleep(g_dwInterval + offset);
    }
    return 0;
}

void StartClicking() {
    if (g_bRunning) return;
    
    char buf[32];
    DWORD h = 0, m = 0, s = 0, ms = 0;
    
    GetWindowText(g_hHours, buf, 32);
    h = atoi(buf);
    GetWindowText(g_hMins, buf, 32);
    m = atoi(buf);
    GetWindowText(g_hSecs, buf, 32);
    s = atoi(buf);
    GetWindowText(g_hMs, buf, 32);
    ms = atoi(buf);
    GetWindowText(g_hOffset, buf, 32);
    g_dwOffset = atoi(buf);
    
    g_dwInterval = (h * 3600000) + (m * 60000) + (s * 1000) + ms;
    
    if (g_dwInterval < 1) g_dwInterval = 1;
    
    g_bRunning = TRUE;
    g_hClickThread = CreateThread(NULL, 0, ClickThread, NULL, 0, NULL);
    
    EnableWindow(g_hStartBtn, FALSE);
    EnableWindow(g_hStopBtn, TRUE);
}

void StopClicking() {
    if (!g_bRunning) return;
    
    g_bRunning = FALSE;
    if (g_hClickThread) {
        WaitForSingleObject(g_hClickThread, INFINITE);
        CloseHandle(g_hClickThread);
        g_hClickThread = NULL;
    }
    
    EnableWindow(g_hStartBtn, TRUE);
    EnableWindow(g_hStopBtn, FALSE);
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* pKbStruct = (KBDLLHOOKSTRUCT*)lParam;
        if (pKbStruct->vkCode == g_nHotkey) {
            if (g_bRunning) {
                StopClicking();
            } else {
                StartClicking();
            }
            return 1;
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

void DrawGradientRect(HDC hdc, RECT* rc, COLORREF c1, COLORREF c2) {
    TRIVERTEX vertex[2];
    vertex[0].x = rc->left;
    vertex[0].y = rc->top;
    vertex[0].Red = GetRValue(c1) << 8;
    vertex[0].Green = GetGValue(c1) << 8;
    vertex[0].Blue = GetBValue(c1) << 8;
    vertex[0].Alpha = 0x0000;
    
    vertex[1].x = rc->right;
    vertex[1].y = rc->bottom;
    vertex[1].Red = GetRValue(c2) << 8;
    vertex[1].Green = GetGValue(c2) << 8;
    vertex[1].Blue = GetBValue(c2) << 8;
    vertex[1].Alpha = 0x0000;
    
    GRADIENT_RECT gRect = {0, 1};
    GradientFill(hdc, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_V);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, TEXT_COLOR);
            SetBkColor(hdcStatic, BG_COLOR);
            return (LRESULT)g_hBgBrush;
        }
        
        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, TEXT_COLOR);
            SetBkColor(hdcEdit, INPUT_BG);
            return (LRESULT)g_hInputBrush;
        }
        
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_START_BTN) {
                StartClicking();
            } else if (LOWORD(wParam) == ID_STOP_BTN) {
                StopClicking();
            } else if (LOWORD(wParam) == ID_HOTKEY_BTN) {
                char buf[32];
                GetWindowText(g_hHotkeyInput, buf, 32);
                if (strlen(buf) > 0) {
                    g_nHotkey = VkKeyScan(buf[0]);
                    char hotkeyText[64];
                    sprintf(hotkeyText, "Start (F%d)", g_nHotkey - VK_F1 + 1);
                    if (g_nHotkey >= 'A' && g_nHotkey <= 'Z') {
                        sprintf(hotkeyText, "Start (%c)", buf[0]);
                    }
                    SetWindowText(g_hStartBtn, hotkeyText);
                    sprintf(hotkeyText, "Stop (%c)", buf[0]);
                    SetWindowText(g_hStopBtn, hotkeyText);
                }
            }
            break;
            
        case WM_DESTROY:
            StopClicking();
            if (g_hHook) UnhookWindowsHookEx(g_hHook);
            DeleteObject(g_hBgBrush);
            DeleteObject(g_hPanelBrush);
            DeleteObject(g_hInputBrush);
            DeleteObject(g_hFont);
            DeleteObject(g_hFontBold);
            PostQuitMessage(0);
            break;
            
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

HWND CreateModernEdit(HWND parent, int x, int y, int w, int h, int id, const char* text) {
    HWND hEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        "EDIT", text,
        WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER,
        x, y, w, h,
        parent, (HMENU)id, GetModuleHandle(NULL), NULL
    );
    SendMessage(hEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    return hEdit;
}

HWND CreateModernButton(HWND parent, int x, int y, int w, int h, int id, const char* text) {
    HWND hBtn = CreateWindow(
        "BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_FLAT | BS_OWNERDRAW,
        x, y, w, h,
        parent, (HMENU)id, GetModuleHandle(NULL), NULL
    );
    SendMessage(hBtn, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
    return hBtn;
}

void CreateLabel(HWND parent, int x, int y, const char* text) {
    HWND hLabel = CreateWindow(
        "STATIC", text,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, 100, 20,
        parent, NULL, GetModuleHandle(NULL), NULL
    );
    SendMessage(hLabel, WM_SETFONT, (WPARAM)g_hFont, TRUE);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(BG_COLOR);
    wc.lpszClassName = "AutoClickerClass";
    
    RegisterClassEx(&wc);
    
    g_hBgBrush = CreateSolidBrush(BG_COLOR);
    g_hPanelBrush = CreateSolidBrush(PANEL_COLOR);
    g_hInputBrush = CreateSolidBrush(INPUT_BG);
    g_hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, 
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
    g_hFontBold = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
    
    g_hWnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        "AutoClickerClass",
        "AutoClicker Pro",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 380,
        NULL, NULL, hInstance, NULL
    );
    
    CreateLabel(g_hWnd, 20, 20, "Click Interval:");
    CreateLabel(g_hWnd, 20, 50, "Hours:");
    g_hHours = CreateModernEdit(g_hWnd, 70, 47, 60, 25, ID_HOURS, "0");
    CreateLabel(g_hWnd, 140, 50, "Mins:");
    g_hMins = CreateModernEdit(g_hWnd, 185, 47, 60, 25, ID_MINS, "0");
    CreateLabel(g_hWnd, 20, 85, "Secs:");
    g_hSecs = CreateModernEdit(g_hWnd, 70, 82, 60, 25, ID_SECS, "0");
    CreateLabel(g_hWnd, 140, 85, "Ms:");
    g_hMs = CreateModernEdit(g_hWnd, 185, 82, 60, 25, ID_MS, "100");
    
    CreateLabel(g_hWnd, 20, 130, "Random Offset (ms):");
    g_hOffset = CreateModernEdit(g_hWnd, 160, 127, 85, 25, ID_OFFSET, "0");
    
    CreateLabel(g_hWnd, 20, 175, "Hotkey Settings:");
    CreateLabel(g_hWnd, 20, 200, "Key:");
    g_hHotkeyInput = CreateModernEdit(g_hWnd, 60, 197, 40, 25, ID_HOTKEY_INPUT, "F6");
    CreateModernButton(g_hWnd, 110, 197, 80, 25, ID_HOTKEY_BTN, "Set Key");
    
    g_hStartBtn = CreateModernButton(g_hWnd, 20, 250, 170, 45, ID_START_BTN, "Start (F6)");
    g_hStopBtn = CreateModernButton(g_hWnd, 210, 250, 170, 45, ID_STOP_BTN, "Stop (F6)");
    EnableWindow(g_hStopBtn, FALSE);
    
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
    
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}