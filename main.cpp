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
  MH_STATUS status = MH_Initialize();

  while (!GetPresentPointer())
  {
    Sleep(10);
  };

  if (MH_CreateHook(reinterpret_cast<void **>(pPresentTarget),
                    reinterpret_cast<LPVOID>(&DetourPresent),
                    reinterpret_cast<void **>(&pPresent)) == MH_OK)
  {
    MH_EnableHook(reinterpret_cast<LPVOID>(pPresentTarget));
  }

  LPVOID pSetCursorPos = (LPVOID)GetProcAddress(hUser32, "SetCursorPos");
  if (pSetCursorPos && MH_CreateHook(pSetCursorPos, (LPVOID)&HookedSetCursorPos,
                                     reinterpret_cast<LPVOID *>(
                                         &g_OriginalSetCursorPos)) == MH_OK)
  {
    MH_EnableHook(pSetCursorPos);
  }

  LPVOID pGetRawInputData = (LPVOID)GetProcAddress(hUser32, "GetRawInputData");
  if (pGetRawInputData &&
      MH_CreateHook(pGetRawInputData, (LPVOID)&HookedGetRawInputData,
                    reinterpret_cast<LPVOID *>(&g_OriginalGetRawInputData)) ==
          MH_OK)
  {
    MH_EnableHook(pGetRawInputData);
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
