#include "MinHook.h"
#include "gui.h"
#include "patches.h"
#include "utils.h"
#include <minwindef.h>
#include <winnt.h>

void InitHooks();

int WINAPI main()
{
  RefreshIniValues();
  ApplyResPatch();

  while (!GetCurrentProcessWindow())
    Sleep(10);

  if (BORDERLESS_ENABLED)
    ApplyBorderlessPatch();
  ApplyIntroPatch();
  ApplyAutolootPatch();
  ApplyFramelockPatch();
  ApplyFovPatch();
  initialized = true;

  Sleep(7000);
  GetSwapChainPointers();
  InitHooks();
  InitKillsDeathsAddresses();

  return 0;
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpvReserved)
{

  if (fdwReason == DLL_PROCESS_ATTACH)
  {
    MH_Initialize();
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)main, NULL, 0, NULL);
    SetupD3DCompilerProxy();
  }
  else if (fdwReason == DLL_PROCESS_DETACH)
  {
    MH_Uninitialize();
    CleanupD3DCompilerProxy();
  }
  return TRUE;
}

void InitHooks()
{
  HMODULE hUser32 = GetModuleHandleA("user32.dll");
  LPVOID pSetCursorPos = (LPVOID)GetProcAddress(hUser32, "SetCursorPos");
  LPVOID pGetRawInputData = (LPVOID)GetProcAddress(hUser32, "GetRawInputData");

  if (MH_CreateHook(pSetCursorPos, (LPVOID)&HookedSetCursorPos, (LPVOID *)&g_OriginalSetCursorPos) == MH_OK)
    MH_EnableHook(pSetCursorPos);

  if (MH_CreateHook(pGetRawInputData, (LPVOID)&HookedGetRawInputData, (LPVOID *)&g_OriginalGetRawInputData) == MH_OK)
    MH_EnableHook(pGetRawInputData);

  if (pPresentTarget)
    if (MH_CreateHook((LPVOID)pPresentTarget, (LPVOID)&DetourPresent, (LPVOID *)&pPresent) == MH_OK)
      MH_EnableHook((LPVOID)pPresentTarget);

  if (pResizeBuffersTarget)
    if (MH_CreateHook((LPVOID)pResizeBuffersTarget, (LPVOID)&DetourResizeBuffers, (LPVOID *)&pResizeBuffers) == MH_OK)
      MH_EnableHook((LPVOID)pResizeBuffersTarget);
}