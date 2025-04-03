#include "utils.h"
#include "offset.h"
#include "patches.h"

typedef DWORD64(CALLBACK *DIRECTINPUT8CREATE)(HINSTANCE, DWORD, REFIID,
                                              LPVOID *, LPUNKNOWN);
DIRECTINPUT8CREATE fpDirectInput8Create;
void SetupD8Proxy()
{
  char syspath[320];
  GetSystemDirectoryA(syspath, 320);
  strcat_s(syspath, "\\dinput8.dll");
  auto hMod = LoadLibraryA(syspath);
  fpDirectInput8Create =
      (DIRECTINPUT8CREATE)GetProcAddress(hMod, "DirectInput8Create");
}
extern "C" __declspec(dllexport) HRESULT CALLBACK
DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf,
                   LPVOID *ppvOut, LPUNKNOWN punkOuter)
{
  return fpDirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
}

bool WriteProtectedMemory(uintptr_t address, const void *data, size_t size)
{
  size_t writtenSize = WriteProcessMemory(GetCurrentProcess(), (LPVOID)address, data, size, NULL);
  return (writtenSize = size);
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

uintptr_t DereferenceStaticX64Pointer(uintptr_t instructionAddress,
                                      int instructionLength)
{

  return instructionAddress +
         *(int32_t *)(instructionAddress + (instructionLength - 0x04)) +
         instructionLength;

  ;
}

uintptr_t GetAbsoluteAddress(uintptr_t offset)
{
  HMODULE hGameExe = GetModuleHandle(nullptr);
  uintptr_t base = reinterpret_cast<uintptr_t>(hGameExe);
  uintptr_t absoluteAddress = base + offset;

  if (absoluteAddress < base)
  {
    return 0;
  }

  return absoluteAddress;
}

void InitKillsDeathsAddresses()
{
  Sleep(15000);

  uintptr_t pDeathCount = GetAbsoluteAddress(player_deaths_offset + 29);
  uintptr_t pPlayerDeaths = DereferenceStaticX64Pointer(pDeathCount, 7);
  player_deaths_addr =
      *(uintptr_t *)pPlayerDeaths + *(int32_t *)(pDeathCount + 9);

  uintptr_t pKillCount = GetAbsoluteAddress(total_kills_offset) + 7;
  uintptr_t pPlayerKills = DereferenceStaticX64Pointer(pKillCount, 7);
  total_kills_addr = *(uintptr_t *)(*(uintptr_t *)pPlayerKills + 0x08) + 0xDC;
}

void GetCurrentProcessWindow(HWND *hWnd)
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

  *hWnd = currentHwnd;
}

int GetIniValue(const char *str)
{
  std::string &setting = config["Settings"][str];

  return std::stoi(setting);
}

void SetIniValue(const char *str, int value)
{
  config["Settings"][str] = std::to_string(value);
  ini.write(config);
}

void RefreshIniValues()
{
  if (!ini.read(config))
  {
    config["Settings"]["EnableFpsUnlock"] = "1";
    config["Settings"]["FpsLimit"] = "165";
    config["Settings"]["EnableFOV"] = "0";
    config["Settings"]["ValueFOV"] = "0";
    config["Settings"]["EnableCustomRes"] = "0";
    config["Settings"]["CustomResWidth"] = "2560";
    config["Settings"]["CustomResHeight"] = "1080";
    config["Settings"]["EnableAutoloot"] = "1";
    config["Settings"]["EnableIntroSkip"] = "1";
    config["Settings"]["EnableBorderlessFullscreen"] = "1";
    config["Settings"]["EnableShowPlayerDeathsKills"] = "0";
    ini.generate(config);
  };

  FPSUNLOCK_ENABLED = GetIniValue("EnableFpsUnlock");
  FPS_LIMIT = GetIniValue("FpsLimit");
  FOV_ENABLED = GetIniValue("EnableFOV");
  FOV_VALUE = GetIniValue("ValueFOV");
  CUSTOM_RES_ENABLED = GetIniValue("EnableCustomRes");
  CUSTOM_RES_WIDTH = GetIniValue("CustomResWidth");
  CUSTOM_RES_HEIGHT = GetIniValue("CustomResHeight");
  AUTOLOOT_ENABLED = GetIniValue("EnableAutoloot");
  INTROSKIP_ENABLED = GetIniValue("EnableIntroSkip");
  BORDERLESS_ENABLED = GetIniValue("EnableBorderlessFullscreen");
  SHOW_PLAYER_DEATHSKILLS_ENABLED = GetIniValue("EnableShowPlayerDeathsKills");
}
