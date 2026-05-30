#include <Windows.h>
#include <shellapi.h>
#include <tchar.h>
#include <d3d11.h>
#include <dxgi.h>
#include <thread>
#include <atomic>

#include "ImGUI/imgui.h"
#include "ImGUI/imgui_impl_win32.h"
#include "ImGUI/imgui_impl_dx11.h"

#include "ProcAttach/variables.h"
#include "ProcAttach/procattach.h"
#include "Assets/font/rubik-font.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
    
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static void CreateRenderTarget();
static void CleanupRenderTarget();
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static const int TRACK_COUNT = IM_COUNTOF(RandomizerVariables::TRACK_NAMES);
static int selected_track_index = 0;
static int number_of_laps = 1;
static ImGuiTextFilter track_filter;

static void ShowAttachPopup(bool* show_attached_popup, int attached) {
    if (*show_attached_popup && attached == 1) {
        ImGui::OpenPopup("Successfully Attached");
        *show_attached_popup = false;
    }
    if (*show_attached_popup && attached == -1) {
        ImGui::OpenPopup("Could Not Attach");
        *show_attached_popup = false;
    }

    if (ImGui::BeginPopupModal("Successfully Attached", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::Text("Successfully attached to PCSX2");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Could Not Attach", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::Text("Could not attach to PCSX2. Make sure it is running.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

static void ShowNoAttachProcPopup(std::atomic_bool* show_cant_use_popup) {
    if (*show_cant_use_popup) {
        ImGui::OpenPopup("Cannot Set");
        *show_cant_use_popup = false;
    }
    if (ImGui::BeginPopupModal("Cannot Set", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::Text("Cannot set property. PCSX2 has not been attached\nPlease attach PCSX2 by going to App > Attach To PCSX2");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

static void ShowMemorySetPopup(std::atomic_bool* show_memory_write_success_popup, std::atomic_bool* show_memory_write_fail_popup) {
    if (*show_memory_write_success_popup) {
        ImGui::OpenPopup("Successful");
        *show_memory_write_success_popup = false;
    }

    if (*show_memory_write_fail_popup) {
        ImGui::OpenPopup("Failed");
        *show_memory_write_fail_popup = false;
    }

    if (ImGui::BeginPopupModal("Successful", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::Text("Successfully set the value. Enjoy the game!");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Failed", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::Text("Could not set the value. Make sure PCSX2 is running and not paused.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

static void ToggleImGuiTheme(bool* is_dark_mode) {
    *is_dark_mode ? ImGui::StyleColorsDark() : ImGui::StyleColorsLight();
    *is_dark_mode = !*is_dark_mode;
}

int APIENTRY main(HINSTANCE hInstance, HINSTANCE, LPTSTR, int)
{
    std::srand((int)std::time(NULL)); // set random seed
    ProcAttachSpace::ProcAttachClass ProcessAttach;

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L,
                      hInstance, nullptr, nullptr, nullptr, nullptr,
                      _T("ImGuiBase"), nullptr };
    ::RegisterClassEx(&wc);

    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("Enthusia Randomizer"),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, 20, 20, 800, 360, // x, y, size_x, size_y
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking

    ImGuiStyle& style = ImGui::GetStyle();
    style.FontSizeBase = 16.0f;

    io.IniFilename = NULL;
    io.Fonts->AddFontFromMemoryCompressedTTF(rubikfont_compressed_data, rubikfont_compressed_size);

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // main IMGUI window here

    bool done = false;
    bool light_mode = false;
    int attached = 0;
    bool show_attached_popup = false;
    std::atomic_bool show_button_loading_icon = false;
    std::atomic_bool show_button_loading_icon_randomzie = false;
    std::atomic_bool show_memory_write_success_popup = false;
    std::atomic_bool show_memory_write_fail_popup = false;
    std::atomic_bool show_cant_use_popup = false;

    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);

        const char* track_name_selected = RandomizerVariables::TRACK_NAMES[selected_track_index];
        ImGui::Begin("Enthusia Randomizer", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("App"))
            {
                if (ImGui::MenuItem("Attach To PCSX2")) {
                    attached = ProcessAttach.ProcAttach();
                    show_attached_popup = true;
                }
                if (ImGui::MenuItem("Toggle Theme")) ToggleImGuiTheme(&light_mode);
                if (ImGui::MenuItem("Made By real-xp")) ShellExecute(0, 0, L"https://github.com/real-xp/", 0, 0, SW_SHOW);
                if (ImGui::MenuItem("Exit")) exit(0);
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Help")) ShellExecute(0, 0, L"https://github.com/real-xp/EnthusiaTrackRandomizer/blob/master/README.md", 0, 0, SW_SHOW);
            ImGui::EndMenuBar();
        }
        
        ShowAttachPopup(&show_attached_popup, attached);

        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, show_button_loading_icon || show_button_loading_icon_randomzie);
        ImGui::Dummy(ImVec2(0, 20));
        ImGui::PushFont(NULL, 39);
        ImGui::Text("Enthusia Randomizer", ImGui::GetFontSize());
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, 20));

        // MANUAL SELECT TRACK

        ImGui::Text("Select Track");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##SelectTrack", track_name_selected, 0))
        {
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
                track_filter.Clear();
            }
            ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F);
            track_filter.Draw("##Filter", -FLT_MIN);
            for (int i = 0; i < TRACK_COUNT; i++)
            {
                if (!track_filter.PassFilter(RandomizerVariables::TRACK_NAMES[i]))
                    continue;

                const bool isSelected = (selected_track_index == i);
                if (ImGui::Selectable(RandomizerVariables::TRACK_NAMES[i], isSelected)) selected_track_index = i;

                // Keep selected item visible when opening
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::Text("Select Laps ");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderInt("##SelectLaps", &number_of_laps, 0, 200, "%d", 0);

        ImGui::Dummy(ImVec2(0, 8));

        if (ImGui::Button(show_button_loading_icon ? "Working...##Track" : "Set Track And Laps##Track", ImVec2(ImGui::GetContentRegionAvail().x, 25))) {
            if (attached == 1)
                std::jthread(ProcessAttach.SelectTrackAndLapsAndAssignValue, number_of_laps, selected_track_index, &show_button_loading_icon, &show_memory_write_success_popup, &show_memory_write_fail_popup).detach();
            else
                show_cant_use_popup = true;
        }

        ShowMemorySetPopup(&show_memory_write_success_popup, &show_memory_write_fail_popup);
        ShowNoAttachProcPopup(&show_cant_use_popup);

        ImGui::PopItemFlag();


        ImGui::Dummy(ImVec2(0, 8));


        if (ImGui::Button("Instant Win##InstantWin", ImVec2(io.DisplaySize.x / 4, 25))) { if (attached == 1) ProcessAttach.InstantWinFunction(); else show_cant_use_popup = true;}
        ImGui::SameLine();
        if (ImGui::Button("Debug Car##DebugCar", ImVec2(io.DisplaySize.x / 4, 25))) { if (attached == 1) ProcessAttach.ChangeDriverMode(6); else show_cant_use_popup = true; }
        ImGui::SameLine();
        if (ImGui::Button("AIRS##AIRS", ImVec2(io.DisplaySize.x / 4, 25))) { if (attached == 1) ProcessAttach.ChangeDriverMode(3); else show_cant_use_popup = true;}
        ImGui::SameLine();
        if (ImGui::Button("Player Car##PlayerCar", ImVec2(ImGui::GetContentRegionAvail().x, 25))) { if (attached == 1) ProcessAttach.ChangeDriverMode(5); else show_cant_use_popup = true;}

        ImGui::Dummy(ImVec2(0, 8));

        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, show_button_loading_icon || show_button_loading_icon_randomzie);

        ImGui::PushFont(NULL, 26);
        if (ImGui::Button(show_button_loading_icon_randomzie ? "WORKING...##TrackRandom" : "RANDOMIZE TRACK##TrackRandom", ImVec2(ImGui::GetContentRegionAvail().x, 50))) {
            if (attached == 1)
                std::jthread(ProcessAttach.RandomizeAndSelectTrackAndAssignValue, &show_button_loading_icon_randomzie, &show_memory_write_success_popup, &show_memory_write_fail_popup).detach();
            else
                show_cant_use_popup = true;
        }
        ImGui::PopFont();
        ImGui::PopItemFlag();

        ImGui::End();   
        ImGui::Render();

        const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // DX11 Window Color
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

static bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0
    };

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION,
        &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext
    );

    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

static void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer)
    {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

static void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;

    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
