#include "utils.h"
#include "offset.h"
#include "patches.h"

typedef HRESULT(WINAPI *D3DGETBLOBPART)(LPCVOID pSrcData, SIZE_T SrcDataSize, UINT Part, UINT Flags, void **ppPart);
HMODULE d3dHMOD = nullptr;
D3DGETBLOBPART fpD3DGetBlobPart = nullptr;
void SetupD3DCompilerProxy()
{
  char syspath[MAX_PATH];
  if (GetSystemDirectoryA(syspath, MAX_PATH))
  {
    strcat_s(syspath, "\\D3DCompiler_43.dll");
    if ((d3dHMOD = LoadLibraryA(syspath)))
    {
      fpD3DGetBlobPart = (D3DGETBLOBPART)GetProcAddress(d3dHMOD, "D3DGetBlobPart");
    }
  }
}
extern "C" __declspec(dllexport) HRESULT WINAPI D3DGetBlobPart(LPCVOID pSrcData, SIZE_T SrcDataSize, UINT Part, UINT Flags, void **ppPart)
{
  return fpD3DGetBlobPart(pSrcData, SrcDataSize, Part, Flags, ppPart);
}

typedef struct
{
  const char *key;
  const char *value;
  void *var;
  bool isFloat = false;
} Setting;
constexpr Setting defaultSettings[] = {
    {"EnableFpsUnlock", "1", &FPSUNLOCK_ENABLED},
    {"FpsLimit", "165", &FPS_LIMIT},
    {"EnableFOV", "0", &FOV_ENABLED},
    {"ValueFOV", "0", &FOV_VALUE},
    {"EnableCustomRes", "0", &CUSTOM_RES_ENABLED},
    {"CustomResWidth", "2560", &CUSTOM_RES_WIDTH},
    {"CustomResHeight", "1080", &CUSTOM_RES_HEIGHT},
    {"EnableAutoloot", "1", &AUTOLOOT_ENABLED},
    {"EnableIntroSkip", "1", &INTROSKIP_ENABLED},
    {"EnableBorderlessFullscreen", "1", &BORDERLESS_ENABLED},
    {"EnableTimescale", "0", &TIMESCALE_ENABLED},
    {"ValueTimescale", "1.0", &TIMESCALE_VALUE, true},
    {"EnablePlayerTimescale", "0", &PLAYER_TIMESCALE_ENABLED},
    {"ValuePlayerTimescale", "1.0", &PLAYER_TIMESCALE_VALUE, true},
    {"EnableShowPlayerDeathsKills", "0", &SHOW_PLAYER_DEATHSKILLS_ENABLED},
    {"PlayerDeathsKillsX", "100", &PLAYER_DEATHSKILLS_X},
    {"PlayerDeathsKillsY", "100", &PLAYER_DEATHSKILLS_Y},
    {"PlayerDeathsKillsFZ", "24", &PLAYER_DEATHSKILLS_FZ},
};

bool WriteProtectedMemory(uintptr_t address, const void *data, size_t size, const char *str)
{
  SIZE_T bytesWritten = 0;
  BOOL result = WriteProcessMemory(GetCurrentProcess(), (LPVOID)address, data, size, &bytesWritten);

  if (!result || bytesWritten != size)
  {
    DWORD errCode = GetLastError();
    char buf[128];
    snprintf(buf, sizeof(buf), "%s\nWriteProcessMemory failed at 0x%p (error %lu)", str, (void *)address, errCode);

    return ErrorLog(true, buf), false;
  }

  return true;
}

uintptr_t AllocateMemoryNearAddress(uintptr_t target, size_t size)
{
  SYSTEM_INFO si;
  GetSystemInfo(&si);

  uintptr_t minAddr = (target > 0x70000000)
                          ? target - 0x70000000
                          : (uintptr_t)si.lpMinimumApplicationAddress;
  uintptr_t maxAddr = target + 0x70000000;
  MEMORY_BASIC_INFORMATION mbi;

  while (minAddr < maxAddr)
  {
    if (VirtualQueryEx(GetCurrentProcess(), (LPCVOID)minAddr, &mbi,
                       sizeof(mbi)) != sizeof(mbi))
      break;

    if (mbi.State == MEM_FREE && mbi.RegionSize >= size)
    {
      LPVOID allocated =
          VirtualAllocEx(GetCurrentProcess(), (LPVOID)minAddr, size,
                         MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
      if (allocated)
      {
        return reinterpret_cast<uintptr_t>(allocated);
      }
    }
    minAddr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
  }
  return 0;
}

uintptr_t DereferenceStaticX64Pointer(uintptr_t instructionAddress, int instructionLength)
{
  return instructionAddress + *(int32_t *)(instructionAddress + (instructionLength - 0x04)) + instructionLength;
}

bool IsValidReadPtr(uintptr_t ptr, size_t size)
{
  if (!ptr)
    return false;

  if (size == 0)
    return false;

  unsigned char buffer[size];
  SIZE_T bytesRead = 0;
  BOOL result = ReadProcessMemory(GetCurrentProcess(), (LPCVOID)ptr, buffer, size, &bytesRead);

  if (!result || bytesRead != size)
    return false;

  return true;
}

uintptr_t GetAbsoluteAddress(uintptr_t offset)
{
  HMODULE hGameExe = GetModuleHandle(nullptr);
  uintptr_t base = reinterpret_cast<uintptr_t>(hGameExe);
  uintptr_t absoluteAddress = base + offset;

  if (absoluteAddress < base)
    return 0;

  return absoluteAddress;
}

bool InitKillsDeathsAddresses()
{
  // Deaths
  uintptr_t pDeathOffset = GetAbsoluteAddress(player_deaths_offset + 29);
  if (!IsValidReadPtr(pDeathOffset, sizeof(uintptr_t)))
    return false;
  uintptr_t pDeathOffset2 = DereferenceStaticX64Pointer(pDeathOffset, 7);
  if (!IsValidReadPtr(pDeathOffset2, sizeof(uintptr_t)))
    return false;
  uintptr_t pPlayerDeaths = *(uintptr_t *)pDeathOffset2 + *(int32_t *)(pDeathOffset + 9);
  if (!IsValidReadPtr(pPlayerDeaths, sizeof(uintptr_t)))
    return false;

  // Kills
  uintptr_t pKillsOffset = GetAbsoluteAddress(total_kills_offset) + 7;
  if (!IsValidReadPtr(pKillsOffset, sizeof(uintptr_t)))
    return false;
  uintptr_t pKillsOffset2 = DereferenceStaticX64Pointer(pKillsOffset, 7);
  if (!IsValidReadPtr(pKillsOffset2, sizeof(uintptr_t)))
    return false;
  uintptr_t pTotalKills = *(uintptr_t *)(*(uintptr_t *)pKillsOffset2 + 0x08) + 0xDC;
  if (!IsValidReadPtr(pTotalKills, sizeof(uintptr_t)))
    return false;

  // Final Addresses
  player_deaths_addr = pPlayerDeaths;
  total_kills_addr = pTotalKills;

  return true;
}

HWND GetCurrentProcessWindow()
{
  HWND currentHwnd = NULL;
  DWORD currentPID = GetCurrentProcessId();

  EnumWindows(
      [](HWND hwnd, LPARAM lParam) -> BOOL
      {
        DWORD pid;
        HWND *resultPtr = (HWND *)lParam;

        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == GetCurrentProcessId() && !GetWindow(hwnd, GW_OWNER) &&
            IsWindowVisible(hwnd))
        {
          *resultPtr = hwnd;
          return FALSE;
        }
        return TRUE;
      },
      (LPARAM)&currentHwnd);

  return currentHwnd;
}

void RefreshIniValues()
{
  // Generate INI if not present
  if (!ini.read(config))
    ini.generate(config);

  // Write missing settings if INI is present
  // Retrieve setting values
  for (const Setting &setting : defaultSettings)
  {
    if (!config["Settings"].has(setting.key))
    {
      config["Settings"][setting.key] = setting.value;
      ini.write(config);
    }

    if (setting.isFloat)
      *(float *)setting.var = GetIniValue<float>(setting.key);
    else
      *(int *)setting.var = GetIniValue<int>(setting.key);
  }

  // Removes old section from 1.0 version
  config.remove("settings");
}

void ErrorLog(bool showMessage, const char *err, ...)
{
  va_list ArgList;
  char buffer[1024];
  va_start(ArgList, err);
  vsnprintf(buffer, sizeof(buffer), err, ArgList);
  va_end(ArgList);

  FILE *logFile = fopen("sekirofpsimgui_log.txt", "a");
  if (logFile)
  {
    time_t now = time(NULL);
    tm *timeinfo = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S] ", timeinfo);

    fprintf(logFile, "%s%s\n", timestamp, buffer);
    fclose(logFile);
  }

  if (showMessage)
    MessageBoxA(nullptr, buffer, "Sekiro FPS Unlock ImGui", MB_OK | MB_ICONERROR);
}