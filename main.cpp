#include "MinHook.h"
#include "gui.h"
#include "patches.h"
#include "utils.h"

int WINAPI main()
{
  HWND hWnd;
  while (!hWnd)
  {
    Sleep(10);
    GetCurrentProcessWindow(&hWnd);
  }

  RefreshIniValues();
  ApplyResPatch();
  if (BORDERLESS_ENABLED)
    ApplyBorderlessPatch();
  ApplyIntroPatch();
  ApplyAutolootPatch();
  ApplyFramelockPatch();
  ApplyFovPatch();

  HMODULE hUser32 = GetModuleHandleA("user32.dll");
  LPVOID pSetCursorPos = (LPVOID)GetProcAddress(hUser32, "SetCursorPos");
  LPVOID pGetRawInputData = (LPVOID)GetProcAddress(hUser32, "GetRawInputData");

  while (!GetSwapChainPointers())
  {
    Sleep(10);
  }

  if (MH_Initialize() == MH_OK)
  {

    MH_CreateHook(reinterpret_cast<void **>(pPresentTarget), reinterpret_cast<LPVOID>(&DetourPresent), reinterpret_cast<void **>(&pPresent));
    MH_CreateHook(reinterpret_cast<void *>(pResizeBuffersTarget), reinterpret_cast<LPVOID>(&DetourResizeBuffers), reinterpret_cast<void **>(&pResizeBuffers));
    MH_CreateHook(pSetCursorPos, (LPVOID)&HookedSetCursorPos, reinterpret_cast<LPVOID *>(g_OriginalSetCursorPos));
    MH_CreateHook(pGetRawInputData, (LPVOID)&HookedGetRawInputData, reinterpret_cast<LPVOID *>(&g_OriginalGetRawInputData));

    MH_EnableHook(MH_ALL_HOOKS);
  }

  InitKillsDeathsAddresses();

  return 0;
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpvReserved)
{

  if (fdwReason == DLL_PROCESS_ATTACH)
  {
    SetupD8Proxy();
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)main, NULL, 0, NULL);
  }
  return TRUE;
}
