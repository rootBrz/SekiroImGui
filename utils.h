#pragma once
#define MINI_CASE_SENSITIVE

#include "mini/ini.h"
#include <cstdio>
#include <windows.h>

// void SetupD8Proxy();
inline HMODULE d3dHMOD = nullptr;
void SetupD3DCompilerProxy();
void CleanupD3DCompilerProxy();

bool WriteProtectedMemory(uintptr_t address, const void *data, size_t size);
uintptr_t GetAbsoluteAddress(uintptr_t offset);
HWND GetCurrentProcessWindow();
int GetIniValue(const char *str);
void SetIniValue(const char *str, int value);
void InitKillsDeathsAddresses();
uintptr_t DereferenceStaticX64Pointer(uintptr_t instructionAddress,
                                      int instructionLength);
uintptr_t AllocateMemoryNearAddress(uintptr_t target, size_t size);
void RefreshIniValues();
void ErrorLog(const char *err);

inline mINI::INIFile ini("sekiroimgui.ini");
inline mINI::INIStructure config;