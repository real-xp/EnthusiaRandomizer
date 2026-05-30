#include <Windows.h>
#include <iostream>
#include <thread>
#include <Psapi.h>
#include <TlHelp32.h>
#include "variables.h"
#include "procattach.h"

// PROCESS ATTACH FUNCTIONS
const std::wstring PCSX2_PROC_NAME = L"pcsx2-qt.exe";

// PROCATTACH BEGINS HERE

uintptr_t eeMemBase = 0;
HANDLE hProc;

DWORD ProcAttachSpace::ProcAttachClass::GetProcIDByName(const std::wstring procName) {
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

void ProcAttachSpace::ProcAttachClass::SelectTrackAndLapsAndAssignValue(int number_of_laps, int selected_track_index, bool* start_loading_phase, bool* show_popup_success, bool* show_popup_fail) {
    *start_loading_phase = true;
    bool do1 = false;
    bool do2 = false;
    bool do3 = false;
    uint8_t number_of_laps_interal = (uint8_t)number_of_laps;
    ULONGLONG start_time = GetTickCount64();

    uintptr_t ingame_track_address_el = eeMemBase + RandomizerVariables::TRACK_ADDRESS_EL;
    uintptr_t ingame_track_address_other = eeMemBase + RandomizerVariables::TRACK_ADDRESS_OTHER;
    uintptr_t ingame_laps_address_el = eeMemBase + RandomizerVariables::LAPS_ADDRESS_EL;

    while (GetTickCount64() - start_time < 30000) {

        do1 = WriteProcessMemory(hProc, (LPVOID)ingame_track_address_el, &RandomizerVariables::TRACK_ID[selected_track_index], sizeof(RandomizerVariables::TRACK_ID[selected_track_index]), nullptr);
        do2 = WriteProcessMemory(hProc, (LPVOID)ingame_track_address_other, &RandomizerVariables::TRACK_ID[selected_track_index], sizeof(RandomizerVariables::TRACK_ID[selected_track_index]), nullptr);

        if (number_of_laps_interal != 0) {
            do3 = WriteProcessMemory(hProc, (LPVOID)ingame_laps_address_el, &number_of_laps_interal, sizeof(number_of_laps_interal), nullptr);
        }
        else {
            do3 = true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (do1 && do2 && do3) {
        std::cout << "SUCCESS" << std::endl;
        *show_popup_success = true;
    }
    else {
        std::cout << "FAILURE" << std::endl;
        *show_popup_fail = true;
    }
    *start_loading_phase = false;
}

void ProcAttachSpace::ProcAttachClass::RandomizeAndSelectTrackAndAssignValue(bool* start_loading_phase, bool* show_popup_success, bool* show_popup_fail) {
    *start_loading_phase = true;
    bool do1 = false;
    bool do2 = false;

    uintptr_t ingame_track_address_el = eeMemBase + RandomizerVariables::TRACK_ADDRESS_EL;
    uintptr_t ingame_track_address_other = eeMemBase + RandomizerVariables::TRACK_ADDRESS_OTHER;

    uint8_t rand_track_id_val = (uint8_t)1; // fallback value

    for (size_t i = 0; i < 6; i++) // basically so mirage occurs less in 6 tries
    {
        rand_track_id_val = (uint8_t)std::rand() % 64;

        if (!(rand_track_id_val >= 30 && rand_track_id_val <= 46)) {
            break;
        }
    }

    ULONGLONG start_time = GetTickCount64();

    while (GetTickCount64() - start_time < 30000) {

        do1 = WriteProcessMemory(hProc, (LPVOID)ingame_track_address_el, &RandomizerVariables::TRACK_ID[rand_track_id_val], sizeof(RandomizerVariables::TRACK_ID[rand_track_id_val]), nullptr);
        do2 = WriteProcessMemory(hProc, (LPVOID)ingame_track_address_other, &RandomizerVariables::TRACK_ID[rand_track_id_val], sizeof(RandomizerVariables::TRACK_ID[rand_track_id_val]), nullptr);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (do1 && do2) {
        std::cout << "SUCCESS" << std::endl;
        *show_popup_success = true;
    }
    else {
        std::cout << "FAILURE" << std::endl;
        *show_popup_fail = true;
    }
    *start_loading_phase = false;
}

void ProcAttachSpace::ProcAttachClass::InstantWinFunction() {
    uintptr_t ingame_instantwin_address_el = eeMemBase + RandomizerVariables::INSTANT_WIN_EL;
    uint8_t instant_win_value = (uint8_t)4;

    WriteProcessMemory(hProc, (LPVOID)ingame_instantwin_address_el, &instant_win_value, sizeof(instant_win_value), nullptr);
}

void ProcAttachSpace::ProcAttachClass::ChangeDriverMode(uint8_t driver_mode) {
    uintptr_t ingame_driver_mode_address_el = eeMemBase + RandomizerVariables::DRIVER_MODE_EL;
    uint8_t mode_value = (uint8_t)driver_mode;

    WriteProcessMemory(hProc, (LPVOID)ingame_driver_mode_address_el, &mode_value, sizeof(mode_value), nullptr);
}

int ProcAttachSpace::ProcAttachClass::ProcAttach() {

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
