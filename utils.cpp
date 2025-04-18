#include "utils.h"
#include "md5c/md5.h"
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
    {"EnableVSync", "1", &VSYNC_ENABLED},
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
    {"DisableCamReset", "0", &DISABLE_CAMRESET_ENABLED},
    {"DisableCamAutorotate", "0", &DISABLE_CAMERA_AUTOROTATE_ENABLED},
};

bool WriteProtectedMemory(uintptr_t address, const void *data, size_t size, const char *msg)
{
  SIZE_T bytesWritten = 0;
  BOOL result = WriteProcessMemory(GetCurrentProcess(), (LPVOID)address, data, size, &bytesWritten);

  if (!result || bytesWritten != size)
  {
    DWORD errCode = GetLastError();
    char buf[128];
    snprintf(buf, sizeof(buf), "%s\nWriteProcessMemory failed at 0x%p (error %lu)", msg, (void *)address, errCode);

    return LogMessage(LogLevel::Error, true, buf), false;
  }

  LogMessage(LogLevel::Success, false, "%s.", msg);
  return true;
}

uintptr_t AllocateMemoryNearAddress(uintptr_t target, size_t size, bool executable)
{
  DWORD flProtect = executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;

  SYSTEM_INFO si;
  GetSystemInfo(&si);

  uintptr_t granularity = si.dwAllocationGranularity;
  uintptr_t minAddr = (target >= 0x70000000)
                          ? target - 0x70000000
                          : reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);
  uintptr_t maxAddr = target + 0x70000000;

  HANDLE process = GetCurrentProcess();
  MEMORY_BASIC_INFORMATION mbi;

  for (uintptr_t addr = target; addr >= minAddr; addr -= granularity)
  {
    if (VirtualQueryEx(process, (LPCVOID)addr, &mbi, sizeof(mbi)) != sizeof(mbi))
      continue;

    if (mbi.State == MEM_FREE && mbi.RegionSize >= size)
    {
      LPVOID allocated = VirtualAllocEx(process, (LPVOID)addr, size, MEM_COMMIT | MEM_RESERVE, flProtect);
      if (allocated)
        return reinterpret_cast<uintptr_t>(allocated);
    }
  }

  for (uintptr_t addr = target + granularity; addr < maxAddr; addr += granularity)
  {
    if (VirtualQueryEx(process, (LPCVOID)addr, &mbi, sizeof(mbi)) != sizeof(mbi))
      continue;

    if (mbi.State == MEM_FREE && mbi.RegionSize >= size)
    {
      LPVOID allocated = VirtualAllocEx(process, (LPVOID)addr, size, MEM_COMMIT | MEM_RESERVE, flProtect);
      if (allocated)
        return reinterpret_cast<uintptr_t>(allocated);
    }
  }

  LogMessage(LogLevel::Error, true, "Failed to allocate memory near target address 0x%p", (void *)target);
  return 0;
}

uintptr_t DereferenceStaticX64Pointer(uintptr_t instructionAddress, int instructionLength)
{
  return instructionAddress + *(int32_t *)(instructionAddress + (instructionLength - 0x04)) + instructionLength;
}

bool IsValidReadPtr(uintptr_t addr, size_t size)
{
  if (addr == 0 || size == 0)
    return false;

  if (addr + size < addr)
    return false;

  HANDLE hProcess = GetCurrentProcess();

  BYTE buffer[1];
  SIZE_T bytesRead;

  const char *startAddr = reinterpret_cast<const char *>(addr);
  const char *endAddr = startAddr + size - 1;

  if (!ReadProcessMemory(hProcess, startAddr, buffer, 1, &bytesRead) || bytesRead != 1)
    return false;

  if (!ReadProcessMemory(hProcess, endAddr, buffer, 1, &bytesRead) || bytesRead != 1)
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

std::vector<uint8_t> RelJmpBytes(int length, int32_t relative)
{
  std::vector<uint8_t> bytes(length);
  bytes[0] = 0xE9;
  memcpy(&bytes[1], &relative, sizeof(int32_t));
  for (int i = 5; i < length; i++)
    bytes[i] = 0x90;
  return bytes;
}

uintptr_t InjectShellcodeWithJumpBack(uintptr_t targetAddr, const uint8_t *shellcode, size_t shellcodeSize, size_t patchSize, const char *msg)
{
  uintptr_t codeCave = AllocateMemoryNearAddress(targetAddr, shellcodeSize + 5, true);
  uintptr_t returnAddr = targetAddr + patchSize;

  int32_t relJumpBack = (int32_t)(returnAddr - (codeCave + shellcodeSize + 5));

  std::vector<uint8_t> detour(shellcodeSize + 5);
  memcpy(detour.data(), shellcode, shellcodeSize);
  detour[shellcodeSize] = 0xE9;
  memcpy(detour.data() + shellcodeSize + 1, &relJumpBack, sizeof(relJumpBack));

  WriteProtectedMemory(codeCave, detour.data(), detour.size(), msg);
  return codeCave;
}

bool ComputeExeMD5()
{
  static const char *expectedHash = "0E0A8407C78E896A73D8F27DA3D4C0CC";
  char path[MAX_PATH], actualHash[33] = {0};
  uint8_t binaryHash[16];
  FILE *file;

  if (!GetModuleFileNameA(0, path, MAX_PATH) || fopen_s(&file, path, "rb") || !file)
    return LogMessage(LogLevel::Error, true, "MD5 check failed"), false;

  md5File(file, binaryHash);
  fclose(file);

  for (int i = 0; i < 16; i++)
    sprintf_s(actualHash + i * 2, 3, "%02X", binaryHash[i]);

  return !strcmp(actualHash, expectedHash) || (LogMessage(LogLevel::Error, true, "Hash mismatch\nCurrent: %s\nExpected: %s", actualHash, expectedHash), false);
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

void LogMessage(LogLevel level, bool showMessage, const char *err, ...)
{
  const char *levelStr = nullptr;
  switch (level)
  {
  case LogLevel::Info:
    levelStr = "[INFO]";
    break;
  case LogLevel::Error:
    levelStr = "[ERRO]";
    break;
  case LogLevel::Success:
    levelStr = "[SUCC]";
    break;
  }

  va_list ArgList;
  char message[1024];
  va_start(ArgList, err);
  vsnprintf(message, sizeof(message), err, ArgList);
  va_end(ArgList);

  FILE *logFile;
  fopen_s(&logFile, "sekirofpsimgui.log", "a");
  if (logFile)
  {
    time_t now = time(NULL);
    tm timeinfo;
    localtime_s(&timeinfo, &now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", &timeinfo);

    fprintf(logFile, "%s %s %s\n", timestamp, levelStr, message);
    fclose(logFile);
  }

  if (showMessage)
    MessageBoxA(nullptr, message, "Sekiro FPS Unlock ImGui", MB_OK | MB_ICONERROR);
}