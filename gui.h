#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "minhook.x64d.lib")

typedef BOOL(WINAPI *SetCursorPosFn)(int X, int Y);
typedef UINT(WINAPI *GetRawInputDataFn)(HRAWINPUT hRawInput, UINT uiCommand,
                                        LPVOID pData, PUINT pcbSize,
                                        UINT cbSizeHeader);
typedef long(CALLBACK *present)(IDXGISwapChain *, UINT, UINT);

inline SetCursorPosFn g_OriginalSetCursorPos;
inline GetRawInputDataFn g_OriginalGetRawInputData;
inline present pPresent;
inline present pPresentTarget;
inline WNDPROC oWndProc;

bool GetSwapChainPointers();
typedef long(CALLBACK *resizeBuffers)(IDXGISwapChain *, UINT, UINT, UINT, DXGI_FORMAT, UINT);
inline resizeBuffers pResizeBuffersTarget;
inline resizeBuffers pResizeBuffers;
long CALLBACK DetourResizeBuffers(
    IDXGISwapChain *pSwapChain,
    UINT BufferCount,
    UINT Width,
    UINT Height,
    DXGI_FORMAT NewFormat,
    UINT SwapChainFlags);

long CALLBACK DetourPresent(IDXGISwapChain *pSwapchain, UINT syncInterval,
                            UINT flags);
BOOL WINAPI HookedSetCursorPos(int X, int Y);
UINT WINAPI HookedGetRawInputData(HRAWINPUT hRawInput, UINT uiCommand,
                                  LPVOID pData, PUINT pcbSize,
                                  UINT cbSizeHeader);
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

inline bool SHOW_IMGUI = false;
