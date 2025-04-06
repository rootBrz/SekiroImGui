#include "patches.h"
#include "offset.h"
#include "utils.h"
#include <cstdint>
#include <synchapi.h>
#include <winuser.h>

uintptr_t fovData;
uintptr_t speedData;
constexpr double PI = 3.14159265358979323846;

// typedef struct
// {
//   int minWidth;
//   uintptr_t offset;
//   uint8_t disable[8];
//   uintptr_t address() const
//   {
//     return GetAbsoluteAddress(offset);
//   }
// } ResPatch;

// constexpr static ResPatch resPatch[4]{
//     {3840, resolution_default_3840_offset, {0x00, 0x0F, 0x00, 0x00, 0x70, 0x08, 0x00, 0x00}},
//     {2560, resolution_default_2560_offset, {0x00, 0x0A, 0x00, 0x00, 0xA0, 0x05, 0x00, 0x00}},
//     {1920, resolution_default_1920_offset, {0x80, 0x07, 0x00, 0x00, 0x38, 0x04, 0x00, 0x00}},
//     {1280, resolution_default_1280_offset, {0x00, 0x05, 0x00, 0x00, 0xD0, 0x02, 0x00, 0x00}},
//     {840, resolution_default_840_offset, {0x48, 0x03, 0x00, 0x00, 0xC2, 0x01, 0x00, 0x00}}
// };

constexpr static const float speedFixMatrix[]{
    15.0f, 16.0f, 16.6667f, 18.0f, 18.6875f, 18.8516f, 20.0f,
    24.0f, 25.0f, 27.5f, 30.0f, 32.0f, 38.5f, 40.0f,
    48.0f, 49.5f, 50.0f, 57.2958f, 60.0f, 64.0f, 66.75f,
    67.0f, 78.8438f, 80.0f, 84.0f, 90.0f, 93.8f, 100.0f,
    120.0f, 127.0f, 128.0f, 130.0f, 140.0f, 144.0f, 150.0f};

void ApplySpeedFix(float FPS)
{
  speedfix_addr =
      speedfix_addr ? speedfix_addr : GetAbsoluteAddress(speedfix_offset + 15);
  speedData = speedData
                  ? speedData
                  : AllocateMemoryNearAddress(speedfix_addr, sizeof(float));

  float idealSpeedFix = FPS / 2.0f;
  float closestSpeedFix = 30.0f; // Default value for 60 fps
  for (size_t i = 0; i < sizeof(speedFixMatrix) / sizeof(speedFixMatrix[0]);
       i++)
  {
    float speedFix = speedFixMatrix[i];
    if (abs(idealSpeedFix - speedFix) < abs(idealSpeedFix - closestSpeedFix))
    {
      closestSpeedFix = speedFix;
    }
  }

  if (!WriteProtectedMemory((uintptr_t)speedData, &closestSpeedFix, sizeof(float)))
  {
    ErrorLog("Failed to write speed value");
    return;
  }

  uintptr_t nextAddress = speedfix_addr + sizeof(int32_t);
  int32_t relative = static_cast<int32_t>((uintptr_t)speedData - nextAddress);

  if (!WriteProtectedMemory((uintptr_t)speedfix_addr, &relative, sizeof(relative)))
  {
    ErrorLog("Failed to patch speed fix");
    return;
  }
}

void ApplyFramelockPatch()
{
  fps_addr = fps_addr ? fps_addr : GetAbsoluteAddress(fps_offset);

  float FPS = std::clamp(FPS_LIMIT, 30, 300);
  FPS_LIMIT = FPS;

  if (!FPSUNLOCK_ENABLED)
    FPS = 60.0f;

  const float targetDelta = 1.0f / FPS;

  if (!WriteProtectedMemory(fps_addr, &targetDelta, sizeof(float)))
  {
    ErrorLog("Failed to patch framelock");
    return;
  }
  ApplySpeedFix(FPS);
}

void ApplyAutolootPatch()
{
  autoloot_addr =
      autoloot_addr ? autoloot_addr : GetAbsoluteAddress(autoloot_offset);

  constexpr uint8_t patchEnable[2] = {0xB0, 0x01};  // mov al,1
  constexpr uint8_t patchDisable[2] = {0x32, 0xC0}; // xor al,al

  if (!WriteProtectedMemory(autoloot_addr, AUTOLOOT_ENABLED ? patchEnable : patchDisable, sizeof(uint8_t) * 2))
    ErrorLog("Failed to patch autoloot");
}

void ApplyIntroPatch()
{
  introskip_addr =
      introskip_addr ? introskip_addr : GetAbsoluteAddress(introskip_offset);

  if (!INTROSKIP_ENABLED)
    return;

  constexpr uint8_t patchEnable[1] = {0x75};
  constexpr uint8_t patchDisable[1] = {0x74};

  if (!WriteProtectedMemory(introskip_addr, patchEnable, sizeof(char)))
    ErrorLog("Failed to patch logos");
}

void ApplyBorderlessPatch()
{
  if (FULLSCREEN_STATE)
    return;

  HWND hwnd = GetCurrentProcessWindow();

  if (BORDERLESS_ENABLED)
  {
    SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, screenWidth, screenHeight, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
  }
  else if (initialized)
  {
    uintptr_t curResAddr = GetAbsoluteAddress(resolution_pointer_offset + 3);
    uintptr_t curResValue = DereferenceStaticX64Pointer(curResAddr, 6);

    int width = *(int32_t *)curResValue;
    int height = *(int32_t *)(curResValue + 4);

    SetWindowLongPtr(hwnd, GWL_STYLE, WS_VISIBLE | WS_CAPTION | WS_BORDER | WS_CLIPSIBLINGS | WS_DLGFRAME | WS_SYSMENU | WS_GROUP | WS_MINIMIZEBOX);
    RECT wr = {0, 0, (LONG)width, (LONG)height};
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    AdjustWindowRectEx(&wr, style, false, exStyle);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, wr.right - wr.left, wr.bottom - wr.top,
                 SWP_NOREDRAW);
  }
}

void ApplyFovPatch()
{
  fovsetting_addr = fovsetting_addr ? fovsetting_addr : GetAbsoluteAddress(fovsetting_offset) + 8;
  fovData = fovData ? fovData : AllocateMemoryNearAddress(fovsetting_addr, sizeof(float));

  float fov = std::clamp(FOV_VALUE, -95, 95);
  float fovValue = (float)((PI / 180.0) * ((fov / 100.0f) + 1));
  if (!FOV_ENABLED)
    fovValue = 0.0174533f;

  if (!WriteProtectedMemory((uintptr_t)fovData, &fovValue, sizeof(float)))
  {
    ErrorLog("Failed to write FOV value to allocated memory");
    return;
  }

  uintptr_t nextAddress = fovsetting_addr + sizeof(int32_t);
  int32_t relative = static_cast<int32_t>((uintptr_t)fovData - nextAddress);

  if (!WriteProtectedMemory((uintptr_t)fovsetting_addr, &relative, sizeof(relative)))
    ErrorLog("Failed to patch FOV");
}

void ApplyResScalingFix()
{
  uintptr_t resScalingAddr = GetAbsoluteAddress(resolution_scaling_fix_offset);
  constexpr uint8_t PatchScalingFixEnable[3] = {0x90, 0x90, 0xEB};
  if (!WriteProtectedMemory(resScalingAddr, PatchScalingFixEnable, sizeof(uint8_t) * 3))
    ErrorLog("Failed to apply resolution scaling fix");
}

bool ForceWriteMemory(uintptr_t address, const void *data, size_t size)
{
  DWORD oldProtect;
  if (!VirtualProtect((LPVOID)address, size, PAGE_EXECUTE_READWRITE, &oldProtect))
    return false;

  SIZE_T bytesWritten = 0;
  BOOL result = WriteProcessMemory(GetCurrentProcess(), (LPVOID)address, data, size, &bytesWritten);

  VirtualProtect((LPVOID)address, size, oldProtect, &oldProtect); // restore

  return result && (bytesWritten == size);
}

void ApplyResPatch()
{
  if (initialized || !CUSTOM_RES_ENABLED)
    return;

  uintptr_t resAddr = GetAbsoluteAddress(resolution_default_840_offset);
  uintptr_t widthCheck = GetAbsoluteAddress(0x11BA038);
  uintptr_t heightCheck = GetAbsoluteAddress(0x11BA03E);
  uint8_t patchEnable[1] = {0x7E};
  uint8_t patchDisable[1] = {0x7F};

  while (*(uint8_t *)widthCheck != patchDisable[0])
    Sleep(10);

  // Patch resolution list to allow only 840x450
  if (!WriteProtectedMemory(widthCheck, patchEnable, sizeof(uint8_t)))
  {
    ErrorLog("Failed to patch resolution width list");
    return;
  }
  if (!WriteProtectedMemory(heightCheck, patchEnable, sizeof(uint8_t)))
  {
    ErrorLog("Failed to patch resolution height list");
    return;
  }

  // Replace 840x450 with custom resolution
  if (!WriteProtectedMemory(resAddr, &CUSTOM_RES_WIDTH, sizeof(CUSTOM_RES_WIDTH)))
  {
    ErrorLog("Failed to patch resolution width pointer");
    return;
  }
  if (!WriteProtectedMemory(resAddr + 4, &CUSTOM_RES_HEIGHT, sizeof(CUSTOM_RES_HEIGHT)))
  {
    ErrorLog("Failed to patch resolution height pointer");
    return;
  }

  ApplyResScalingFix();
}
