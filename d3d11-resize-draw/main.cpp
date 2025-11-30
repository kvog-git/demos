// --------------------------------------------------------------------------------
// Direct3D 11 demo comparing various methods of drawing while resizing
// 
// ref: https://gist.github.com/d7samurai/261c69490cce0620d0bfc93003cd1052
//
// SPDX-License-Identifier: 0BSD
// --------------------------------------------------------------------------------

#define ACTIVE_METHOD 3

// --------------------------------------------------------------------------------
// Method 1: Don't do anything special. This is just to highlight the problem.
// --------------------------------------------------------------------------------
// #define ACTIVE_METHOD 1

// --------------------------------------------------------------------------------
// Method 2: Set CS_VREDRAW | CS_HREDRAW and render in WM_PAINT.
// 
// This is visually perfect, but some functions can't be called from inside
// WM_PAINT. For example, creating a modal dialog box with MessageBoxW() or
// assert() will block the message loop. When this happens, the app will hang or
// crash.
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
// Method 3: Render during resize with WM_TIMER.
// 
// This is a well-known workaround that has been generally proven safe. This is
// what sokol_app.h does. 
// 
// If you use WM_TIMER alone, there is a small gap at the side of the window when
// resizing, but it's barely visible.
// 
// If you draw on WM_TIMER and WM_SIZE, there is no bar, but there may be redundant
// draw calls.
// 
// Creating dialogs in WM_TIMER and WM_SIZE doesn't seem to be an issue.
// --------------------------------------------------------------------------------

#if !defined(ACTIVE_METHOD) || ACTIVE_METHOD < 1 || ACTIVE_METHOD > 3
#   error ACTIVE_METHOD not set
#endif

#include <cmath>
#include <cstdio>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

// --------------------------------------------------------------------------------
// Shared code
// --------------------------------------------------------------------------------

#define Assert(expr) do { if (!(expr)) { __debugbreak(); } } while (0);
#define HR(hr) do { HRESULT hr_ = (hr); Assert(SUCCEEDED(hr_)); } while (0);

struct CBuffer
{
    FLOAT color[4];
    FLOAT rot[16];
};

static struct
{
    HWND hwnd;
    IDXGISwapChain* d3dswp;
    ID3D11Device* d3ddev;
    ID3D11DeviceContext* d3dctx;
    ID3D11RenderTargetView* d3drtv;
    UINT vstride;
    UINT voffset;
    ID3D11Buffer* vbuf_quad;
    ID3D11Buffer* ibuf_quad;
    UINT ibuf_quad_count;
    ID3D11VertexShader* vs;
    ID3D11PixelShader* ps;
    ID3D11InputLayout* vlayout;
    ID3D11Buffer* cbuf;
    FLOAT vp_w;
    FLOAT vp_h;
} G = { 0 };

static void CreateDemoWindow(UINT class_style, WNDPROC wndproc)
{
    WNDCLASSW wcl = { .style = class_style, .lpfnWndProc = wndproc, .hInstance = GetModuleHandleW(0), .lpszClassName = L"d3d11-resize-draw"};
    RegisterClassW(&wcl);

    G.hwnd = CreateWindowExW(0, wcl.lpszClassName, wcl.lpszClassName, WS_OVERLAPPEDWINDOW, 0, 0, 1024, 768, 0, 0, 0, 0);
    Assert(G.hwnd);
}

static void DestroyRenderTarget()
{
    if (G.d3drtv) {
        G.d3drtv->Release();
        G.d3drtv = 0;
    }
}

static void CreateRenderTarget()
{
    Assert(!G.d3drtv);
    ID3D11Texture2D* tex;
    HR(G.d3dswp->GetBuffer(0, IID_PPV_ARGS(&tex)));
    HR(G.d3ddev->CreateRenderTargetView(tex, 0, &G.d3drtv));
    tex->Release();
}

static void SetupD3D11()
{
    DXGI_SWAP_CHAIN_DESC swap_chain_desc = {
        .BufferDesc = { .Format = DXGI_FORMAT_R8G8B8A8_UNORM, },
        .SampleDesc = { .Count = 1, },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2,
        .OutputWindow = G.hwnd,
        .Windowed = TRUE,
        .SwapEffect = DXGI_SWAP_EFFECT_DISCARD,
    };
    D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0 };

    HR(D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0, D3D11_CREATE_DEVICE_DEBUG, feature_levels, ARRAYSIZE(feature_levels), D3D11_SDK_VERSION, &swap_chain_desc, &G.d3dswp, &G.d3ddev, 0, &G.d3dctx));

    // Vertex shader
    {
        ID3DBlob* blob;
        HR(D3DCompileFromFile(L"shader.hlsl", 0, 0, "VsMain", "vs_5_0", 0, 0, &blob, nullptr));
        HR(G.d3ddev->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), 0, &G.vs));

        D3D11_INPUT_ELEMENT_DESC idesc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        HR(G.d3ddev->CreateInputLayout(idesc, ARRAYSIZE(idesc), blob->GetBufferPointer(), blob->GetBufferSize(), &G.vlayout));
        blob->Release();
    }
    // Pixel shader
    {
        ID3DBlob* blob;
        HR(D3DCompileFromFile(L"shader.hlsl", 0, 0, "PsMain", "ps_5_0", 0, 0, &blob, nullptr));
        HR(G.d3ddev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), 0, &G.ps));
        blob->Release();
    }

    struct Vertex { FLOAT p[2]; FLOAT t[2]; };
    G.vstride = sizeof(Vertex);
    G.voffset = 0;

    // Quad vertex buffer
    {
        Vertex vdata[] = {
            { { -1.0f, +1.0f }, { 0.0f, 0.0f } },
            { { +1.0f, +1.0f }, { 1.0f, 0.0f } },
            { { +1.0f, -1.0f }, { 1.0f, 1.0f } },
            { { -1.0f, -1.0f }, { 0.0f, 1.0f } },
        };
        D3D11_BUFFER_DESC bdesc = { .ByteWidth = sizeof(vdata), .Usage = D3D11_USAGE_IMMUTABLE, .BindFlags = D3D11_BIND_VERTEX_BUFFER };
        D3D11_SUBRESOURCE_DATA sdata = { .pSysMem = vdata };
        HR(G.d3ddev->CreateBuffer(&bdesc, &sdata, &G.vbuf_quad));
    }
    // Quad index buffer
    {
        UINT16 idata[] = {
            0, 1, 2,
            0, 2, 3,
        };
        D3D11_BUFFER_DESC bdesc = { .ByteWidth = sizeof(idata), .Usage = D3D11_USAGE_IMMUTABLE, .BindFlags = D3D11_BIND_INDEX_BUFFER };
        D3D11_SUBRESOURCE_DATA sdata = { .pSysMem = idata };
        HR(G.d3ddev->CreateBuffer(&bdesc, &sdata, &G.ibuf_quad));
        G.ibuf_quad_count = ARRAYSIZE(idata);
    }

    // Constant buffer
    {
        D3D11_BUFFER_DESC bdesc = { .ByteWidth = sizeof(CBuffer), .Usage = D3D11_USAGE_DYNAMIC, .BindFlags = D3D11_BIND_CONSTANT_BUFFER , .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE };
        HR(G.d3ddev->CreateBuffer(&bdesc, 0, &G.cbuf));
    }
}

static void MakeRotationZ(FLOAT angle, FLOAT* out)
{
    FLOAT c = cosf(angle);
    FLOAT s = sinf(angle);

    out[ 0] = c; out[ 1] = -s; out[ 2] = 0; out[ 3] = 0;
    out[ 4] = s; out[ 5] =  c; out[ 6] = 0; out[ 7] = 0;
    out[ 8] = 0; out[ 9] =  0; out[10] = 1; out[11] = 0;
    out[12] = 0; out[13] =  0; out[14] = 0; out[15] = 1;
}

static void MakeScale(FLOAT s, FLOAT* out)
{
    ZeroMemory(out, sizeof(FLOAT) * 16);
    out[ 0] = s;
    out[ 5] = s;
    out[10] = 1.0f;
    out[15] = 1.0f;
}

static void MulMat4(const FLOAT* A, const FLOAT* B, FLOAT* out)
{
    FLOAT r[16];
    for (UINT row = 0; row < 4; row++) {
        for (UINT col = 0; col < 4; col++) {
            r[row * 4 + col] = A[row * 4 + 0] * B[0 * 4 + col] + A[row * 4 + 1] * B[1 * 4 + col] + A[row * 4 + 2] * B[2 * 4 + col] + A[row * 4 + 3] * B[3 * 4 + col];
        }
    }
    memcpy(out, r, sizeof(r));
}

static void MakeTransform(FLOAT r, FLOAT s, FLOAT* out)
{
    FLOAT ms[16];
    FLOAT mr[16];
    MakeScale(s, ms);
    MakeRotationZ(r, mr);
    MulMat4(mr, ms, out);
}

static void DrawD3D11()
{
    RECT rc; GetClientRect(G.hwnd, &rc);
    D3D11_VIEWPORT vp = { .Width = (FLOAT)(rc.right - rc.left), .Height = (FLOAT)(rc.bottom - rc.top), .MaxDepth = 1.0f };
    G.d3dctx->RSSetViewports(1, &vp);
    
    FLOAT clear[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
    G.d3dctx->OMSetRenderTargets(1, &G.d3drtv, 0);
    G.d3dctx->ClearRenderTargetView(G.d3drtv, clear);

    G.d3dctx->VSSetShader(G.vs, 0, 0);
    G.d3dctx->PSSetShader(G.ps, 0, 0);
    G.d3dctx->VSSetConstantBuffers(0, 1, &G.cbuf);
    G.d3dctx->PSSetConstantBuffers(0, 1, &G.cbuf);

    G.d3dctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    G.d3dctx->IASetInputLayout(G.vlayout);
    G.d3dctx->IASetVertexBuffers(0, 1, &G.vbuf_quad, &G.vstride, &G.voffset);
    G.d3dctx->IASetIndexBuffer(G.ibuf_quad, DXGI_FORMAT_R16_UINT, 0);

    // Quad 1
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        HR(G.d3dctx->Map(G.cbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
        CBuffer* cb = (CBuffer*)mapped.pData;

        cb->color[0] = 1.0f;
        cb->color[1] = 0.0f;
        cb->color[2] = 0.0f;
        cb->color[3] = 1.0f;

        MakeTransform(0.0f, 1.0f, cb->rot);

        G.d3dctx->Unmap(G.cbuf, 0);
        G.d3dctx->DrawIndexed(G.ibuf_quad_count, 0, 0);
    }

    // Quad 2
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        HR(G.d3dctx->Map(G.cbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
        CBuffer* cb = (CBuffer*)mapped.pData;

        cb->color[0] = 0.0f;
        cb->color[1] = 1.0f;
        cb->color[2] = 0.0f;
        cb->color[3] = 1.0f;

        MakeTransform((FLOAT)GetTickCount64() / 1e3f, 0.5f, cb->rot);

        G.d3dctx->Unmap(G.cbuf, 0);
        G.d3dctx->DrawIndexed(G.ibuf_quad_count, 0, 0);
    }

    HR(G.d3dswp->Present(1, 0));

    SHORT key = GetAsyncKeyState('A');
    if ((key & 0x8000) && (key & 0x1)) {
        MessageBoxW(G.hwnd, L"This is an example dialog", L"d3d11-resize-draw", MB_OK);
    }
}

static void ResizeD3D11(LPARAM lparam)
{
    DestroyRenderTarget();
    HR(G.d3dswp->ResizeBuffers(0, LOWORD(lparam), HIWORD(lparam), DXGI_FORMAT_UNKNOWN, 0));
    CreateRenderTarget();
}

static void SpewMsg(const char* fmt, ...)
{
    char buf[512];
    va_list va;
    va_start(va, fmt);
    vsnprintf(buf, sizeof(buf), fmt, va);
    va_end(va);

    OutputDebugStringA(buf); OutputDebugStringA("\n");
}

static void SpewWndProc(HWND hwnd, UINT msg, WPARAM, LPARAM)
{
    switch (msg) {
    case WM_SIZE: {
        SpewMsg("WM_SIZE  hwnd=%p", hwnd);
    } break;
    case WM_PAINT: {
        SpewMsg("WM_PAINT hwnd=%p", hwnd);
    } break;
    }
}
#define SPEW_WNDPROC() SpewWndProc(hwnd, msg, wparam, lparam);

// --------------------------------------------------------------------------------
// Method 1: Don't do anything special. This is just to highlight the problem.
// --------------------------------------------------------------------------------
#if ACTIVE_METHOD == 1

static LRESULT WINAPI Method1_WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    SPEW_WNDPROC();
    switch (msg) {
    case WM_SIZE: {
        if (wparam != SIZE_MINIMIZED) {
            ResizeD3D11(lparam);
        }
        DrawD3D11();
        return 0;
    } break;
    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    } break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
    CreateDemoWindow(CS_CLASSDC, Method1_WndProc);
    SetupD3D11();
    CreateRenderTarget();
    ShowWindow(G.hwnd, SW_SHOW);

    BOOL running = TRUE;
    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) {
                running = FALSE;
            }
        }

        DrawD3D11();
    }
}

#endif // ACTIVE_METHOD == 1

// --------------------------------------------------------------------------------
// Method 2: Set CS_VREDRAW | CS_HREDRAW and render in WM_PAINT.
// --------------------------------------------------------------------------------
#if ACTIVE_METHOD == 2

static LRESULT WINAPI Method1_WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    SPEW_WNDPROC();
    switch (msg) {
    case WM_SIZE: {
        if (wparam != SIZE_MINIMIZED) {
            ResizeD3D11(lparam);
        }
        return 0;
    } break;
    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    } break;
    case WM_PAINT: {
        DrawD3D11();
        return 0;
    } break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
    CreateDemoWindow(CS_CLASSDC | CS_HREDRAW | CS_VREDRAW, Method1_WndProc);
    SetupD3D11();
    CreateRenderTarget();
    ShowWindow(G.hwnd, SW_SHOW);

    BOOL running = TRUE;
    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) {
                running = FALSE;
            }
        }
    }
}

#endif // ACTIVE_METHOD == 2

// --------------------------------------------------------------------------------
// Method 3: Render during resize with WM_TIMER.
// --------------------------------------------------------------------------------
#if ACTIVE_METHOD == 3

static LRESULT WINAPI Method1_WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    SPEW_WNDPROC();
    switch (msg) {
    case WM_SIZE: {
        if (wparam != SIZE_MINIMIZED) {
            ResizeD3D11(lparam);
        }
#if 1
        DrawD3D11();
#endif
        return 0;
    } break;
    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    } break;
    case WM_ENTERSIZEMOVE: {
        SetTimer(hwnd, 1, USER_TIMER_MINIMUM, 0);
    } break;
    case WM_EXITSIZEMOVE: {
        KillTimer(hwnd, 1);
    } break;
    case WM_TIMER: {
        DrawD3D11();
    } break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
    CreateDemoWindow(CS_CLASSDC, Method1_WndProc);
    SetupD3D11();
    CreateRenderTarget();
    ShowWindow(G.hwnd, SW_SHOW);

    BOOL running = TRUE;
    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) {
                running = FALSE;
            }
        }

        DrawD3D11();
    }
}

#endif // ACTIVE_METHOD == 3
