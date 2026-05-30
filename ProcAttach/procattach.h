#pragma once
#include <string>
#include <atomic>

namespace ProcAttachSpace {
	class ProcAttachClass {
	public: 
		static int ProcAttach();
		static void SelectTrackAndLapsAndAssignValue(int number_of_laps, int selected_track_index, std::atomic_bool* start_loading_phase, std::atomic_bool* show_popup_success, std::atomic_bool* show_popup_fail);
		static void RandomizeAndSelectTrackAndAssignValue(std::atomic_bool* start_loading_phase, std::atomic_bool* show_popup_success, std::atomic_bool* show_popup_fail);
		static void InstantWinFunction();
		static void ChangeDriverMode(uint8_t driver_mode);
	private:
		static DWORD GetProcIDByName(const std::wstring procName);
	};
};