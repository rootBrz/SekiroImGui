#pragma once

#include <d3d11.h>
#include <windows.h>

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "minhook.x64.lib")

bool GetSwapChainPointers();
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

typedef BOOL(WINAPI *setCursorPos)(int X, int Y);
typedef UINT(WINAPI *getRawInputData)(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader);
typedef long(WINAPI *changeDisplaySettingsExW)(LPCWSTR lpszDeviceName, DEVMODEW *lpDevMode, HWND hwnd, DWORD dwflags, LPVOID lParam);
typedef long(CALLBACK *present)(IDXGISwapChain *pSwapChain, UINT syncInterval, UINT flags);
typedef long(CALLBACK *resizeBuffers)(IDXGISwapChain *pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);

inline bool SHOW_IMGUI = false;
inline WNDPROC oWndProc;
inline ID3D11Device *pDevice = nullptr;
inline ID3D11DeviceContext *pContext = nullptr;
inline ID3D11RenderTargetView *mainRenderTargetView = nullptr;
inline setCursorPos fnSetCursorPos;
inline getRawInputData fnGetRawInputData;
inline changeDisplaySettingsExW fnChangeDisplaySettingsExW;
inline present pPresent;
inline present fnPresent;
inline resizeBuffers pResizeBuffers;
inline resizeBuffers fnResizeBuffers;

BOOL WINAPI DetourSetCursorPos(int X, int Y);
UINT WINAPI DetourGetRawInputData(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader);
long WINAPI DetourChangeDisplaySettingsExW(LPCWSTR lpszDeviceName, DEVMODEW *lpDevMode, HWND hwnd, DWORD dwflags, LPVOID lParam);
long CALLBACK DetourPresent(IDXGISwapChain *pSwapChain, UINT syncInterval, UINT flags);
long CALLBACK DetourResizeBuffers(IDXGISwapChain *pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);