#include <Windows.h>
#include <iostream>
#include <TlHelp32.h>
#include <string>
#include <Psapi.h>

const std::wstring PCSX2_PROC_NAME = L"pcsx2-qt.exe";

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

int main() {

	// ACTUALLY GETTING ALL PATH AND STUFF

	DWORD procID = GetProcIDByName(PCSX2_PROC_NAME); // gets procID from const str

	if (!procID) { // checks if empty
		std::cout << "PCSX2 PROCESS WAS NOT FOUND" << std::endl;
		return -1;
	}

	std::cout << "PCSX2 WAS FOUND" << std::endl;

	HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procID); // open process with all perms
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

	uintptr_t eeMemBase = 0;
	ReadProcessMemory(hProc, (LPVOID)eeMemRemoteAddress, &eeMemBase, sizeof(uintptr_t), nullptr); // reads process memory from PCSX2 itself

	// MAIN FUNCTION EDITING BEGINS HERE

	//    uintptr_t gameAddress =
	//        eememBase + 0x0033CD90;
	//
	//    int value = 0;
	//
	//    ReadProcessMemory(
	//        hProcess,
	//        (LPCVOID)gameAddress,
	//        &value,
	//        sizeof(value),
	//        nullptr
	//    );
	//
	//    std::cout << "Read Value: "
	//        << std::dec
	//        << value
	//        << "\n";



	return 0;

}