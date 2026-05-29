#include <Windows.h>
#include <shellapi.h>
#include <iostream>
#include <tchar.h>
#include <d3d11.h>
#include <dxgi.h>
#include <TlHelp32.h>
#include <string>
#include <Psapi.h>
#include <thread>

#include "ImGUI/imgui.h"
#include "ImGUI/imgui_impl_win32.h"
#include "ImGUI/imgui_impl_dx11.h"

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

// PROCESS ATTACH FUNCTIONS
const std::wstring PCSX2_PROC_NAME = L"pcsx2-qt.exe";
const char* TRACK_NAMES[64] = {
    "AUTUMN HILL FORWARD",
    "AUTUMN MOUNTAIN FORWARD",
    "VICTORIA GARDEN FORWARD",
    "VICTORIA ROAD FORWARD",
    "ROUTE de la SEINE FORWARD",
    "BURGENSCHLUCHT FORWARD",
    "WINTERTRAUM FORWARD",
    "MARCO STRADA WET FORWARD",
    "MARCO STRADA FORWARD",
    "TSUKUBA FORWARD",
    "TSUKUBA NIGHT FORWARD",
    "TSUKUBA WET FORWARD",
    "NORDSCHLEIFE FORWARD",
    "SAN FRANCHISCO FORWARD",
    "LOWENSEERING FORWARD",
    "REV CITY FORWARD",
    "SPEEDIAPOLIS RING",
    "MYSTIC CAVEWAY FORWARD",
    "EDGE OF THE ROAD FORWARD",
    "DRAGON RANGE UPHILL",
    "AIRPORT SQUARE",
    "OCEAN BRIDGE TA",
    "OCEAN BRIDGE RAIN TA",
    "MIRAGE #1",
    "MIRAGE #2",
    "MIRAGE #3",
    "MIRAGE #4",
    "MIRAGE #5",
    "MIRAGE #6",
    "MIRAGE #7",
    "MIRAGE #8",
    "MIRAGE #9",
    "MIRAGE #10",
    "MIRAGE #11",
    "MIRAGE #12",
    "MIRAGE #13",
    "MIRAGE #14",
    "MIRAGE #15",
    "MIRAGE #16",
    "COSMIC EGGWAY FORWARD",
    "COSMIC WINDING FORWARD",
    "WILD WEST ENDURO FORWARD",
    "AUTUMN HILL REVERSE",
    "AUTUMN MOUNTAIN REVERSE",
    "VICTORIA GARDEN REVERSE",
    "VICTORIA ROAD REVERSE",
    "ROUTE de la SEINE REVERSE",
    "BURGENSCHLUCHT REVERSE",
    "WINTERTRAUM REVERSE",
    "MARCO STRADA WET REVERSE",
    "MARCO STRADA REVERSE",
    "TSUKUBA REVERSE",
    "TSUKUBA NIGHT REVERSE",
    "TSUKUBA WET REVERSE",
    "NORDSCHLEIFE REVERSE",
    "SAN FRANCHISCO REVERSE",
    "LOWENSEERING REVERSE",
    "REV CITY REVERSE",
    "MYSTIC CAVEWAY REVERSE",
    "EDGE OF THE ROAD REVERSE",
    "DRAGON RANGE DOWNHILL",
    "COSMIC EGGWAY REVERSE",
    "COSMIC WINDING REVERSE",
    "WILD WEST ENDURO REVERSE (NO LIGHTING)",
};

const int TRACK_ID[64] = {
    1,   // AUTUMN HILL FORWARD
    2,   // AUTUMN MOUNTAIN FORWARD
    3,   // VICTORIA GARDEN FORWARD
    4,   // VICTORIA ROAD FORWARD
    5,   // ROUTE de la SEINE FORWARD
    7,   // BURGENSCHLUCHT FORWARD
    8,   // WINTERTRAUM FORWARD
    10,  // MARCO STRADA WET FORWARD
    11,  // MARCO STRADA FORWARD
    12,  // TSUKUBA FORWARD
    13,  // TSUKUBA NIGHT FORWARD
    14,  // TSUKUBA WET FORWARD
    15,  // NORDSCHLEIFE FORWARD
    16,  // SAN FRANCHISCO FORWARD
    17,  // LOWENSEERING FORWARD
    18,  // REV CITY FORWARD
    20,  // SPEEDIAPOLIS RING
    21,  // MYSTIC CAVEWAY FORWARD
    22,  // EDGE OF THE ROAD FORWARD
    23,  // DRAGON RANGE UPHILL
    26,  // AIRPORT SQUARE
    28,  // OCEAN BRIDGE TA
    29,  // OCEAN BRIDGE RAIN TA
    30,  // MIRAGE #1
    31,  // MIRAGE #2
    32,  // MIRAGE #3
    33,  // MIRAGE #4
    34,  // MIRAGE #5
    35,  // MIRAGE #6
    36,  // MIRAGE #7
    37,  // MIRAGE #8
    38,  // MIRAGE #9
    39,  // MIRAGE #10
    40,  // MIRAGE #11
    41,  // MIRAGE #12
    42,  // MIRAGE #13
    43,  // MIRAGE #14
    44,  // MIRAGE #15
    45,  // MIRAGE #16
    46,  // COSMIC EGGWAY FORWARD
    47,  // COSMIC WINDING FORWARD
    48,  // WILD WEST ENDURO FORWARD
    201, // AUTUMN HILL REVERSE
    202, // AUTUMN MOUNTAIN REVERSE
    203, // VICTORIA GARDEN REVERSE
    204, // VICTORIA ROAD REVERSE
    205, // ROUTE de la SEINE REVERSE
    207, // BURGENSCHLUCHT REVERSE
    208, // WINTERTRAUM REVERSE
    210, // MARCO STRADA WET REVERSE
    211, // MARCO STRADA REVERSE
    212, // TSUKUBA REVERSE
    213, // TSUKUBA NIGHT REVERSE
    214, // TSUKUBA WET REVERSE
    215, // NORDSCHLEIFE REVERSE
    216, // SAN FRANCHISCO REVERSE
    217, // LOWENSEERING REVERSE
    218, // REV CITY REVERSE
    221, // MYSTIC CAVEWAY REVERSE
    222, // EDGE OF THE ROAD REVERSE
    223, // DRAGON RANGE DOWNHILL
    246, // COSMIC EGGWAY REVERSE
    247, // COSMIC WINDING REVERSE
    248, // WILD WEST ENDURO REVERSE (NO LIGHTING)
};

static const int TRACK_COUNT = IM_COUNTOF(TRACK_NAMES);
static int selected_track_index = 0;
static int number_of_laps = 1;
static ImGuiTextFilter track_filter;

// ADDRESSES FOR DIFFERENT FUNCTIONS
const int TRACK_ADDRESS_EL = 0x01ab7c44;
const int TRACK_ADDRESS_OTHER = 0x01af7be4;

const int LAPS_ADDRESS_EL = 0x01ab7c50;
const int LAPS_ADDRESS_OTHER = 0x01af7bf0;

// PROCATTACH BEGINS HERE

uintptr_t eeMemBase = 0;
HANDLE hProc;


DWORD GetProcIDByName(const std::wstring procName) {
    DWORD prodID = 0; // init proc id


    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); // snapshot of current processes open

    PROCESSENTRY32W process; // object to hold information about a single proc
    process.dwSize = sizeof(process); // doesnt work without this

    if (Process32FirstW(snap, &process)) {
        do {
            if (!_wcsicmp(process.szExeFile, procName.c_str())) { // compares exe file names, from snapshot to provided name
                prodID = process.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &process)); // do loop till there is no longer another process in snapshot
    }
    CloseHandle(snap); // close handle, free from memory
    return prodID;
}

static void SelectTrackAndLapsAndAssignValue(bool* start_loading_phase, bool* show_popup_success, bool* show_popup_fail) {
    *start_loading_phase = true;
    bool do1 = false;
    bool do2 = false;
    bool do3 = false;
    bool do4 = false;
    ULONGLONG startTime = GetTickCount64();

    uintptr_t ingame_track_address_el = eeMemBase + TRACK_ADDRESS_EL;
    uintptr_t ingame_track_address_other = eeMemBase + TRACK_ADDRESS_OTHER;
    uintptr_t ingame_laps_address_el = eeMemBase + LAPS_ADDRESS_EL;
    uintptr_t ingame_laps_address_other = eeMemBase + LAPS_ADDRESS_OTHER;

    while (GetTickCount64() - startTime < 20000) {

        do1 = WriteProcessMemory(hProc, (LPVOID)ingame_track_address_el, &TRACK_ID[selected_track_index], sizeof(TRACK_ID[selected_track_index]), nullptr);
        do2 = WriteProcessMemory(hProc, (LPVOID)ingame_track_address_other, &TRACK_ID[selected_track_index], sizeof(TRACK_ID[selected_track_index]), nullptr);

        if (number_of_laps != 0) {
            do3 = WriteProcessMemory(hProc, (LPVOID)ingame_laps_address_el, &number_of_laps, sizeof(number_of_laps), nullptr);
            do4 = WriteProcessMemory(hProc, (LPVOID)ingame_laps_address_other, &number_of_laps, sizeof(number_of_laps), nullptr);
        }
        else {
            do3 = true;
            do4 = true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (do1 && do2 && do3 && do4) {
        std::cout << "SUCCESS" << std::endl;
        *start_loading_phase = false;
        *show_popup_success = true;
    }
    else {
        std::cout << "FAILURE" << std::endl;
        *start_loading_phase = false;
        *show_popup_fail = true;
    }
}

//static void SelectLapsAndAssignVariable(bool* start_loading_phase, bool* show_popup_success, bool* show_popup_fail) {
//    *start_loading_phase = true;
//    bool do1 = false;
//    bool do2 = false;
//    ULONGLONG startTime = GetTickCount64();
//
//    uintptr_t ingame_laps_address_el = eeMemBase + LAPS_ADDRESS_EL;
//    uintptr_t ingame_laps_address_other = eeMemBase + LAPS_ADDRESS_OTHER;
//
//    while (GetTickCount64() - startTime < 10000) {
//
//
//        do1 = WriteProcessMemory(hProc, (LPVOID)ingame_laps_address_el, &number_of_laps, sizeof(number_of_laps), nullptr);
//        do2 = WriteProcessMemory(hProc, (LPVOID)ingame_laps_address_other, &number_of_laps, sizeof(number_of_laps), nullptr);
//
//        std::this_thread::sleep_for(std::chrono::milliseconds(10));
//    }
//
//    if (do1 && do2) {
//        std::cout << "SUCCESS" << std::endl;
//        *start_loading_phase = false;
//        *show_popup_success = true;
//    }
//    else {
//        std::cout << "FAILURE" << std::endl;
//        *start_loading_phase = false;
//        *show_popup_fail = true;
//    }
//}

int ProcAttach() {

    // ACTUALLY GETTING ALL PATH AND STUFF

    DWORD procID = GetProcIDByName(PCSX2_PROC_NAME); // gets procID from const str

    if (!procID) { // checks if empty
        std::cout << "PCSX2 PROCESS WAS NOT FOUND" << std::endl;
        return -1;
    }

    std::cout << "PCSX2 WAS FOUND" << std::endl;

    hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procID); // open process with all perms
    HMODULE hMods[1024]; // array for modules that will come with PCSX2 process
    DWORD cbNeeded; // number of bytes

    EnumProcessModules(hProc, hMods, sizeof(hMods), &cbNeeded); // returns mods from proc

    wchar_t procPath[MAX_PATH];
    GetModuleFileNameExW(hProc, NULL, procPath, MAX_PATH); // returns path for pcsx2-qt.exe

    uintptr_t pcsx2BaseAddress = (uintptr_t)hMods[0]; // gets base address of the main process of PCSX2, that is the main .exe file

    HMODULE pcsx2LocalCopy = LoadLibraryExW(procPath, NULL, DONT_RESOLVE_DLL_REFERENCES); // makes local copy of .exe to work on

    if (!pcsx2LocalCopy) {
        std::cout << "LOCAL COPY COULD NOT BE MADE " << GetLastError() << std::endl;
        return -1;
    }

    uintptr_t eeMemExportAddress = (uintptr_t)GetProcAddress(pcsx2LocalCopy, "EEmem"); // gets export address of EE memory

    // main offset calculation part

    uintptr_t eeMemExportOffset = eeMemExportAddress - (uintptr_t)pcsx2LocalCopy; // basically offset of EE memory
    uintptr_t eeMemRemoteAddress = pcsx2BaseAddress + eeMemExportOffset; // finally finds real address of EE mem (usually cuz its a pointer)

    // get actual addresses

    eeMemBase = 0;
    ReadProcessMemory(hProc, (LPVOID)eeMemRemoteAddress, &eeMemBase, sizeof(uintptr_t), nullptr); // reads process memory from PCSX2 itself

    return 1;
}

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

static void ShowNoAttachProcPopup(bool* show_cant_use_popup) {
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

static void ShowMemorySetPopup(bool* show_memory_write_success_popup, bool* show_memory_write_fail_popup) {
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
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L,
                      hInstance, nullptr, nullptr, nullptr, nullptr,
                      _T("ImGuiBase"), nullptr };
    ::RegisterClassEx(&wc);

    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("Enthusia Randomizer"),
        WS_OVERLAPPEDWINDOW, 20, 20, 800, 360, // x, y, size_x, size_y
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
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // main IMGUI window here

    bool done = false;
    bool light_mode = false;
    int attached = 0;
    bool show_attached_popup = false;
    bool show_button_loading_icon = false;
    bool show_button_loading_icon_laps = false;
    bool show_memory_write_success_popup = false;
    bool show_memory_write_fail_popup = false;
    bool show_cant_use_popup = false;

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
        //ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);

        const char* track_name_selected = TRACK_NAMES[selected_track_index];
        ImGui::Begin("Enthusia Randomizer", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("App"))
            {
                if (ImGui::MenuItem("Attach To PCSX2")) {
                    attached = ProcAttach();
                    show_attached_popup = true;
                }
                if (ImGui::MenuItem("Toggle Theme")) {
                    ToggleImGuiTheme(&light_mode);
                }
                if (ImGui::MenuItem("GitHub")) {
                    ShellExecute(0, 0, L"https://github.com/real-xp/", 0, 0, SW_SHOW);
                }
                if (ImGui::MenuItem("Exit")) {
                    exit(0);
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }
        
        ShowAttachPopup(&show_attached_popup, attached);

        ImGui::Dummy(ImVec2(0, 20));
        ImGui::PushFont(NULL, 42);
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
                if (!track_filter.PassFilter(TRACK_NAMES[i]))
                    continue;

                const bool isSelected = (selected_track_index == i);
                if (ImGui::Selectable(TRACK_NAMES[i], isSelected))
                    selected_track_index = i;

                // Keep selected item visible when opening
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        //ImGui::SameLine();
        ImGui::Text("Select Laps ");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderInt("##SelectLaps", &number_of_laps, 0, 200, "%d", 0);
        //ImGui::SameLine();
        //ImGui::PushItemFlag(ImGuiItemFlags_Disabled, show_button_loading_icon_laps);
        //if (ImGui::Button(show_button_loading_icon_laps ? "Working...##Laps" : "Set##Laps", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        //    if (attached == 1) {
        //        std::jthread(SelectLapsAndAssignVariable, &show_button_loading_icon_laps, &show_memory_write_success_popup, &show_memory_write_fail_popup).detach();
        //    }
        //    else {
        //        show_cant_use_popup = true;
        //    }
        //}

        //ImGui::PopItemFlag();

        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, show_button_loading_icon);
        if (ImGui::Button(show_button_loading_icon ? "Working...##Track" : "Set##Track", ImVec2(ImGui::GetContentRegionAvail().x, 30))) {
            if (attached == 1) {
                std::jthread(SelectTrackAndLapsAndAssignValue, &show_button_loading_icon, &show_memory_write_success_popup, &show_memory_write_fail_popup).detach();
            }
            else {
                show_cant_use_popup = true;
            }
        }
        ImGui::PopItemFlag();

        ShowMemorySetPopup(&show_memory_write_success_popup, &show_memory_write_fail_popup);
        ShowNoAttachProcPopup(&show_cant_use_popup);

        ImGui::Dummy(ImVec2(0, 20));
        ImGui::Button("Randomize Track##TrackRandom", ImVec2(ImGui::GetContentRegionAvail().x, 50));
        //ImGui::SameLine();
        //ImGui::Button("Randomize Laps (1 to 10)##TrackRandom", ImVec2(ImGui::GetContentRegionAvail().x, 50));

        ImGui::End();
        ImGui::Render();

        const float clear_color[4] = { 0.29f, 0.29f, 0.29f, 1.0f }; // DX11 Window Color
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
