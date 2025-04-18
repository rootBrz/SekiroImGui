#include "MinHook.h"
#include "gui.h"
#include "offset.h"
#include "patches.h"
#include "utils.h"
#include "version.h"

void InitUser32Hooks()
{
  HMODULE hUser32 = GetModuleHandleA("user32.dll");
  LPVOID pSetCursorPos = (LPVOID)GetProcAddress(hUser32, "SetCursorPos");
  LPVOID pGetRawInputData = (LPVOID)GetProcAddress(hUser32, "GetRawInputData");
  LPVOID pChangeDisplaySettingsExW = (LPVOID)GetProcAddress(hUser32, "ChangeDisplaySettingsExW");

  if (MH_CreateHook(pSetCursorPos, (LPVOID)&DetourSetCursorPos, (LPVOID *)&fnSetCursorPos) == MH_OK)
    if (MH_EnableHook(pSetCursorPos) != MH_OK)
      LogMessage(LogLevel::Error, false, "SetCursorPos hook failed");

  if (MH_CreateHook(pGetRawInputData, (LPVOID)&DetourGetRawInputData, (LPVOID *)&fnGetRawInputData) == MH_OK)
    if (MH_EnableHook(pGetRawInputData) != MH_OK)
      LogMessage(LogLevel::Error, false, "GetRawInputData hook failed");

  if (MH_CreateHook(pChangeDisplaySettingsExW, (LPVOID)&DetourChangeDisplaySettingsExW, (LPVOID *)&fnChangeDisplaySettingsExW) == MH_OK)
    if (MH_EnableHook(pChangeDisplaySettingsExW) != MH_OK)
      LogMessage(LogLevel::Error, false, "ChangeDisplaySettingsExW hook failed");
}

void InitSwapchainHooks()
{
  if (pPresent)
    if (MH_CreateHook((LPVOID)pPresent, (LPVOID)&DetourPresent, (LPVOID *)&fnPresent) == MH_OK)
      if (MH_EnableHook((LPVOID)pPresent) != MH_OK)
        LogMessage(LogLevel::Error, false, "Present hook failed");

  if (pResizeBuffers)
    if (MH_CreateHook((LPVOID)pResizeBuffers, (LPVOID)&DetourResizeBuffers, (LPVOID *)&fnResizeBuffers) == MH_OK)
      if (MH_EnableHook((LPVOID)pResizeBuffers) != MH_OK)
        LogMessage(LogLevel::Error, false, "ResizeBuffers hook failed");
}

void Shutdown()
{
  INITIALIZED = false;
  MH_DisableHook(MH_ALL_HOOKS);
  MH_Uninitialize();
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  if (mainRenderTargetView)
  {
    mainRenderTargetView->Release();
    mainRenderTargetView = NULL;
  }
  if (pContext)
  {
    pContext->Release();
    pContext = NULL;
  }
  if (pDevice)
  {
    pDevice->Release();
    pDevice = NULL;
  }

  SetWindowLongPtr(GetCurrentProcessWindow(), GWLP_WNDPROC, (LONG_PTR)(oWndProc));

  LogMessage(LogLevel::Info, false, "Shutdown initiated.");
  Sleep(100);

  // Necessary because in pair with SLE game hungs on exit, don't see any cons
  ExitProcess(0);
}

DWORD WINAPI InitThread(LPVOID lpParam)
{
  SetupD3DCompilerProxy();

  if (!ComputeExeMD5())
    return false;

  remove("sekirofpsimgui.log");
  LogMessage(LogLevel::Info, false, "Sekiro FPS Unlock ImGui v%s log initialized.", PROJECT_VERSION_STRING);

  MH_Initialize();
  RefreshIniValues();
  ApplyResPatch();

  while (!GetCurrentProcessWindow())
    Sleep(10);

  InitUser32Hooks();

  ApplyFullscreenHZPatch();
  is_game_in_loading_addr = GetAbsoluteAddress(is_game_in_loading_offset);
  ApplyBorderlessPatch();
  ApplyIntroPatch();
  ApplyCamResetPatch();
  ApplyCamAutorotatePatch();
  ApplyAutolootPatch();
  ApplyFramelockPatch();
  ApplyFovPatch();
  while (!ApplyTimescalePatch())
    Sleep(10);
  INITIALIZED = true;

  GetSwapChainPointers();
  InitSwapchainHooks();

  while (true)
  {
    HWND hWnd = GetCurrentProcessWindow();
    if (!IsWindow(hWnd))
      break;

    DWORD exitCode = 0;
    GetExitCodeProcess(GetCurrentProcess(), &exitCode);
    if (exitCode != STILL_ACTIVE)
      break;

    Sleep(500);
  }

  Shutdown();

  return true;
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpvReserved)
{

  if (fdwReason == DLL_PROCESS_ATTACH)
  {
    DisableThreadLibraryCalls(hModule);
    CreateThread(NULL, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(InitThread), NULL, 0, NULL);
  }

  return true;
}
