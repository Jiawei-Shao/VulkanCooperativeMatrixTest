#include "Window.h"

LRESULT CALLBACK WindowProc(
    HWND hWnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    switch (message) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
    case WM_KEYDOWN:
    case WM_POINTERDOWN:
    case WM_POINTERUPDATE:
    case WM_POINTERUP:
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

HWND CreateAppWindow() {
    WNDCLASSEX windowClass;
    ZeroMemory(&windowClass, sizeof(WNDCLASSEX));
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = GetModuleHandle(NULL);
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = L"TraversalShaderApp";
    RegisterClassEx(&windowClass);

    enum {
        basicStyle = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE,
        windowedStyle = basicStyle | WS_OVERLAPPEDWINDOW,
        fullscreenStyle = basicStyle
    };
    RECT windowRect = { 0, 0, 1366, 768 };
    AdjustWindowRect(&windowRect, windowedStyle, FALSE);
    EnableMouseInPointer(TRUE);

    HWND hwnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        L"TraversalShaderApp",
        L"TraversalShader",
        windowedStyle,
        0,
        0,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        windowClass.hInstance,
        nullptr);

    SetForegroundWindow(hwnd);

    return hwnd;
}