#define IMGUI_DISABLE_DEFAULT_FONT

#include "gui.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "offset.h"
#include "patches.h"
#include "roboto_regular_data.h"
#include "utils.h"

HWND window = nullptr;
ID3D11Device *pDevice = nullptr;
ID3D11DeviceContext *pContext = nullptr;
ID3D11RenderTargetView *mainRenderTargetView = nullptr;
static bool init = false;
static ImFont *statsRoboto;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg,
                                              WPARAM wParam, LPARAM lParam);

void CreateRenderTarget(IDXGISwapChain *pSwapchain)
{
  ID3D11Texture2D *pBackBuffer;
  pSwapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID *)&pBackBuffer);
  pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
  pBackBuffer->Release();
}

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

  return pResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat,
                        SwapChainFlags);
}

void CreateImGuiLayout()
{
  ImGuiIO &io = ImGui::GetIO();

  const int totalDeaths = player_deaths_addr ? *(const int *)player_deaths_addr : 0;
  const int totalKills = total_kills_addr ? *(const int *)total_kills_addr : 0;

  // Helper lambda to update the INI setting and optionally apply a patch function.
  auto updateSetting = [&](const char *key, auto value, void (*patchFunc)() = nullptr)
  {
    SetIniValue(key, value);
    RefreshIniValues();
    if (patchFunc)
      patchFunc();
  };

  io.MouseDrawCursor = true;

  ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoResize);

  // --- FPS Controls (disabled when in fullscreen) ---
  ImGui::BeginDisabled(FULLSCREEN_STATE);
  if (ImGui::Checkbox("Enable FPS Unlock", reinterpret_cast<bool *>(&FPSUNLOCK_ENABLED)))
    updateSetting("EnableFpsUnlock", FPSUNLOCK_ENABLED, ApplyFramelockPatch);

  if (FULLSCREEN_STATE && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("This option is disabled because the game is in fullscreen state, change to windowed!");

  ImGui::SameLine();
  ImGui::SetNextItemWidth(100);
  if (ImGui::InputInt("##fpslimit", &FPS_LIMIT, 1, 10))
    updateSetting("FpsLimit", FPS_LIMIT, ApplyFramelockPatch);

  // --- Borderless windowed toggle (disabled when in fullscreen) ---
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
    updateSetting("EnablePlayerTimescale", PLAYER_TIMESCALE_ENABLED);

  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("NOT SAFE, USE WITH CAUTION!");

  ImGui::SameLine();
  ImGui::BeginDisabled(!PLAYER_TIMESCALE_ENABLED);
  ImGui::SetNextItemWidth(200);
  if (ImGui::SliderFloat("##playertimescalevalue", &PLAYER_TIMESCALE_VALUE, 0.1, 10, "%.2f", ImGuiSliderFlags_AlwaysClamp))
    updateSetting("ValuePlayerTimescale", PLAYER_TIMESCALE_VALUE);

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
  if (ImGui::Checkbox("Show Player Deaths", reinterpret_cast<bool *>(&SHOW_PLAYER_DEATHSKILLS_ENABLED)))
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
  ImGui::Text("Deaths: %d", totalDeaths);
  ImGui::Text("Kills: %d", (totalKills - totalDeaths));

  ImGui::End();
}

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
      return pPresent(pSwapChain, syncInterval, flags);
  }

  DXGI_SWAP_CHAIN_DESC sd;
  pSwapChain->GetDesc(&sd);
  pSwapChain->GetFullscreenState((BOOL *)&FULLSCREEN_STATE, nullptr);

  int totalDeaths = player_deaths_addr ? *(const int *)player_deaths_addr : 0;
  int totalKills = total_kills_addr ? *(const int *)total_kills_addr : 0;

  ImGui_ImplDX11_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = nullptr;
  ImVec2 displaySize = ImGui::GetIO().DisplaySize;
  io.MouseDrawCursor = false;
  ImGui::SetNextWindowPos(
      ImVec2((sd.BufferDesc.Width * 0.35f), sd.BufferDesc.Height * 0.5f),
      ImGuiCond_Always, ImVec2(0.5f, 0.5f));

  if (SHOW_IMGUI)
    CreateImGuiLayout();

  if (SHOW_PLAYER_DEATHSKILLS_ENABLED)
  {
    ImDrawList *draw_list = ImGui::GetForegroundDrawList();
    char textBuffer[128];
    snprintf(textBuffer, sizeof(textBuffer), "Deaths: %d\nKills: %d", totalDeaths, (totalKills - totalDeaths));
    draw_list->AddText(statsRoboto, PLAYER_DEATHSKILLS_FZ, ImVec2(PLAYER_DEATHSKILLS_X, PLAYER_DEATHSKILLS_Y), IM_COL32(255, 255, 255, 255), textBuffer);
  }

  ImGui::EndFrame();
  ImGui::Render();
  pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

  return pPresent(pSwapChain, syncInterval, flags);
}

BOOL WINAPI HookedSetCursorPos(int X, int Y)
{
  if (SHOW_IMGUI)
  {
    return 0;
  }
  return g_OriginalSetCursorPos(X, Y);
}

UINT WINAPI HookedGetRawInputData(HRAWINPUT hRawInput, UINT uiCommand,
                                  LPVOID pData, PUINT pcbSize,
                                  UINT cbSizeHeader)
{
  if (SHOW_IMGUI)
  {
    if (pcbSize)
      *pcbSize = 0;

    return 0;
  }
  return g_OriginalGetRawInputData(hRawInput, uiCommand, pData, pcbSize,
                                   cbSizeHeader);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (uMsg == WM_KEYDOWN && wParam == VK_HOME)
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
  HWND hWnd;
  while (!(hWnd = GetCurrentProcessWindow()))
    Sleep(10);

  IDXGISwapChain *swapChain = nullptr;
  ID3D11Device *device = nullptr;
  DXGI_SWAP_CHAIN_DESC sd = {0};
  const D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_0};

  sd.BufferCount = 1;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hWnd;
  sd.SampleDesc.Count = 1;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(
          nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevels, 1,
          D3D11_SDK_VERSION, &sd, &swapChain, &device, nullptr, nullptr)))
  {
    void **pVtable = *reinterpret_cast<void ***>(swapChain);

    pPresentTarget = reinterpret_cast<present>(pVtable[8]);
    pResizeBuffersTarget = reinterpret_cast<resizeBuffers>(pVtable[13]);

    swapChain->Release();
    device->Release();
    return true;
  }

  ErrorLog(false, "Failed to get swapchain pointers!");
  return false;
}
