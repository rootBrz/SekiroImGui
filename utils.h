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

HWND GetCurrentProcessWindow();
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
