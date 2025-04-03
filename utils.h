#pragma once
#include "mini/ini.h"
#include <cmath>
#include <cstdio>
#include <windows.h>

void SetupD8Proxy();
bool WriteProtectedMemory(uintptr_t address, const void *data, size_t size);
uintptr_t GetAbsoluteAddress(uintptr_t offset);
void GetCurrentProcessWindow(HWND *hWnd);
int GetIniValue(const char *str);
void SetIniValue(const char *str, int value);
void InitKillsDeathsAddresses();
uintptr_t DereferenceStaticX64Pointer(uintptr_t instructionAddress,
                                      int instructionLength);
uintptr_t AllocateMemoryNearAddress(uintptr_t target, size_t size);
void RefreshIniValues();

inline mINI::INIFile ini("sekiroimgui.ini");
inline mINI::INIStructure config;