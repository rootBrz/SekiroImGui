#include "patches.h"
#include "offset.h"
#include "utils.h"
#include <cstdint>

uintptr_t fovData;
uintptr_t speedData;
constexpr double PI = 3.14159265358979323846;
bool resInit = false;

void ApplySpeedFix(float FPS)
{
  speedfix_addr =
      speedfix_addr ? speedfix_addr : GetAbsoluteAddress(speedfix_offset + 15);
  speedData = speedData
                  ? speedData
                  : AllocateMemoryNearAddress(speedfix_addr, sizeof(float));

  static const float speedFixMatrix[] = {
      15.0f, 16.0f, 16.6667f, 18.0f, 18.6875f, 18.8516f, 20.0f,
      24.0f, 25.0f, 27.5f, 30.0f, 32.0f, 38.5f, 40.0f,
      48.0f, 49.5f, 50.0f, 57.2958f, 60.0f, 64.0f, 66.75f,
      67.0f, 78.8438f, 80.0f, 84.0f, 90.0f, 93.8f, 100.0f,
      120.0f, 127.0f, 128.0f, 130.0f, 140.0f, 144.0f, 150.0f};

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

  if (!WriteProtectedMemory((uintptr_t)speedData, &closestSpeedFix,
                            sizeof(float)))
  {
    MessageBoxA(nullptr, "Failed to write speed value to allocated memory",
                "Patch Error", MB_OK);
    return;
  }

  uintptr_t nextAddress = speedfix_addr + sizeof(int32_t);
  int32_t relative = static_cast<int32_t>((uintptr_t)speedData - nextAddress);

  if (!WriteProtectedMemory((uintptr_t)speedfix_addr, &relative,
                            sizeof(relative)))
  {
    MessageBoxA(nullptr, "Failed to patch FOV pointer", "Patch Error", MB_OK);
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
    MessageBoxA(nullptr, "Framelock patch failed", "Patch Error", MB_OK);
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

  if (!WriteProtectedMemory(autoloot_addr,
                            AUTOLOOT_ENABLED ? patchEnable : patchDisable,
                            sizeof(uint8_t) * 2))
  {
    MessageBoxA(nullptr, "Autoloot patch failed", "Patch Error", MB_OK);
  }
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
  {
    MessageBoxA(nullptr, "Skip intro patch failed", "Patch Error", MB_OK);
  }
}

void ApplyBorderlessPatch()
{
  if (FULLSCREEN_STATE)
    return;

  HWND hwnd;
  GetCurrentProcessWindow(&hwnd);

  if (BORDERLESS_ENABLED)
  {
    SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, screenWidth, screenHeight,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);
  }
  else
  {
    uintptr_t curResAddr = GetAbsoluteAddress(resolution_pointer_offset + 3);
    uintptr_t curResValue = DereferenceStaticX64Pointer(curResAddr, 6);

    int width = *(int32_t *)curResValue;
    int height = *(int32_t *)(curResValue + 4);

    SetWindowLongPtr(hwnd, GWL_STYLE,
                     WS_VISIBLE | WS_CAPTION | WS_BORDER | WS_CLIPSIBLINGS |
                         WS_DLGFRAME | WS_SYSMENU | WS_GROUP | WS_MINIMIZEBOX);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, width,
                 height, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
  }
}

void ApplyFovPatch()
{
  fovsetting_addr = fovsetting_addr ? fovsetting_addr
                                    : GetAbsoluteAddress(fovsetting_offset) + 8;
  fovData = fovData ? fovData
                    : AllocateMemoryNearAddress(fovsetting_addr, sizeof(float));

  float fov = std::clamp(FOV_VALUE, -95, 95);
  float fovValue = (float)((PI / 180.0) * ((fov / 100.0f) + 1));
  if (!FOV_ENABLED)
    fovValue = 0.0174533f;

  if (!WriteProtectedMemory((uintptr_t)fovData, &fovValue, sizeof(float)))
  {
    MessageBoxA(nullptr, "Failed to write FOV value to allocated memory",
                "Patch Error", MB_OK);
    return;
  }

  uintptr_t nextAddress = fovsetting_addr + sizeof(int32_t);
  int32_t relative = static_cast<int32_t>((uintptr_t)fovData - nextAddress);

  if (!WriteProtectedMemory((uintptr_t)fovsetting_addr, &relative,
                            sizeof(relative)))
  {
    MessageBoxA(nullptr, "Failed to patch FOV pointer", "Patch Error", MB_OK);
    return;
  }
}

void ApplyResScalingFix()
{
  uintptr_t resScalingAddr = GetAbsoluteAddress(resolution_scaling_fix_offset);
  constexpr uint8_t PatchScalingFixEnable[3] = {0x90, 0x90, 0xEB};
  if (!WriteProtectedMemory(resScalingAddr, PatchScalingFixEnable, sizeof(uint8_t) * 3))
  {
    MessageBoxA(nullptr, "Failed to patch resolution scaling pointer", "Patch Error", MB_OK);
    return;
  }
}

void ApplyResPatch()
{
  HWND hwnd;
  GetCurrentProcessWindow(&hwnd);

  uintptr_t resToPatch = GetSystemMetrics(SM_CXSCREEN) >= 1920 ? resolution_default_offset : resolution_default_720_offset;
  uintptr_t resAddr = GetAbsoluteAddress(resToPatch);

  constexpr uint8_t PatchDisable[8] = {0x80, 0x07, 0x00, 0x00,
                                       0x38, 0x04, 0x00, 0x00};

  if (!CUSTOM_RES_ENABLED)
  {
    if (!WriteProtectedMemory(resAddr, PatchDisable, sizeof(uint8_t) * 8))
    {
      MessageBoxA(nullptr, "Failed to patch resolution pointer", "Patch Error",
                  MB_OK);
      return;
    }
    return;
  }

  if (!WriteProtectedMemory(resAddr, &CUSTOM_RES_WIDTH,
                            sizeof(CUSTOM_RES_WIDTH)))
  {
    MessageBoxA(nullptr, "Failed to patch resolution width pointer",
                "Patch Error", MB_OK);
    return;
  }

  if (!WriteProtectedMemory(resAddr + 4, &CUSTOM_RES_HEIGHT,
                            sizeof(CUSTOM_RES_HEIGHT)))
  {
    MessageBoxA(nullptr, "Failed to patch resolution height pointer",
                "Patch Error", MB_OK);
    return;
  }

  ApplyResScalingFix();

  if (!resInit)
  {
    RECT totalRect = {0, 0, CUSTOM_RES_WIDTH, CUSTOM_RES_HEIGHT};
    AdjustWindowRect(&totalRect, GetWindowLong(hwnd, GWL_STYLE), FALSE);
    SetWindowPos(hwnd, NULL, 0, 0, totalRect.right - totalRect.left,
                 totalRect.bottom - totalRect.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
    resInit = true;
  }
}
