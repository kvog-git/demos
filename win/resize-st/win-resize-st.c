// ================================================================================================
// Example of how to draw while resizing / moving a window on Windows in C.
//
// Window contents are rendered via GDI, but the behavior is the same no matter what graphics API
// you use. The original version of this demo (on old-20260427 branch) used D3D11.
//
// This demo uses a single thread. There's a multi threaded version called win-resize-mt if that's
// more your style.
//
// Build (MSVC):
//     > cl /W4 /Od /Zi win-resize-st.c /Fe:win-resize-st.exe
// Build (GCC/clang):
//     $ cc -o win-resize-st.exe -Wall -Wextra -Wpedantic -O0 -g win-resize-st.c -ldwmapi -lgdi32
//
// Changelog:
//     5/15/2026: Initial release
//
// License:
//     Copyright (c) 2026 Hunter Kvalevog
//
//     Permission to use, copy, modify, and/or distribute this software for any
//     purpose with or without fee is hereby granted.
//
//     THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
//     WITH REGARD TO THIS SOFTWARE.
// ================================================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>

#ifdef _MSC_VER
#   pragma comment(lib, "dwmapi.lib")
#   pragma comment(lib, "gdi32.lib")
#   pragma comment(lib, "user32.lib")
#endif

#define UNUSED(Var) ((void)(Var))

static void GfxInit(HWND wnd);
static void GfxResize(UINT vpw, UINT vph);
static void GfxDraw(void);

//
static LRESULT CALLBACK WndProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            GfxResize(LOWORD(lparam), HIWORD(lparam));
            GfxDraw();
            break;
        case WM_ENTERSIZEMOVE:
            SetTimer(wnd, 1, USER_TIMER_MINIMUM, 0);
            break;
        case WM_EXITSIZEMOVE:
            KillTimer(wnd, 1);
            break;
        case WM_TIMER:
            GfxDraw();
            break;
    }
    return DefWindowProcW(wnd, msg, wparam, lparam);
}

//
int WinMain(HINSTANCE instance, HINSTANCE previnstance, LPSTR cmdline, int cmdshow)
{
    UNUSED(previnstance); UNUSED(cmdline);

    // Register window class
    WNDCLASSEXW wc = { 0 };
    wc.cbSize        = sizeof(wc);
    wc.hCursor       = LoadCursor(0, IDC_ARROW);
    wc.hInstance     = instance;
    wc.lpfnWndProc   = WndProc;
    wc.lpszClassName = L"win-resize-st";

    ATOM atom = RegisterClassExW(&wc);
    assert(atom && "Failed to register window class");

    // Create window
    HWND wnd = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, wc.lpszClassName,
                               WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
                               CW_USEDEFAULT, 0, 0, wc.hInstance, 0);
    assert(wnd && "Failed to create window");

    //
    GfxInit(wnd);

    //
    ShowWindow(wnd, cmdshow);

    // Run a typical D3D-style window loop
    BOOL quit = FALSE;
    while (!quit)
    {
        // Pump the message loop
        MSG msg;
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
        {
            quit |= (msg.message == WM_QUIT);
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // Update the screen
        GfxDraw();
    }

    return 0;
}

// ================================================================================================

static UINT    g_vpw       = 0;
static UINT    g_vph       = 0;
static HDC     g_hdc       = 0;
static HDC     g_hdc_back  = 0;
static HBITMAP g_bmp       = 0;
static HBRUSH  g_bgbr      = 0;
static HBRUSH  g_fgbr      = 0;
static HPEN    g_pen       = 0;

static void GfxInit(HWND wnd)
{
    g_hdc       = GetDC(wnd);
    g_hdc_back  = CreateCompatibleDC(g_hdc);
    g_bgbr      = CreateSolidBrush(RGB(0x00, 0x20, 0x20));
    g_fgbr      = CreateSolidBrush(RGB(0xE0, 0xE0, 0xE0));
    g_pen       = CreatePen(PS_SOLID, 2, RGB(0xFF, 0x10, 0x10));

    RECT rc;
    GetClientRect(wnd, &rc);

    GfxResize(rc.right, rc.bottom);
}

static void GfxResize(UINT vpw, UINT vph)
{
    if (g_bmp)
    {
        DeleteObject(g_bmp);
        g_bmp = 0;
    }

    if (vpw > 0 && vph > 0)
    {
        g_bmp = CreateCompatibleBitmap(g_hdc, vpw, vph);
        SelectObject(g_hdc_back, g_bmp);
    }

    g_vpw = vpw;
    g_vph = vph;
}

static void GfxDraw(void)
{
    static UINT frame = 0;
    ++frame;

    if (g_bmp && g_vpw > 0 && g_vph > 0)
    {
        HDC hdc = g_hdc_back;

        // Clear
        {
            RECT rc = { 0, 0, g_vpw, g_vph };
            FillRect(hdc, &rc, g_bgbr);
        }

        // Draw X
        {
            SelectObject(hdc, g_pen);
            MoveToEx(hdc, 0, 0, 0);
            LineTo(hdc, g_vpw, g_vph);
            MoveToEx(hdc, 0, g_vph, 0);
            LineTo(hdc, g_vpw, 0);
        }

        // Draw rotating triangle
        {
            int cx = g_vpw / 2;
            int cy = g_vph / 2;
            int r = (g_vpw < g_vph ? g_vpw : g_vph) / 3;

            POINT verts[3];
            for (int i = 0; i < 3; ++i)
            {
                float ang = ((float)frame / 100.0f) + (float)i * (2.0f * 3.14159265f / 3.0f);
                verts[i].x = cx + (int)(cosf(ang) * r);
                verts[i].y = cy + (int)(sinf(ang) * r);
            }

            SelectObject(hdc, g_pen);
            SelectObject(hdc, g_fgbr);
            Polygon(hdc, verts, ARRAYSIZE(verts));
        }

        // Draw text
        {
            WCHAR txtbuf[256];
            _snwprintf_s(txtbuf, ARRAYSIZE(txtbuf), _TRUNCATE, L"win-resize-st (frame %u)", frame);

            SIZE sz;
            GetTextExtentPoint32W(hdc, txtbuf, (int)wcslen(txtbuf), &sz);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(0xE0, 0xE0, 0xE0));
            TextOutW(hdc, (g_vpw - sz.cx) / 2, 19, txtbuf, (int)wcslen(txtbuf));
        }

        // Present
        BitBlt(g_hdc, 0, 0, g_vpw, g_vph, g_hdc_back, 0, 0, SRCCOPY);

        // Wait for dwm to display a frame
        DwmFlush();
    }
}

