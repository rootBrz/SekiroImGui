#pragma once
#define MINI_CASE_SENSITIVE

#include "mini/ini.h"
#include <windows.h>

enum class LogLevel
{
  Info,
  Error,
  Success
};

HWND GetGameWindow();
bool IsValidReadPtr(uintptr_t addr, size_t size);
bool WriteProtectedMemory(uintptr_t address, const void *data, size_t size, const char *msg);
uintptr_t GetAbsoluteAddress(uintptr_t offset);
uintptr_t DereferenceStaticX64Pointer(uintptr_t instructionAddress, int instructionLength);
uintptr_t AllocateMemoryNearAddress(uintptr_t target, size_t size, bool executable = false);
uintptr_t InjectShellcodeWithJumpBack(uintptr_t targetAddr, const uint8_t *shellcode, size_t shellcodeSize, size_t patchSize, const char *msg);
std::vector<uint8_t> RelJmpBytes(int length, int32_t relative);
void RefreshIniValues();
void LogMessage(LogLevel level, bool showMessage, const char *err, ...);
void SetupD3DCompilerProxy();
bool ComputeExeMD5();

inline mINI::INIFile ini("sekiroimgui.ini");
inline mINI::INIStructure config;

template <typename T>
T GetIniValue(const char *str)
{
  if constexpr (std::is_same_v<T, int>)
    return std::stoi(config["Settings"][str]);
  else if constexpr (std::is_same_v<T, float>)
    return std::stof(config["Settings"][str]);
}

template <typename T>
void SetIniValue(const char *str, T value)
{
  config["Settings"][str] = std::to_string(value);
  ini.write(config);
}

// INI SETTINGS
inline bool FULLSCREEN_STATE;
inline int FPS_LIMIT;
inline int FOV_VALUE;
inline int CUSTOM_RES_ENABLED;
inline int CUSTOM_RES_WIDTH;
inline int CUSTOM_RES_HEIGHT;
inline int FPSUNLOCK_ENABLED;
inline int AUTOLOOT_ENABLED;
inline int INTROSKIP_ENABLED;
inline int BORDERLESS_ENABLED;
inline int FOV_ENABLED;
inline int SHOW_PLAYER_DEATHSKILLS_ENABLED;
inline int PLAYER_DEATHSKILLS_X;
inline int PLAYER_DEATHSKILLS_Y;
inline int PLAYER_DEATHSKILLS_FZ;
inline int TIMESCALE_ENABLED;
inline int PLAYER_TIMESCALE_ENABLED;
inline float TIMESCALE_VALUE;
inline float PLAYER_TIMESCALE_VALUE;
inline int DISABLE_CAMRESET_ENABLED;
inline int DISABLE_CAMERA_AUTOROTATE_ENABLED;
inline int VSYNC_ENABLED;
inline int DISABLE_DEATH_PENALTIES_ENABLED;
inline int DISABLE_DRAGONROT_ENABLED;
inline int UI_OPEN_KEY;
