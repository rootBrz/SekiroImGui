#define IMGUI_DISABLE_DEFAULT_FONT

#include "gui.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
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
  pSwapChain->GetFullscreenState(&FULLSCREEN_STATE, nullptr);
  int iTotalDeaths = player_deaths_addr
                         ? *reinterpret_cast<const int *>(player_deaths_addr)
                         : 0;
  int iTotalKills =
      total_kills_addr ? *reinterpret_cast<const int *>(total_kills_addr) : 0;

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
  {
    io.MouseDrawCursor = true;
    ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoResize);
    ImGui::BeginDisabled(FULLSCREEN_STATE);
    // FPS CONTROLS
    if (ImGui::Checkbox("Enable FPS Unlock",
                        reinterpret_cast<bool *>(&FPSUNLOCK_ENABLED)))
    {
      SetIniValue("EnableFpsUnlock", FPSUNLOCK_ENABLED);
      RefreshIniValues();
      ApplyFramelockPatch();
    }
    if (FULLSCREEN_STATE &&
        ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
      ImGui::SetTooltip("This option is disabled because the game in "
                        "fullscreen state, change to windowed!");
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputInt("##fpslimit", &FPS_LIMIT, 1, 10))
    {
      SetIniValue("FpsLimit", FPS_LIMIT);
      RefreshIniValues();
      ApplyFramelockPatch();
    }

    // BORDERLESS
    if (ImGui::Checkbox("Enable Borderless Windowed",
                        (bool *)&BORDERLESS_ENABLED))
    {
      SetIniValue("EnableBorderlessFullscreen", BORDERLESS_ENABLED);
      RefreshIniValues();
      ApplyBorderlessPatch();
    }
    if (FULLSCREEN_STATE &&
        ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
      ImGui::SetTooltip("This option is disabled because the game in "
                        "fullscreen state, change to windowed!");
    }
    ImGui::EndDisabled();

    // AUTOLOOT
    if (ImGui::Checkbox("Enable Autoloot", (bool *)&AUTOLOOT_ENABLED))
    {
      SetIniValue("EnableAutoloot", AUTOLOOT_ENABLED);
      RefreshIniValues();
      ApplyAutolootPatch();
    }

    // LOGOS/INTRO
    if (ImGui::Checkbox("Enable Logo Skip", (bool *)&INTROSKIP_ENABLED))
    {
      SetIniValue("EnableIntroSkip", INTROSKIP_ENABLED);
      RefreshIniValues();
    }

    // FOV
    if (ImGui::Checkbox("FOV", (bool *)&FOV_ENABLED))
    {
      SetIniValue("EnableFOV", FOV_ENABLED);
      RefreshIniValues();
      ApplyFovPatch();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!FOV_ENABLED);
    if (ImGui::SliderInt("##fovvalue", &FOV_VALUE, -95, 95))
    {
      SetIniValue("ValueFOV", FOV_VALUE);
      RefreshIniValues();
      ApplyFovPatch();
    }
    ImGui::EndDisabled();

    // RESOLUTION
    if (ImGui::Checkbox("Enable Custom Resolution",
                        (bool *)&CUSTOM_RES_ENABLED))
    {
      SetIniValue("EnableCustomRes", CUSTOM_RES_ENABLED);
      RefreshIniValues();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      ImGui::SetTooltip(
          "Forces custom resolution, REQUIRES RESTART TO ENABLE/DISABLE!!!");

    ImGui::BeginDisabled(!CUSTOM_RES_ENABLED);
    ImGui::SetNextItemWidth(110);
    if (ImGui::InputInt("##customreswidth", &CUSTOM_RES_WIDTH))
    {
      SetIniValue("CustomResWidth", CUSTOM_RES_WIDTH);
      RefreshIniValues();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      ImGui::SetTooltip(
          "Forces custom resolution, REQUIRES RESTART TO ENABLE/DISABLE!!!");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(110);
    if (ImGui::InputInt("##customresheight", &CUSTOM_RES_HEIGHT))
    {
      SetIniValue("CustomResHeight", CUSTOM_RES_HEIGHT);
      RefreshIniValues();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      ImGui::SetTooltip(
          "Forces custom resolution, REQUIRES RESTART TO ENABLE/DISABLE!!!");
    ImGui::EndDisabled();

    // PLAYER DEATHS AND KILLS
    if (ImGui::Checkbox("Show Player Deaths",
                        (bool *)&SHOW_PLAYER_DEATHSKILLS_ENABLED))
    {
      SetIniValue("EnableShowPlayerDeathsKills",
                  SHOW_PLAYER_DEATHSKILLS_ENABLED);
      RefreshIniValues();
    };
    ImGui::SetNextItemWidth(200);
    if (ImGui::SliderInt("Stats Text X", &PLAYER_DEATHSKILLS_X, 0, io.DisplaySize.x))
    {
      SetIniValue("PlayerDeathsKillsX",
                  PLAYER_DEATHSKILLS_X);
      RefreshIniValues();
    }

    ImGui::SetNextItemWidth(200);
    if (ImGui::SliderInt("Stats Text Y", &PLAYER_DEATHSKILLS_Y, 0, io.DisplaySize.y))
    {
      SetIniValue("PlayerDeathsKillsY",
                  PLAYER_DEATHSKILLS_Y);
      RefreshIniValues();
    }

    ImGui::SetNextItemWidth(200);
    if (ImGui::SliderInt("Stats Font Size", &PLAYER_DEATHSKILLS_FZ, 16, 64))
    {
      SetIniValue("PlayerDeathsKillsFZ",
                  PLAYER_DEATHSKILLS_FZ);
      RefreshIniValues();
    }

    ImGui::Text("Deaths: %d", iTotalDeaths);
    ImGui::Text("Kills: %d", (iTotalKills - iTotalDeaths));

    ImGui::End();
  }

  if (SHOW_PLAYER_DEATHSKILLS_ENABLED)
  {
    ImDrawList *draw_list = ImGui::GetForegroundDrawList();
    char textBuffer[128];
    snprintf(textBuffer, sizeof(textBuffer), "Deaths: %d\nKills: %d", iTotalDeaths, (iTotalKills - iTotalDeaths));
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
  HWND hWnd = GetCurrentProcessWindow();
  while (!hWnd)
    hWnd = GetCurrentProcessWindow();

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

  ErrorLog("Failed to get swapchain pointers!");
  return false;
}
