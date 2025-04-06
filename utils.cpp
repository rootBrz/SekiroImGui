#include "utils.h"
#include "offset.h"
#include "patches.h"
#include <d3dcommon.h>

typedef enum D3D_BLOB_PART
{
  D3D_BLOB_INPUT_SIGNATURE_BLOB,
  D3D_BLOB_OUTPUT_SIGNATURE_BLOB,
  D3D_BLOB_INPUT_AND_OUTPUT_SIGNATURE_BLOB,
  D3D_BLOB_PATCH_CONSTANT_SIGNATURE_BLOB,
  D3D_BLOB_ALL_SIGNATURE_BLOB,
  D3D_BLOB_DEBUG_INFO,
  D3D_BLOB_LEGACY_SHADER,
  D3D_BLOB_XNA_PREPASS_SHADER,
  D3D_BLOB_XNA_SHADER,
  D3D_BLOB_PDB,
  D3D_BLOB_PRIVATE_DATA,
  D3D_BLOB_ROOT_SIGNATURE,
  D3D_BLOB_DEBUG_NAME,
  D3D_BLOB_TEST_ALTERNATE_SHADER = 0x8000,
  D3D_BLOB_TEST_COMPILE_DETAILS,
  D3D_BLOB_TEST_COMPILE_PERF,
  D3D_BLOB_TEST_COMPILE_REPORT,
} D3D_BLOB_PART;
typedef HRESULT(CALLBACK *D3DGETBLOBPART)(LPCVOID pSrcData, SIZE_T SrcDataSize, D3D_BLOB_PART Part, UINT Flags, ID3DBlob **ppPart);
D3DGETBLOBPART fpD3DGetBlobPart;
void SetupD3DCompilerProxy()
{
  UINT len = GetSystemDirectoryA(NULL, 0);
  char *syspath = new char[len + sizeof("\\D3DCompiler_43.dll")];
  GetSystemDirectoryA(syspath, len);
  strcat_s(syspath, len + sizeof("\\D3DCompiler_43.dll"), "\\D3DCompiler_43.dll");
  d3dHMOD = LoadLibraryA(syspath);
  if (d3dHMOD)
  {
    fpD3DGetBlobPart = (D3DGETBLOBPART)GetProcAddress(d3dHMOD, "D3DGetBlobPart");
  }

  delete[] syspath;
}
extern "C" __declspec(dllexport) HRESULT CALLBACK D3DGetBlobPart(LPCVOID pSrcData, SIZE_T SrcDataSize, D3D_BLOB_PART Part, UINT Flags, ID3DBlob **ppPart)
{
  return fpD3DGetBlobPart(pSrcData, SrcDataSize, Part, Flags, ppPart);
}
void CleanupD3DCompilerProxy()
{
  if (d3dHMOD)
  {
    FreeLibrary(d3dHMOD);
    d3dHMOD = nullptr;
  }
  fpD3DGetBlobPart = nullptr;
}

typedef struct
{
  const char *key;
  const char *value;
  int *var;
} Setting;
constexpr Setting defaultSettings[14] = {
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
    {"EnableShowPlayerDeathsKills", "0", &SHOW_PLAYER_DEATHSKILLS_ENABLED},
    {"PlayerDeathsKillsX", "100", &PLAYER_DEATHSKILLS_X},
    {"PlayerDeathsKillsY", "100", &PLAYER_DEATHSKILLS_Y},
    {"PlayerDeathsKillsFZ", "24", &PLAYER_DEATHSKILLS_FZ}};

// typedef DWORD64(CALLBACK *DIRECTINPUT8CREATE)(HINSTANCE, DWORD, REFIID,
//                                               LPVOID *, LPUNKNOWN);
// DIRECTINPUT8CREATE fpDirectInput8Create;
// void SetupD8Proxy()
// {
//   char syspath[320];
//   GetSystemDirectoryA(syspath, 320);
//   strcat_s(syspath, "\\dinput8.dll");
//   auto hMod = LoadLibraryA(syspath);
//   fpDirectInput8Create =
//       (DIRECTINPUT8CREATE)GetProcAddress(hMod, "DirectInput8Create");
// }
// extern "C" __declspec(dllexport) HRESULT CALLBACK
// DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf,
//                    LPVOID *ppvOut, LPUNKNOWN punkOuter)
// {
//   return fpDirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
// }

bool WriteProtectedMemory(uintptr_t address, const void *data, size_t size)
{
  SIZE_T bytesWritten = 0;
  BOOL result = WriteProcessMemory(GetCurrentProcess(), (LPVOID)address, data, size, &bytesWritten);

  if (!result || bytesWritten != size)
  {
    DWORD errCode = GetLastError();
    char buf[128];
    snprintf(buf, sizeof(buf), "WriteProcessMemory failed at 0x%p (error %lu)", (void *)address, errCode);
    ErrorLog(buf);
    return false;
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
  Sleep(8000);

  uintptr_t pDeathCount = GetAbsoluteAddress(player_deaths_offset + 29);
  uintptr_t pPlayerDeaths = DereferenceStaticX64Pointer(pDeathCount, 7);
  player_deaths_addr =
      *(uintptr_t *)pPlayerDeaths + *(int32_t *)(pDeathCount + 9);

  uintptr_t pKillCount = GetAbsoluteAddress(total_kills_offset) + 7;
  uintptr_t pPlayerKills = DereferenceStaticX64Pointer(pKillCount, 7);
  total_kills_addr = *(uintptr_t *)(*(uintptr_t *)pPlayerKills + 0x08) + 0xDC;
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
  bool configAvailable = (ini.read(config));

  for (const Setting &setting : defaultSettings)
  {
    if (!config["Settings"].has(setting.key))
    {
      config["Settings"][setting.key] = setting.value;
      if (configAvailable)
        ini.write(config);
    }

    *(int *)setting.var = GetIniValue(setting.key);
  }

  if (!configAvailable)
    ini.generate(config);

  config.remove("settings");
}

void ErrorLog(const char *err)
{
  MessageBoxA(nullptr, err, "Sekiro FPS Unlock ImGui", MB_OK | MB_ICONERROR);
}
