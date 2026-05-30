#pragma once

namespace ProcAttachSpace {
	class ProcAttachClass {
	public: 
		static int ProcAttach();
		static void SelectTrackAndLapsAndAssignValue(int number_of_laps, int selected_track_index, bool* start_loading_phase, bool* show_popup_success, bool* show_popup_fail);
		static void RandomizeAndSelectTrackAndAssignValue(bool* start_loading_phase, bool* show_popup_success, bool* show_popup_fail);
		static void InstantWinFunction();
		static void ChangeDriverMode(uint8_t driver_mode);
	private:
		static DWORD GetProcIDByName(const std::wstring procName);
	};
};