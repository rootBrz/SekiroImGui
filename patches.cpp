#include "patches.h"
#include "gui.h"
#include "offset.h"
#include "utils.h"

uintptr_t fovData;
uintptr_t speedData;
constexpr double PI = 3.14159265358979323846;

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

void ApplySpeedFix(float fps)
{
  uintptr_t speedFixAddr = GetAbsoluteAddress(speedfix_offset + 15);
  float idealSpeedFix = fps / 2.0f;
  float closestSpeedFix = 30.0f; // Default value for 60 fps
  if (!speedData)
    speedData = AllocateMemoryNearAddress(speedFixAddr, sizeof(float));
  int32_t relative = (int32_t)(speedData - (speedFixAddr + sizeof(int32_t)));

  for (const float val : speedFixMatrix)
    if (abs(idealSpeedFix - val) < abs(idealSpeedFix - closestSpeedFix))
      closestSpeedFix = val;

  Patch patches[] = {
      {speedData, &closestSpeedFix, sizeof(float), "Write speed value to allocated memory"},
      {speedFixAddr, &relative, sizeof(int32_t), "Patch speed value"},
  };

  for (const Patch &patch : patches)
    if (!patch.write())
      return;
}

void ApplyFramelockPatch()
{
  if (!INITIALIZED && !FPSUNLOCK_ENABLED)
    return;

  float fpsLimit = FPSUNLOCK_ENABLED ? std::clamp(FPS_LIMIT, 30, 300) : 60.0f;
  const float targetDelta = 1.0f / (GAME_LOADING ? 300.0f : fpsLimit);

  if (!WriteProtectedMemory(GetAbsoluteAddress(fps_offset), &targetDelta, sizeof(float), "Patch framelock"))
    return;

  if (!GAME_LOADING)
    ApplySpeedFix(fpsLimit);
}

void ApplyAutolootPatch()
{
  if (!AUTOLOOT_ENABLED && !INITIALIZED)
    return;

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

  if (BORDERLESS_ENABLED)
  {
    SetWindowLongPtr(GAME_WINDOW, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(GAME_WINDOW, HWND_TOP, 0, 0, screenWidth, screenHeight, SWP_NOREDRAW);
  }
  else if (INITIALIZED)
  {
    uintptr_t curResAddr = GetAbsoluteAddress(resolution_pointer_offset + 3);
    uintptr_t curResValue = DereferenceStaticX64Pointer(curResAddr, 6);

    int width = *(int32_t *)curResValue;
    int height = *(int32_t *)(curResValue + 4);

    SetWindowLongPtr(GAME_WINDOW, GWL_STYLE, WS_VISIBLE | WS_CAPTION | WS_BORDER | WS_CLIPSIBLINGS | WS_DLGFRAME | WS_SYSMENU | WS_GROUP | WS_MINIMIZEBOX);
    RECT wr = {0, 0, (LONG)width, (LONG)height};
    DWORD style = GetWindowLong(GAME_WINDOW, GWL_STYLE);
    DWORD exStyle = GetWindowLong(GAME_WINDOW, GWL_EXSTYLE);
    AdjustWindowRectEx(&wr, style, false, exStyle);
    SetWindowPos(GAME_WINDOW, HWND_TOP, 0, 0, wr.right - wr.left, wr.bottom - wr.top,
                 SWP_NOREDRAW);
  }
}

void ApplyFovPatch()
{
  if (!INITIALIZED && !FOV_ENABLED)
    return;

  uintptr_t fovAddr = GetAbsoluteAddress(fovsetting_offset) + 8;
  if (!fovData)
    fovData = AllocateMemoryNearAddress(fovAddr, sizeof(float));

  float fov = std::clamp(FOV_VALUE, -95, 95);
  float fovValue = FOV_ENABLED ? PI / 180.0 * (fov / 100.0f + 1) : 0.0174533f;
  int32_t relative = (int32_t)(fovData - (fovAddr + sizeof(int32_t)));

  Patch patches[] = {
      {fovData, &fovValue, sizeof(float), "Write FOV value to allocated memory"},
      {fovAddr, &relative, sizeof(relative), "Patch FOV"},
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

  float timescale = TIMESCALE_ENABLED ? std::clamp(TIMESCALE_VALUE, 0.1f, 10.0f) : 1.0f;

  return WriteProtectedMemory(globalTimescaleAddr, &timescale, sizeof(float), "Patch global timescale");
}

void ApplyPlayerTimescalePatch()
{
  if (!INITIALIZED && !PLAYER_TIMESCALE_ENABLED)
    return;

  uintptr_t timescaleAddr = GetAbsoluteAddress(timescale_player_offset);
  if (!IsValidReadPtr(timescaleAddr, sizeof(uintptr_t)))
    return;
  uintptr_t timescaleOffset = DereferenceStaticX64Pointer(timescaleAddr, 7);
  if (!IsValidReadPtr(timescaleOffset, sizeof(uintptr_t)))
    return;
  uintptr_t timescaleOffset2 = *(int64_t *)timescaleOffset + 0x0088;
  if (!IsValidReadPtr(timescaleOffset2, sizeof(uintptr_t)))
    return;
  uintptr_t timescaleOffset3 = *(int64_t *)timescaleOffset2 + 0x1FF8;
  if (!IsValidReadPtr(timescaleOffset3, sizeof(uintptr_t)))
    return;
  uintptr_t timescaleOffset4 = *(int64_t *)timescaleOffset3 + 0x0028;
  if (!IsValidReadPtr(timescaleOffset4, sizeof(uintptr_t)))
    return;
  uintptr_t playerTimescaleAddr = *(int64_t *)timescaleOffset4 + 0x0D00;
  if (!IsValidReadPtr(playerTimescaleAddr, sizeof(uintptr_t)))
    return;

  float timescale = PLAYER_TIMESCALE_ENABLED ? std::clamp(PLAYER_TIMESCALE_VALUE, 0.1f, 10.0f) : 1.0f;

  WriteProtectedMemory(playerTimescaleAddr, &timescale, sizeof(float), "Patch player timescale");
}

void ApplyCamResetPatch()
{
  if (!INITIALIZED && !DISABLE_CAMRESET_ENABLED)
    return;

  uintptr_t camResetAddr = GetAbsoluteAddress(camreset_lockon_offset) + 6;
  constexpr uint8_t camResetDisable[1] = {0x00};
  constexpr uint8_t camResetEnable[1] = {0x01};

  WriteProtectedMemory(camResetAddr, DISABLE_CAMRESET_ENABLED ? camResetDisable : camResetEnable, sizeof(uint8_t), "Patch cam reset");
}

// Removes hardvalue of 60 hz before call of ResizeTarget
void ApplyFullscreenHZPatch()
{
  uintptr_t numAddr = GetAbsoluteAddress(fullscreen_refreshrate_num);

  constexpr uint8_t patchEnable[18] = {
      0xB8, 0x00, 0x00, 0x00, 0x00, // mov eax, 0           Refresh Rate Numerator before calling ResizeTarget
      0x89, 0x44, 0x24, 0x28,       // mov [rsp+28], eax
      0xB8, 0x00, 0x00, 0x00, 0x00, // mov eax, 0           Refresh Rate Denumerator before calling ResizeTarget
      0x89, 0x44, 0x24, 0x2C        // mov [rsp+2C], eax
  };

  uintptr_t detour = InjectShellcodeWithJumpBack(numAddr, patchEnable, sizeof(patchEnable), 14, "Write fullscreen hz fix");

  WriteProtectedMemory(numAddr, RelJmpBytes(14, (int32_t)(detour - (numAddr + 5))).data(), 14, "Patch fullscreen hz fix");
}

// Dragonrot
void ApplyDragonrotPatch()
{
  if (!INITIALIZED && !DISABLE_DRAGONROT_ENABLED)
    return;

  uintptr_t dragonrotAddr = GetAbsoluteAddress(dragonrot_effect_offset) + 13;

  uint8_t dragonrotDisable[] = {0x90, 0x90, 0x90, 0xE9};
  uint8_t dragonrotEnable[] = {0x84, 0xC0, 0x0F, 0x85};

  WriteProtectedMemory(dragonrotAddr, DISABLE_DRAGONROT_ENABLED ? dragonrotDisable : dragonrotEnable, sizeof(dragonrotDisable), "Patch dragonrot");
}

// Death penalties
uintptr_t deathPenalties1Addr;
uintptr_t deathPenalties2Addr;
uint8_t deathPenalties1OrigBytes[5];
uint8_t deathPenalties2OrigBytes[32];
void ApplyDeathPenaltiesPatch()
{
  if (!INITIALIZED)
  {
    deathPenalties1Addr = GetAbsoluteAddress(deathpenalties1_offset) + 11;
    deathPenalties2Addr = GetAbsoluteAddress(deathpenalties2_offset);

    ReadProcessMemory(GetCurrentProcess(), (LPVOID)deathPenalties1Addr, deathPenalties1OrigBytes, sizeof(deathPenalties1OrigBytes), nullptr);
    ReadProcessMemory(GetCurrentProcess(), (LPVOID)deathPenalties2Addr, deathPenalties2OrigBytes, sizeof(deathPenalties2OrigBytes), nullptr);
  }

  if (!INITIALIZED && !DISABLE_DEATH_PENALTIES_ENABLED)
    return;

  constexpr uint8_t disablePenalties1[] = {0x90, 0x90, 0x90, 0x90, 0x90};
  constexpr uint8_t disablePenalties2[] = {
      0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
      0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
      0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
      0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};

  WriteProtectedMemory(deathPenalties1Addr, DISABLE_DEATH_PENALTIES_ENABLED ? disablePenalties1 : deathPenalties1OrigBytes, sizeof(disablePenalties1), "Patch death penalties 1");
  WriteProtectedMemory(deathPenalties2Addr, DISABLE_DEATH_PENALTIES_ENABLED ? disablePenalties2 : deathPenalties2OrigBytes, sizeof(disablePenalties2), "Patch death penalties 2");
}

// Autorotate
uintptr_t camAdjustPitchAddr;
uintptr_t camAdjustPitchXYAddr;
uintptr_t camAdjustYawZAddr;
uintptr_t camAdjustYawXYAddr;
uintptr_t camAdjustPitchCode;
uintptr_t camAdjustPitchXYCode;
uintptr_t camAdjustYawZCode;
uintptr_t camAdjustYawXYCode;
uint8_t camAdjustPitchOrigBytes[7];
uint8_t camAdjustPitchXYOrigBytes[12];
uint8_t camAdjustYawZAddrOrigBytes[8];
uint8_t camAdjustYawXYAddrOrigBytes[8];
void ApplyCamAutorotatePatch()
{
  if (!INITIALIZED)
  {
    camAdjustPitchAddr = GetAbsoluteAddress(camadjust_pitch_offset);
    camAdjustPitchXYAddr = GetAbsoluteAddress(camadjust_pitch_xy_offset);
    camAdjustYawZAddr = GetAbsoluteAddress(camadjust_yaw_z_offset) + 5;
    camAdjustYawXYAddr = GetAbsoluteAddress(camadjust_yaw_xy_offset) + 5;

    ReadProcessMemory(GetCurrentProcess(), (LPVOID)camAdjustPitchAddr, camAdjustPitchOrigBytes, sizeof(camAdjustPitchOrigBytes), nullptr);
    ReadProcessMemory(GetCurrentProcess(), (LPVOID)camAdjustPitchXYAddr, camAdjustPitchXYOrigBytes, sizeof(camAdjustPitchXYOrigBytes), nullptr);
    ReadProcessMemory(GetCurrentProcess(), (LPVOID)camAdjustYawZAddr, camAdjustYawZAddrOrigBytes, sizeof(camAdjustYawZAddrOrigBytes), nullptr);
    ReadProcessMemory(GetCurrentProcess(), (LPVOID)camAdjustYawXYAddr, camAdjustYawXYAddrOrigBytes, sizeof(camAdjustYawXYAddrOrigBytes), nullptr);

    // Camera Pitch (Z-axis)
    constexpr uint8_t INJECT_CAMADJUST_PITCH_SHELLCODE[] = {
        0x0F, 0x28, 0xA6, 0x70, 0x01, 0x00, 0x00, // movaps xmm4, [rsi+0x170]
        0x0F, 0x29, 0xA5, 0x70, 0x08, 0x00, 0x00  // movaps [rbp+0x870], xmm4
    };

    // Camera Yaw (Z-axis)
    constexpr uint8_t INJECT_CAMADJUST_YAW_Z_SHELLCODE[] = {
        0xF3, 0x0F, 0x10, 0x86, 0x74, 0x01, 0x00, 0x00, // movss xmm0, [rsi+0x174]
        0xF3, 0x0F, 0x11, 0x86, 0x74, 0x01, 0x00, 0x00  // movss [rsi+0x174], xmm0
    };

    // Camera Pitch (XY-axis)
    constexpr uint8_t INJECT_CAMADJUST_PITCH_XY_SHELLCODE[] = {
        0xF3, 0x0F, 0x10, 0x86, 0x70, 0x01, 0x00, 0x00, // movss xmm0, [rsi+0x170]
        0xF3, 0x0F, 0x11, 0x00,                         // movss [rax], xmm0
        0xF3, 0x0F, 0x10, 0x00,                         // movss xmm0, [rax]
        0xF3, 0x0F, 0x11, 0x86, 0x70, 0x01, 0x00, 0x00  // movss [rsi+0x170], xmm0
    };

    // Camera Yaw (XY-axis)
    constexpr uint8_t INJECT_CAMADJUST_YAW_XY_SHELLCODE[] = {
        0xF3, 0x0F, 0x10, 0x86, 0x74, 0x01, 0x00, 0x00, // movss xmm0, [rsi+0x174]
        0xF3, 0x0F, 0x11, 0x86, 0x74, 0x01, 0x00, 0x00  // movss [rsi+0x174], xmm0
    };

    camAdjustPitchCode = InjectShellcodeWithJumpBack(camAdjustPitchAddr, INJECT_CAMADJUST_PITCH_SHELLCODE, sizeof(INJECT_CAMADJUST_PITCH_SHELLCODE), sizeof(camAdjustPitchOrigBytes), "Write cam adjust pitch code to allocated memory");
    camAdjustPitchXYCode = InjectShellcodeWithJumpBack(camAdjustPitchXYAddr, INJECT_CAMADJUST_PITCH_XY_SHELLCODE, sizeof(INJECT_CAMADJUST_PITCH_XY_SHELLCODE), sizeof(camAdjustPitchXYOrigBytes), "Write cam adjust pitch XY code to allocated memory");
    camAdjustYawZCode = InjectShellcodeWithJumpBack(camAdjustYawZAddr, INJECT_CAMADJUST_YAW_Z_SHELLCODE, sizeof(INJECT_CAMADJUST_YAW_Z_SHELLCODE), sizeof(camAdjustYawZAddrOrigBytes), "Write cam adjust yaw Z code to allocated memory");
    camAdjustYawXYCode = InjectShellcodeWithJumpBack(camAdjustYawXYAddr, INJECT_CAMADJUST_YAW_XY_SHELLCODE, sizeof(INJECT_CAMADJUST_YAW_XY_SHELLCODE), sizeof(camAdjustYawXYAddrOrigBytes), "Write cam adjust yaw XY code to allocated memory");
  }

  if (!INITIALIZED && !DISABLE_CAMERA_AUTOROTATE_ENABLED)
    return;

  WriteProtectedMemory(camAdjustPitchAddr, DISABLE_CAMERA_AUTOROTATE_ENABLED ? RelJmpBytes(7, (int32_t)(camAdjustPitchCode - (camAdjustPitchAddr + 5))).data() : camAdjustPitchOrigBytes, sizeof(camAdjustPitchOrigBytes), "Patch cam adjust pitch");
  WriteProtectedMemory(camAdjustPitchXYAddr, DISABLE_CAMERA_AUTOROTATE_ENABLED ? RelJmpBytes(12, (int32_t)(camAdjustPitchXYCode - (camAdjustPitchXYAddr + 5))).data() : camAdjustPitchXYOrigBytes, sizeof(camAdjustPitchXYOrigBytes), "Patch cam adjust pitch XY");
  WriteProtectedMemory(camAdjustYawZAddr, DISABLE_CAMERA_AUTOROTATE_ENABLED ? RelJmpBytes(8, (int32_t)(camAdjustYawZCode - (camAdjustYawZAddr + 5))).data() : camAdjustYawZAddrOrigBytes, sizeof(camAdjustYawZAddrOrigBytes), "Patch cam adjust yaw Z");
  WriteProtectedMemory(camAdjustYawXYAddr, DISABLE_CAMERA_AUTOROTATE_ENABLED ? RelJmpBytes(8, (int32_t)(camAdjustYawXYCode - (camAdjustYawXYAddr + 5))).data() : camAdjustYawXYAddrOrigBytes, sizeof(camAdjustYawXYAddrOrigBytes), "Patch cam adjust yaw XY");
}