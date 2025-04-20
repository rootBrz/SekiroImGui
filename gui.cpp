#define IMGUI_DISABLE_DEFAULT_FONT

#include "gui.h"
#include "offset.h"
#include "patches.h"
#include "roboto_regular_data.h"
#include "utils.h"

HWND window = nullptr;
static bool init = false;
static ImFont *statsRoboto;
static uint8_t lastLoadingStatus = 0x00;
static bool initPatches = false;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg,
                                              WPARAM wParam, LPARAM lParam);

long CALLBACK DetourResizeBuffers(IDXGISwapChain *pSwapChain, UINT BufferCount,
                                  UINT Width, UINT Height,
                                  DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  if (mainRenderTargetView)
  {
    mainRenderTargetView->Release();
    mainRenderTargetView = NULL;
  }
  if (pContext)
  {
    pContext->Release();
    pContext = NULL;
  }
  if (pDevice)
  {
    pDevice->Release();
    pDevice = NULL;
  }
  SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)(oWndProc));

  init = false;

  return fnResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat,
                         SwapChainFlags);
}

bool InitKillsDeathsAddresses()
{
  // Deaths
  uintptr_t pDeathOffset = GetAbsoluteAddress(player_deaths_offset + 29);
  if (!IsValidReadPtr(pDeathOffset, sizeof(uintptr_t)))
    return false;
  uintptr_t pDeathOffset2 = DereferenceStaticX64Pointer(pDeathOffset, 7);
  if (!IsValidReadPtr(pDeathOffset2, sizeof(uintptr_t)))
    return false;
  uintptr_t pPlayerDeaths = *(uintptr_t *)pDeathOffset2 + *(int32_t *)(pDeathOffset + 9);
  if (!IsValidReadPtr(pPlayerDeaths, sizeof(uintptr_t)))
    return false;

  // Kills
  uintptr_t pKillsOffset = GetAbsoluteAddress(total_kills_offset) + 7;
  if (!IsValidReadPtr(pKillsOffset, sizeof(uintptr_t)))
    return false;
  uintptr_t pKillsOffset2 = DereferenceStaticX64Pointer(pKillsOffset, 7);
  if (!IsValidReadPtr(pKillsOffset2, sizeof(uintptr_t)))
    return false;
  uintptr_t pTotalKills = *(uintptr_t *)(*(uintptr_t *)pKillsOffset2 + 0x08) + 0xDC;
  if (!IsValidReadPtr(pTotalKills, sizeof(uintptr_t)))
    return false;

  // Final Addresses
  player_deaths_addr = reinterpret_cast<int *>(pPlayerDeaths);
  total_kills_addr = reinterpret_cast<int *>(pTotalKills);

  return true;
}

void CreateImGuiLayout()
{
  ImGuiIO &io = ImGui::GetIO();

  // Helper lambda to update the INI setting and optionally apply a patch function.
  auto updateSetting = [&](const char *key, auto value, void (*patchFunc)() = nullptr)
  {
    SetIniValue(key, value);
    RefreshIniValues();
    if (patchFunc)
      patchFunc();
  };

  io.MouseDrawCursor = true;

  ImGui::Begin("Sekiro FPS Unlock ImGui", nullptr, ImGuiWindowFlags_NoResize);

  // --- FPS Controls ---
  if (ImGui::Checkbox("Enable FPS Unlock", reinterpret_cast<bool *>(&FPSUNLOCK_ENABLED)))
    updateSetting("EnableFpsUnlock", FPSUNLOCK_ENABLED, ApplyFramelockPatch);

  ImGui::SameLine();
  ImGui::SetNextItemWidth(100);
  if (ImGui::InputInt("##fpslimit", &FPS_LIMIT, 1, 10))
  {
    FPS_LIMIT = std::clamp(FPS_LIMIT, 30, 300);
    updateSetting("FpsLimit", FPS_LIMIT, ApplyFramelockPatch);
  }

  // --- VSync ---
  if (ImGui::Checkbox("Enable VSync", reinterpret_cast<bool *>(&VSYNC_ENABLED)))
    updateSetting("EnableVSync", VSYNC_ENABLED);

  // --- Borderless windowed toggle (disabled when in fullscreen) ---
  ImGui::BeginDisabled(FULLSCREEN_STATE);
  if (ImGui::Checkbox("Enable Borderless Windowed", reinterpret_cast<bool *>(&BORDERLESS_ENABLED)))
    updateSetting("EnableBorderlessFullscreen", BORDERLESS_ENABLED, ApplyBorderlessPatch);

  if (FULLSCREEN_STATE && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("This option is disabled because the game is in fullscreen state, change to windowed!");
  ImGui::EndDisabled();

  // --- Autoloot ---
  if (ImGui::Checkbox("Enable Autoloot", reinterpret_cast<bool *>(&AUTOLOOT_ENABLED)))
    updateSetting("EnableAutoloot", AUTOLOOT_ENABLED, ApplyAutolootPatch);

  // --- Logo Skip ---
  if (ImGui::Checkbox("Enable Logo Skip", reinterpret_cast<bool *>(&INTROSKIP_ENABLED)))
    updateSetting("EnableIntroSkip", INTROSKIP_ENABLED);

  // --- Cam Reset ---
  if (ImGui::Checkbox("Disable Cam Reset", reinterpret_cast<bool *>(&DISABLE_CAMRESET_ENABLED)))
    updateSetting("DisableCamReset", DISABLE_CAMRESET_ENABLED, ApplyCamResetPatch);
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("If you press your target lock-on key and no target is in sight the game will reset and center the camera position and disable your input while it's doing so. Ticking this checkbox will remove this behaviour of the game.");

  // --- Cam Autorotate ---
  if (ImGui::Checkbox("Disable Cam Autorotate", reinterpret_cast<bool *>(&DISABLE_CAMERA_AUTOROTATE_ENABLED)))
    updateSetting("DisableCamAutorotate", DISABLE_CAMERA_AUTOROTATE_ENABLED, ApplyCamAutorotatePatch);
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("This will completely disable the automatic camera rotation adjustments when you are moving. This is intended for mouse users.");

  // --- Death Penalties ---
  if (ImGui::Checkbox("Disable Death Penalties (except dragonrot)", reinterpret_cast<bool *>(&DISABLE_DEATH_PENALTIES_ENABLED)))
    updateSetting("DisableDeathPenalties", DISABLE_DEATH_PENALTIES_ENABLED, ApplyDeathPenaltiesPatch);
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("Like 'Unseen Aid' you will not lose any Sen or Experience upon death with this option enabled. Dragonrot will not be modified.");

  // --- Dragonrot ---
  if (ImGui::Checkbox("Disable Dragonrot", reinterpret_cast<bool *>(&DISABLE_DRAGONROT_ENABLED)))
    updateSetting("DisableDragonrot", DISABLE_DRAGONROT_ENABLED, ApplyDragonrotPatch);
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("This option will remove the effect dragonrot has on NPCs, if an NPC already got dragonrot then it will ensure that their condition won't worsen when you die.\nThe internal dragonrot counter will however keep increasing, nobody will be affected by it though.\nKeep in mind that there are certain thresholds regarding amount of deaths between dragonrot levels.\nIf you enable this feature and die a level might get skipped so even when you disable it afterwards the dragonrot level for all NPCs will only increase after you have hit the next threshold.");

  // --- Field of View (FOV) ---
  if (ImGui::Checkbox("FOV", reinterpret_cast<bool *>(&FOV_ENABLED)))
    updateSetting("EnableFOV", FOV_ENABLED, ApplyFovPatch);

  ImGui::SameLine();
  ImGui::BeginDisabled(!FOV_ENABLED);
  if (ImGui::SliderInt("##fovvalue", &FOV_VALUE, -95, 95, "%d", ImGuiSliderFlags_AlwaysClamp))
    updateSetting("ValueFOV", FOV_VALUE, ApplyFovPatch);

  ImGui::EndDisabled();

  // --- Timescale ---
  if (ImGui::Checkbox("Global Timescale", reinterpret_cast<bool *>(&TIMESCALE_ENABLED)))
    updateSetting("EnableTimescale", TIMESCALE_ENABLED, reinterpret_cast<void (*)()>(ApplyTimescalePatch));

  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("NOT SAFE, USE WITH CAUTION!");

  ImGui::SameLine();
  ImGui::BeginDisabled(!TIMESCALE_ENABLED);
  ImGui::SetNextItemWidth(200);
  if (ImGui::SliderFloat("##timescalevalue", &TIMESCALE_VALUE, 0.1, 10, "%.2f", ImGuiSliderFlags_AlwaysClamp))
    updateSetting("ValueTimescale", TIMESCALE_VALUE, reinterpret_cast<void (*)()>(ApplyTimescalePatch));

  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("NOT SAFE, USE WITH CAUTION!");

  ImGui::EndDisabled();

  // --- Player Timescale ---
  if (ImGui::Checkbox("Player Timescale", reinterpret_cast<bool *>(&PLAYER_TIMESCALE_ENABLED)))
    updateSetting("EnablePlayerTimescale", PLAYER_TIMESCALE_ENABLED, ApplyPlayerTimescalePatch);

  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("NOT SAFE, USE WITH CAUTION!");

  ImGui::SameLine();
  ImGui::BeginDisabled(!PLAYER_TIMESCALE_ENABLED);
  ImGui::SetNextItemWidth(200);
  if (ImGui::SliderFloat("##playertimescalevalue", &PLAYER_TIMESCALE_VALUE, 0.1, 10, "%.2f", ImGuiSliderFlags_AlwaysClamp))
    updateSetting("ValuePlayerTimescale", PLAYER_TIMESCALE_VALUE, ApplyPlayerTimescalePatch);

  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("NOT SAFE, USE WITH CAUTION!");

  ImGui::EndDisabled();

  // --- Custom Resolution ---
  if (ImGui::Checkbox("Enable Custom Resolution", reinterpret_cast<bool *>(&CUSTOM_RES_ENABLED)))
    updateSetting("EnableCustomRes", CUSTOM_RES_ENABLED);

  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("Forces custom resolution, REQUIRES RESTART TO ENABLE/DISABLE!!!");

  ImGui::BeginDisabled(!CUSTOM_RES_ENABLED);
  ImGui::SetNextItemWidth(110);
  if (ImGui::InputInt("##customreswidth", &CUSTOM_RES_WIDTH))
    updateSetting("CustomResWidth", CUSTOM_RES_WIDTH);

  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("Forces custom resolution, REQUIRES RESTART TO ENABLE/DISABLE!!!");

  ImGui::SameLine();
  ImGui::SetNextItemWidth(110);
  if (ImGui::InputInt("##customresheight", &CUSTOM_RES_HEIGHT))
    updateSetting("CustomResHeight", CUSTOM_RES_HEIGHT);

  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("Forces custom resolution, REQUIRES RESTART TO ENABLE/DISABLE!!!");
  ImGui::EndDisabled();

  // --- Player Deaths and Kills ---
  if (ImGui::Checkbox("Show Player Stats", reinterpret_cast<bool *>(&SHOW_PLAYER_DEATHSKILLS_ENABLED)))
    updateSetting("EnableShowPlayerDeathsKills", SHOW_PLAYER_DEATHSKILLS_ENABLED);

  ImGui::SetNextItemWidth(200);
  if (ImGui::SliderInt("Stats Text X", &PLAYER_DEATHSKILLS_X, 0, io.DisplaySize.x))
    updateSetting("PlayerDeathsKillsX", PLAYER_DEATHSKILLS_X);

  ImGui::SetNextItemWidth(200);
  if (ImGui::SliderInt("Stats Text Y", &PLAYER_DEATHSKILLS_Y, 0, io.DisplaySize.y))
    updateSetting("PlayerDeathsKillsY", PLAYER_DEATHSKILLS_Y);

  ImGui::SetNextItemWidth(200);
  if (ImGui::SliderInt("Stats Font Size", &PLAYER_DEATHSKILLS_FZ, 16, 64))
    updateSetting("PlayerDeathsKillsFZ", PLAYER_DEATHSKILLS_FZ);

  // --- Display Totals ---
  if (player_deaths_addr != nullptr && total_kills_addr != nullptr)
    ImGui::Text("Deaths: %d\nKills: %d", *player_deaths_addr, (*total_kills_addr - *player_deaths_addr));

  ImGui::End();
}

// Detour present to render ImGui
long CALLBACK DetourPresent(IDXGISwapChain *pSwapChain, UINT syncInterval,
                            UINT flags)
{
  if (!init)
  {
    if (SUCCEEDED(
            pSwapChain->GetDevice(__uuidof(ID3D11Device), (void **)&pDevice)))
    {
      pDevice->GetImmediateContext(&pContext);
      DXGI_SWAP_CHAIN_DESC sd;
      pSwapChain->GetDesc(&sd);
      window = sd.OutputWindow;
      ID3D11Texture2D *pBackBuffer;
      pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                            (LPVOID *)&pBackBuffer);
      pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
      pBackBuffer->Release();
      oWndProc =
          (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
      ImGui::CreateContext();
      ImGuiIO &io = ImGui::GetIO();

      ImFont *roboto = io.Fonts->AddFontFromMemoryCompressedTTF(roboto_regular_data_compressed_data, roboto_regular_data_compressed_size, 20.0f, NULL);
      io.FontDefault = roboto;
      statsRoboto = io.Fonts->AddFontFromMemoryCompressedTTF(roboto_regular_data_compressed_data, roboto_regular_data_compressed_size, 64.0f, NULL);

      io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
      ImGui_ImplWin32_Init(window);
      ImGui_ImplDX11_Init(pDevice, pContext);
      init = true;
    }
    else
      return fnPresent(pSwapChain, VSYNC_ENABLED, flags);
  }

  pSwapChain->GetFullscreenState((BOOL *)&FULLSCREEN_STATE, nullptr);

  // Do some things in/after loading screen
  GAME_LOADING = *reinterpret_cast<uint8_t *>(is_game_in_loading_addr);
  if (lastLoadingStatus != GAME_LOADING)
  {
    lastLoadingStatus = GAME_LOADING;
    ApplyFramelockPatch();
    if (!lastLoadingStatus)
    {
      ApplyPlayerTimescalePatch();
      if (!initPatches)
      {
        InitKillsDeathsAddresses();
        ApplyTimescalePatch();
        initPatches = true;
      }
    }
  }

  ImGui_ImplDX11_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = nullptr;
  ImVec2 displaySize = ImGui::GetIO().DisplaySize;
  io.MouseDrawCursor = false;
  ImGui::SetNextWindowPos(
      ImVec2((io.DisplaySize.x * 0.35f), io.DisplaySize.y * 0.5f),
      ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

  if (SHOW_IMGUI)
    CreateImGuiLayout();

  if (SHOW_PLAYER_DEATHSKILLS_ENABLED && (player_deaths_addr != nullptr && total_kills_addr != nullptr))
  {
    ImDrawList *draw_list = ImGui::GetForegroundDrawList();
    char textBuffer[128];
    snprintf(textBuffer, sizeof(textBuffer), "Deaths: %d\nKills: %d", *player_deaths_addr, (*total_kills_addr - *player_deaths_addr));
    draw_list->AddText(statsRoboto, PLAYER_DEATHSKILLS_FZ, ImVec2(PLAYER_DEATHSKILLS_X, PLAYER_DEATHSKILLS_Y), IM_COL32(255, 255, 255, 255), textBuffer);
  }

  ImGui::EndFrame();
  ImGui::Render();
  pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

  return fnPresent(pSwapChain, GAME_LOADING ? 0 : VSYNC_ENABLED, flags);
}

// Detour SetCursorPos to unstuck cursor from center of screen if ImGui open
BOOL WINAPI DetourSetCursorPos(int X, int Y)
{
  if (SHOW_IMGUI)
  {
    return 0;
  }
  return fnSetCursorPos(X, Y);
}

// Detour ChangeDisplaySettingsExW remove 60hz refresh rate lock
long WINAPI DetourChangeDisplaySettingsExW(LPCWSTR lpszDeviceName, DEVMODEW *lpDevMode, HWND hwnd, DWORD dwflags, LPVOID lParam)
{
  if (lpDevMode != nullptr)
  {
    lpDevMode->dmFields &= ~DM_DISPLAYFREQUENCY;
  }

  return fnChangeDisplaySettingsExW(lpszDeviceName, lpDevMode, hwnd, dwflags, lParam);
}

// Detour GetRawInputData to disable input if ImGui open
UINT WINAPI DetourGetRawInputData(HRAWINPUT hRawInput, UINT uiCommand,
                                  LPVOID pData, PUINT pcbSize,
                                  UINT cbSizeHeader)
{
  if (SHOW_IMGUI)
  {
    if (pcbSize)
      *pcbSize = 0;

    return 0;
  }
  return fnGetRawInputData(hRawInput, uiCommand, pData, pcbSize,
                           cbSizeHeader);
}

LRESULT WINAPI WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (uMsg == WM_KEYDOWN && wParam == UI_OPEN_KEY)
  {
    SHOW_IMGUI = !SHOW_IMGUI;
    ImGuiIO &io = ImGui::GetIO();
    io.MouseDrawCursor = SHOW_IMGUI;
    return true;
  }

  (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam));

  return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

bool GetSwapChainPointers()
{
  Sleep(3000);

  WNDCLASSEXW wc = {
      sizeof(WNDCLASSEXW), CS_CLASSDC, DefWindowProc, 0, 0,
      GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
      L"DummyWindowClass", nullptr};
  RegisterClassExW(&wc);
  HWND hWnd = CreateWindowExW(
      0, L"DummyWindowClass", L"Dummy", WS_OVERLAPPEDWINDOW,
      0, 0, 1, 1, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
  ShowWindow(hWnd, SW_HIDE);
  UpdateWindow(hWnd);

  IDXGISwapChain *swapChain = nullptr;
  ID3D11Device *device = nullptr;
  DXGI_SWAP_CHAIN_DESC sd = {0};
  D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_0};

  sd.BufferCount = 1;
  sd.BufferDesc.Height = 1;
  sd.BufferDesc.Width = 1;
  sd.SampleDesc.Count = 1;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hWnd;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  HRESULT hr = D3D11CreateDeviceAndSwapChain(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevels,
      1, D3D11_SDK_VERSION, &sd, &swapChain,
      &device, nullptr, nullptr);

  if (FAILED(hr))
  {
    LogMessage(LogLevel::Error, true, "D3D11CreateDeviceAndSwapChain failed. HRESULT: 0x%08X", hr);
    DestroyWindow(hWnd);
    UnregisterClassW(L"DummyWindowClass", GetModuleHandle(nullptr));
    return false;
  }

  void **pVtable = *reinterpret_cast<void ***>(swapChain);
  pPresent = reinterpret_cast<present>(pVtable[8]);
  pResizeBuffers = reinterpret_cast<resizeBuffers>(pVtable[13]);

  swapChain->Release();
  device->Release();
  DestroyWindow(hWnd);
  UnregisterClassW(L"DummyWindowClass", GetModuleHandle(nullptr));

  LogMessage(LogLevel::Success, false, "Successfully retrieved swap chain function pointers.");
  return true;
}