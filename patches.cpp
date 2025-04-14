#include "patches.h"
#include "offset.h"
#include "utils.h"
#include <cstdint>

uintptr_t fovData;
uintptr_t speedData;
constexpr double PI = 3.14159265358979323846;
float defaultTimescale;
float defaultPlayerTimescale;

typedef struct Patch
{
  uintptr_t addr;
  const void *data;
  size_t size;
  const char *str;
  bool write() const
  {
    return WriteProtectedMemory(addr, data, size, str);
  }
} Patch;

constexpr static const float speedFixMatrix[]{
    15.0f, 16.0f, 16.6667f, 18.0f, 18.6875f, 18.8516f, 20.0f,
    24.0f, 25.0f, 27.5f, 30.0f, 32.0f, 38.5f, 40.0f,
    48.0f, 49.5f, 50.0f, 57.2958f, 60.0f, 64.0f, 66.75f,
    67.0f, 78.8438f, 80.0f, 84.0f, 90.0f, 93.8f, 100.0f,
    120.0f, 127.0f, 128.0f, 130.0f, 140.0f, 144.0f, 150.0f};

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

void ApplySpeedFix(float FPS)
{
  uintptr_t speedFixAddr = GetAbsoluteAddress(speedfix_offset + 15);
  float idealSpeedFix = FPS / 2.0f;
  float closestSpeedFix = 30.0f; // Default value for 60 fps
  if (!speedData)
    speedData = AllocateMemoryNearAddress(speedFixAddr, sizeof(float));
  int32_t relative = (int32_t)(speedData - (speedFixAddr + sizeof(int32_t)));

  for (const float val : speedFixMatrix)
    if (abs(idealSpeedFix - val) < abs(idealSpeedFix - closestSpeedFix))
      closestSpeedFix = val;

  Patch patches[] = {
      {speedData, &closestSpeedFix, sizeof(float), "Write speed value to allocated memory"},
      {speedFixAddr, &relative, sizeof(int32_t), "Apply speed fix"},
  };

  for (const Patch &patch : patches)
    if (!patch.write())
      return;
}

void ApplyFramelockPatch()
{
  if (!INITIALIZED && !FPSUNLOCK_ENABLED)
    return;

  FPS_LIMIT = FPSUNLOCK_ENABLED ? std::clamp(FPS_LIMIT, 30, 300) : 60.0f;
  const float targetDelta = 1.0f / FPS_LIMIT;

  if (!WriteProtectedMemory(GetAbsoluteAddress(fps_offset), &targetDelta, sizeof(float), "Patch framelock"))
    return;

  ApplySpeedFix(FPS_LIMIT);
}

void ApplyAutolootPatch()
{
  constexpr uint8_t patchEnable[2] = {0xB0, 0x01};  // mov al,1
  constexpr uint8_t patchDisable[2] = {0x32, 0xC0}; // xor al,al

  WriteProtectedMemory(GetAbsoluteAddress(autoloot_offset), AUTOLOOT_ENABLED ? patchEnable : patchDisable, sizeof(uint8_t) * 2, "Patch autoloot");
}

void ApplyIntroPatch()
{
  if (!INTROSKIP_ENABLED && !INITIALIZED)
    return;

  constexpr uint8_t patchEnable[1] = {0x75};
  constexpr uint8_t patchDisable[1] = {0x74};

  WriteProtectedMemory(GetAbsoluteAddress(introskip_offset), patchEnable, sizeof(char), "Patch logos skip");
}

void ApplyBorderlessPatch()
{
  if (FULLSCREEN_STATE || (!INITIALIZED && !BORDERLESS_ENABLED))
    return;

  HWND hwnd = GetCurrentProcessWindow();

  if (BORDERLESS_ENABLED)
  {
    SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, screenWidth, screenHeight, SWP_NOREDRAW);
  }
  else if (INITIALIZED)
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
  if (!INITIALIZED && !FOV_ENABLED)
    return;

  uintptr_t fovsetting_addr = GetAbsoluteAddress(fovsetting_offset) + 8;
  if (!fovData)
    fovData = AllocateMemoryNearAddress(fovsetting_addr, sizeof(float));

  float fov = std::clamp(FOV_VALUE, -95, 95);
  float fovValue = FOV_ENABLED ? PI / 180.0 * (fov / 100.0f + 1) : 0.0174533f;
  int32_t relative = (int32_t)(fovData - (fovsetting_addr + sizeof(int32_t)));

  Patch patches[] = {
      {fovData, &fovValue, sizeof(float), "Write FOV value to allocated memory"},
      {fovsetting_addr, &relative, sizeof(relative), "Patch FOV"},
  };

  for (const Patch &patch : patches)
    if (!patch.write())
      return;
}

void ApplyResScalingFix()
{
  uintptr_t resScalingAddr = GetAbsoluteAddress(resolution_scaling_fix_offset);
  constexpr uint8_t PatchEnable[3] = {0x90, 0x90, 0xEB};
  WriteProtectedMemory(resScalingAddr, PatchEnable, sizeof(uint8_t) * 3, "Resolution scaling fix");
}

void ApplyResPatch()
{
  if (INITIALIZED || !CUSTOM_RES_ENABLED)
    return;

  uintptr_t resAddr = GetAbsoluteAddress(resolution_default_840_offset);
  uintptr_t widthCheck = GetAbsoluteAddress(resolution_width_check_offset);
  uintptr_t heightCheck = GetAbsoluteAddress(resolution_height_check_offset);
  constexpr uint8_t patchEnable[1] = {0x7E};
  constexpr uint8_t patchDisable[1] = {0x7F};

  while (*(uint8_t *)widthCheck != patchDisable[0])
    Sleep(10);

  Patch patches[] = {
      {widthCheck, patchEnable, sizeof(uint8_t), "Patch resolution width list"},
      {heightCheck, patchEnable, sizeof(uint8_t), "Patch resolution height list"},
      {resAddr, &CUSTOM_RES_WIDTH, sizeof(CUSTOM_RES_WIDTH), "Patch custom resolution width"},
      {resAddr + 4, &CUSTOM_RES_HEIGHT, sizeof(CUSTOM_RES_HEIGHT), "Patch custom resolution height"}};

  for (const Patch &patch : patches)
    if (!patch.write())
      return;

  ApplyResScalingFix();
}

bool ApplyTimescalePatch()
{
  if (!INITIALIZED && !TIMESCALE_ENABLED)
    return true;

  uintptr_t timescaleAddr = GetAbsoluteAddress(timescale_offset);
  if (!IsValidReadPtr(timescaleAddr, sizeof(uintptr_t)))
    return false;

  uintptr_t timescaleManager = DereferenceStaticX64Pointer(timescaleAddr, 7);
  if (!IsValidReadPtr(timescaleManager, sizeof(uintptr_t)))
    return false;

  uintptr_t globalTimescaleAddr = *(uintptr_t *)timescaleManager + *(int32_t *)(timescaleAddr + 11);
  if (!IsValidReadPtr(globalTimescaleAddr, sizeof(uintptr_t)))
    return false;

  if (!defaultTimescale)
    defaultTimescale = *(float *)globalTimescaleAddr;

  float timescale = TIMESCALE_ENABLED ? std::clamp(TIMESCALE_VALUE, 0.1f, 10.0f) : defaultTimescale;

  return WriteProtectedMemory(globalTimescaleAddr, &timescale, sizeof(float), "Patch global timescale");
}

bool ApplyPlayerTimescalePatch()
{
  if (!INITIALIZED && !PLAYER_TIMESCALE_ENABLED)
    return true;

  uintptr_t timescaleAddr = GetAbsoluteAddress(timescale_player_offset);
  if (!IsValidReadPtr(timescaleAddr, sizeof(uintptr_t)))
    return false;

  uintptr_t timescaleOffset = DereferenceStaticX64Pointer(timescaleAddr, 7);
  if (!IsValidReadPtr(timescaleOffset, sizeof(uintptr_t)))
    return false;

  uintptr_t timescaleOffset2 = *(int64_t *)timescaleOffset + 0x0088;
  if (!IsValidReadPtr(timescaleOffset2, sizeof(uintptr_t)))
    return false;

  uintptr_t timescaleOffset3 = *(int64_t *)timescaleOffset2 + 0x1FF8;
  if (!IsValidReadPtr(timescaleOffset3, sizeof(uintptr_t)))
    return false;

  uintptr_t timescaleOffset4 = *(int64_t *)timescaleOffset3 + 0x0028;
  if (!IsValidReadPtr(timescaleOffset4, sizeof(uintptr_t)))
    return false;

  uintptr_t playerTimescaleAddr = *(int64_t *)timescaleOffset4 + 0x0D00;
  if (!IsValidReadPtr(timescaleOffset4, sizeof(uintptr_t)))
    return false;

  if (!defaultPlayerTimescale)
    defaultPlayerTimescale = *(float *)playerTimescaleAddr;

  float timescale = PLAYER_TIMESCALE_ENABLED ? std::clamp(PLAYER_TIMESCALE_VALUE, 0.1f, 10.0f) : defaultPlayerTimescale;

  return WriteProtectedMemory(playerTimescaleAddr, &timescale, sizeof(float), "Patch player timescale");
}