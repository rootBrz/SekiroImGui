#include "MinHook.h"
#include "gui.h"
#include "md5c/md5.h"
#include "patches.h"
#include "utils.h"

void InitHooks()
{
  HMODULE hUser32 = GetModuleHandleA("user32.dll");
  LPVOID pSetCursorPos = (LPVOID)GetProcAddress(hUser32, "SetCursorPos");
  LPVOID pGetRawInputData = (LPVOID)GetProcAddress(hUser32, "GetRawInputData");

  if (MH_CreateHook(pSetCursorPos, (LPVOID)&HookedSetCursorPos, (LPVOID *)&g_OriginalSetCursorPos) == MH_OK)
    if (MH_EnableHook(pSetCursorPos) != MH_OK)
      ErrorLog(false, "SetCursorPos hook failed");

  if (MH_CreateHook(pGetRawInputData, (LPVOID)&HookedGetRawInputData, (LPVOID *)&g_OriginalGetRawInputData) == MH_OK)
    if (MH_EnableHook(pGetRawInputData))
      ErrorLog(false, "GetRawInputData hook failed");

  if (pPresentTarget)
    if (MH_CreateHook((LPVOID)pPresentTarget, (LPVOID)&DetourPresent, (LPVOID *)&pPresent) == MH_OK)
      if (MH_EnableHook((LPVOID)pPresentTarget))
        ErrorLog(false, "PresentTarget hook failed");

  if (pResizeBuffersTarget)
    if (MH_CreateHook((LPVOID)pResizeBuffersTarget, (LPVOID)&DetourResizeBuffers, (LPVOID *)&pResizeBuffers) == MH_OK)
      if (MH_EnableHook((LPVOID)pResizeBuffersTarget))
        ErrorLog(false, "ResizeBuffersTarget hook failed");
}

bool ComputeExeMD5()
{
  static const char *expectedHash = "0E0A8407C78E896A73D8F27DA3D4C0CC";
  char path[MAX_PATH], actualHash[33] = {0};
  uint8_t binaryHash[16];
  FILE *file;

  if (!GetModuleFileNameA(0, path, MAX_PATH) || fopen_s(&file, path, "rb") || !file)
    return ErrorLog(true, "MD5 check failed"), false;

  md5File(file, binaryHash);
  fclose(file);

  for (int i = 0; i < 16; i++)
    sprintf(actualHash + i * 2, "%02X", binaryHash[i]);

  return !strcmp(actualHash, expectedHash) || (ErrorLog(true, "Hash mismatch\nCurrent: %s\nExpected: %s", actualHash, expectedHash), false);
}

DWORD WINAPI FreezeThread(LPVOID lpParam)
{
  while (!INITIALIZED)
    Sleep(10);

  while (INITIALIZED)
  {
    ApplyPlayerTimescalePatch();
    Sleep(100);
  }

  return true;
}

DWORD WINAPI InitThread(LPVOID lpParam)
{
  SetupD3DCompilerProxy();

  if (!ComputeExeMD5())
    return false;

  remove("sekirofpsimgui_log.txt");
  ErrorLog(false, "Sekiro FPS ImGui log started");

  MH_Initialize();
  RefreshIniValues();
  ApplyResPatch();

  while (!GetCurrentProcessWindow())
    Sleep(10);

  ApplyBorderlessPatch();
  ApplyIntroPatch();
  ApplyAutolootPatch();
  ApplyFramelockPatch();
  ApplyFovPatch();
  while (!ApplyTimescalePatch())
    Sleep(10);
  while (!ApplyPlayerTimescalePatch())
    Sleep(10);
  INITIALIZED = true;

  while (!InitKillsDeathsAddresses())
    Sleep(10);

  Sleep(3000);
  GetSwapChainPointers();
  InitHooks();

  return true;
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpvReserved)
{

  if (fdwReason == DLL_PROCESS_ATTACH)
  {
    DisableThreadLibraryCalls(hModule);
    CreateThread(NULL, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(InitThread), NULL, 0, NULL);
    CreateThread(NULL, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(FreezeThread), NULL, 0, NULL);
  }

  return true;
}
