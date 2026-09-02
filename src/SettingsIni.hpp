#pragma once

#include "DataHandler.hpp"

namespace SettingsIni
{
	// Initialization & default values
	inline int  iVerboseMode = 1;
	inline bool bAsynchronousStartup = true;

	// Quest Filters
	inline bool bQuestEnable = true;
	inline bool bQuestUseCache = true;
	inline int  iQuestPapyrusFilterMode = 2;
	inline bool bQuestRequireLinkedQuest = false;
	inline int  iQuestLinkedQuestFilter = 3;

	// Quest UI - Paths, Functions & Settings
	inline std::string sQuestUIQuestColorVariable = "_root.DialogueMenu_mc.TopicList.iQuestColor";
	inline std::string sQuestUIQuestIconVisibleVariable = "_root.DialogueMenu_mc.TopicList.bQuestIconVisible";
	inline std::string sQuestUISetEntryTextFunction = "_root.DialogueMenu_mc.TopicList.SetEntryText";
	inline std::string sQuestUISetEntryTextCustomFunction = "SetEntryTextCustom";
	inline std::string sQuestUIQuestMarkerClip = "QuestMarker";
	inline bool bQuestUIOverrideUISettings = false;
	inline int iQuestUIDialogueSortMode = 0;
	inline bool bQuestUIDisplayIcon = true;
	inline int cQuestUIQuestEntryColor = 0xFFD966;

	// Dialogue Skip
	inline bool bSkipImmediateOnInput = true;
	inline bool bSkipHoldToSkip = true;
	inline int  iSkipHoldDuration = 500;
	inline int  iSkipAutoSkipInterval = 150;
	inline bool bSkipOnInteraction = false;
	inline bool bSkipDBVOIntegration = true;
	inline int  iSkipPapyrusHandle = 3;

	// Dialogue Pause
	inline bool bPauseEnabled = true;
	inline int  iPauseMinDelay = 200;
	inline int  iPauseMaxDelay = 800;

	class SettingsManager
	{
	public:
		SettingsManager()
		{
			bindings = {
				// General
				{ "General", "iVerboseMode", &iVerboseMode },
				{ "General", "bAsynchronousStartup", &bAsynchronousStartup },

				// Quest Filters
				{ "QuestFilters", "bEnable", &bQuestEnable },
				{ "QuestFilters", "bUseCache", &bQuestUseCache },
				{ "QuestFilters", "iPapyrusFilterMode", &iQuestPapyrusFilterMode },
				{ "QuestFilters", "bRequireLinkedQuest", &bQuestRequireLinkedQuest },
				{ "QuestFilters", "iLinkedQuestFilter", &iQuestLinkedQuestFilter },

				// Quest UI
				{ "QuestUI", "sQuestColorVariable", &sQuestUIQuestColorVariable },
				{ "QuestUI", "sQuestIconVisibleVariable", &sQuestUIQuestIconVisibleVariable },
				{ "QuestUI", "sSetEntryTextFunction", &sQuestUISetEntryTextFunction },
				{ "QuestUI", "sSetEntryTextCustomFunction", &sQuestUISetEntryTextCustomFunction },
				{ "QuestUI", "sQuestMarkerClip", &sQuestUIQuestMarkerClip },
				{ "QuestUI", "bOverrideUISettings", &bQuestUIOverrideUISettings },
				{ "QuestUI", "iDialogueSortMode", &iQuestUIDialogueSortMode },
				{ "QuestUI", "bDisplayIcon", &bQuestUIDisplayIcon },
				{ "QuestUI", "cQuestEntryColor", &cQuestUIQuestEntryColor },

				// Dialogue Skip
				{ "DialogueSkip", "bSkipImmediateOnInput", &bSkipImmediateOnInput },
				{ "DialogueSkip", "bHoldToSkip", &bSkipHoldToSkip },
				{ "DialogueSkip", "iHoldDuration", &iSkipHoldDuration },
				{ "DialogueSkip", "iAutoSkipInterval", &iSkipAutoSkipInterval },
				{ "DialogueSkip", "bSkipOnInteraction", &bSkipOnInteraction },
				{ "DialogueSkip", "bDBVOIntegration", &bSkipDBVOIntegration },
				{ "DialogueSkip", "iPapyrusHandle", &iSkipPapyrusHandle },

				// Dialogue Pause
				{ "DialoguePause", "bEnabled", &bPauseEnabled },
				{ "DialoguePause", "iMinDelay", &iPauseMinDelay },
				{ "DialoguePause", "iMaxDelay", &iPauseMaxDelay }
			};

			questUIBindings = {
				{ "sQuestColorVariable", &sQuestUIQuestColorVariable },
				{ "sQuestIconVisibleVariable", &sQuestUIQuestIconVisibleVariable },
				{ "sUpdateEntriesFunction", &sQuestUISetEntryTextFunction },
				{ "sSetEntryTextCustomFunction", &sQuestUISetEntryTextCustomFunction },
				{ "sQuestMarkerClip", &sQuestUIQuestMarkerClip },
				{ "iDialogueSortMode", &iQuestUIDialogueSortMode },
				{ "bDisplayIcon", &bQuestUIDisplayIcon },
				{ "cQuestEntryColor", &cQuestUIQuestEntryColor }
			};
		}

		bool ReadSettings()
		{
			std::wstring wpath_str(path.begin(), path.end());
			const wchar_t* wpath = wpath_str.c_str();

			bool readStatus = false;

			logger::info("Trying to read INI file at path: {}", path);

			if (std::filesystem::exists(path)) {
				CSimpleIniA ini;
				ini.SetUnicode();

				if (ini.LoadFile(wpath) >= 0) {
					for (const auto& bind : bindings) {
						std::visit([&](auto* ptr) {
							using T = std::decay_t<decltype(*ptr)>;
							if constexpr (std::is_same_v<T, bool>) {
								*ptr = ini.GetBoolValue(bind.section, bind.key, *ptr);
							} else if constexpr (std::is_same_v<T, int>) {
								*ptr = static_cast<int>(ini.GetLongValue(bind.section, bind.key, *ptr));
							} else if constexpr (std::is_same_v<T, float>) {
								*ptr = static_cast<float>(ini.GetDoubleValue(bind.section, bind.key, *ptr));
							} else if constexpr (std::is_same_v<T, std::string>) {
								*ptr = ini.GetValue(bind.section, bind.key, ptr->c_str());
							}
						}, bind.var);
					}
					readStatus = true;
				} else {
					logger::error("Failed to load INI file at {}", path);
				}
			} else {
				logger::warn("INI file does not exist at {}", path);
			}

			// Custom UI INI override logic
			if (std::filesystem::exists(pathCustomUI)) {
				logger::info("Custom UI INI found at {}, overriding QuestUI settings", pathCustomUI);

				std::wstring wpathCustom_str(pathCustomUI.begin(), pathCustomUI.end());
				const wchar_t* wpathCustom = wpathCustom_str.c_str();
				CSimpleIniA iniCustom;
				iniCustom.SetUnicode();

				if (iniCustom.LoadFile(wpathCustom) >= 0) {
					for (auto& item : questUIBindings) {
						if (bQuestUIOverrideUISettings && (
							std::string(item.key) == "iDialogueSortMode" ||
							std::string(item.key) == "bDisplayIcon" ||
							std::string(item.key) == "cQuestEntryColor"
						)) continue;

						std::visit([&](auto* ptr) {
							using T = std::decay_t<decltype(*ptr)>;
							if constexpr (std::is_same_v<T, bool>) {
								*ptr = iniCustom.GetBoolValue("QuestUI", item.key, *ptr);
							} else if constexpr (std::is_same_v<T, int>) {
								*ptr = static_cast<int>(iniCustom.GetLongValue("QuestUI", item.key, *ptr));
							} else if constexpr (std::is_same_v<T, float>) {
								*ptr = static_cast<float>(iniCustom.GetDoubleValue("QuestUI", item.key, *ptr));
							} else if constexpr (std::is_same_v<T, std::string>) {
								*ptr = iniCustom.GetValue("QuestUI", item.key, ptr->c_str());
							}
						}, item.var);
					}

				} else {
					logger::error("Failed to load Custom UI INI at {}", pathCustomUI);
				}
			}

			// Clamping logic

			// General
			iVerboseMode = std::clamp(iVerboseMode, 0, 2);

			// Quest Filters
			iQuestPapyrusFilterMode = std::clamp(iQuestPapyrusFilterMode, 0, 2);
			iQuestLinkedQuestFilter = std::clamp(iQuestLinkedQuestFilter, 0, 3);
			iQuestUIDialogueSortMode = std::clamp(iQuestUIDialogueSortMode, 0, 5);

			// Quest UI - Paths, Functions & Settings
			cQuestUIQuestEntryColor = std::clamp(cQuestUIQuestEntryColor, 0x000000, 0xFFFFFF);

			// Dialogue Skip
			iSkipHoldDuration = std::clamp(iSkipHoldDuration, 250, 2000);
			iSkipAutoSkipInterval = std::clamp(iSkipAutoSkipInterval, 100, 1000);
			iSkipPapyrusHandle = std::clamp(iSkipPapyrusHandle, 0, 3);
			
			// Dialogue Pause
			iPauseMinDelay = std::clamp(iPauseMinDelay, -200, 2000);
			iPauseMaxDelay = std::clamp(iPauseMaxDelay, -200, 2000);
			if (iPauseMaxDelay < iPauseMinDelay) std::swap(iPauseMinDelay, iPauseMaxDelay);

			// External data
			[&]() {
				using namespace ModData;

				debugVerboseMode = iVerboseMode;
			}();

			return readStatus;
		}

		template <typename T>
		T GetValue(const std::string& key_section, const T& defaultValue = T{})
		{
			auto sep = key_section.find(':');
			if (sep == std::string::npos) {
				logger::error("Invalid key_section format: '{}'", key_section);
				return defaultValue;
			}

			std::string section = key_section.substr(0, sep);
			std::string key = key_section.substr(sep + 1);

			for (const auto& bind : bindings) {
				if (key == bind.key && section == bind.section) {
					if constexpr (std::is_same_v<T, bool>) {
						if (auto v = std::get_if<bool*>(&bind.var)) return **v;
						if (auto v = std::get_if<int*>(&bind.var)) return **v != 0;
						if (auto v = std::get_if<float*>(&bind.var)) return **v != 0.0f;
					}
					else if constexpr (std::is_same_v<T, int>) {
						if (auto v = std::get_if<int*>(&bind.var)) return **v;
						if (auto v = std::get_if<float*>(&bind.var)) return static_cast<int>(**v);
						if (auto v = std::get_if<bool*>(&bind.var)) return **v ? 1 : 0;
					}
					else if constexpr (std::is_same_v<T, float>) {
						if (auto v = std::get_if<float*>(&bind.var)) return **v;
						if (auto v = std::get_if<int*>(&bind.var)) return static_cast<float>(**v);
						if (auto v = std::get_if<bool*>(&bind.var)) return **v ? 1.0f : 0.0f;
					}
					else if constexpr (std::is_same_v<T, std::string>) {
						if (auto v = std::get_if<std::string*>(&bind.var)) return **v;
					}

					logger::error("Type mismatch for key '{}' in section '{}'", key, section);
					return defaultValue;
				}
			}

			logger::error("No binding found for key '{}' in section '{}'", key, section);
			return defaultValue;
		}

		template <typename T>
		bool SetValue(const std::string& key_section, const T& value)
		{
			auto sep = key_section.find(':');
			if (sep == std::string::npos) {
				logger::error("Invalid key_section format: '{}'", key_section);
				return false;
			}

			std::string section = key_section.substr(0, sep);
			std::string key = key_section.substr(sep + 1);

			if (section.empty() || key.empty()) {
				logger::error("Empty section or key in '{}'", key_section);
				return false;
			}

			for (auto& bind : bindings) {
				if (section == bind.section && key == bind.key) {
					bool matched = std::visit([&](auto* ptr) -> bool {
						using PtrType = std::decay_t<decltype(*ptr)>;
						if constexpr (std::is_same_v<PtrType, T>) {
							*ptr = value;
							return true;
						}
						return false;
					}, bind.var);

					if (!matched) {
						logger::error("Type mismatch for {}:{}", section, key);
						return false;
					}

					CSimpleIniA ini;
					ini.SetUnicode();
					if (std::filesystem::exists(path)) ini.LoadFile(path.c_str());

					if constexpr (std::is_same_v<T, bool>) {
						ini.SetBoolValue(section.c_str(), key.c_str(), value);
					} else if constexpr (std::is_same_v<T, int>) {
						ini.SetLongValue(section.c_str(), key.c_str(), value);
					} else if constexpr (std::is_same_v<T, float>) {
						ini.SetDoubleValue(section.c_str(), key.c_str(), value);
					} else if constexpr (std::is_same_v<T, std::string>) {
						ini.SetValue(section.c_str(), key.c_str(), value.c_str());
					} else {
						return false;
					}

					if (ini.SaveFile(path.c_str()) < 0) {
						logger::error("Failed to save INI file at {}", path);
						return false;
					}

					return true;
				}
			}

			logger::error("No binding found for {}:{}", section, key);
			return false;
		}

	private:
		inline static std::string path = "Data/SKSE/Plugins/SmartTalk.ini";
		inline static std::string pathCustomUI = "Data/SKSE/Plugins/SmartTalk_CustomUI.ini";

		using IniValue = std::variant<bool*, int*, float*, std::string*>;

		struct IniBinding
		{
			const char* section;
			const char* key;
			IniValue    var;
		};
		std::vector<IniBinding> bindings;

		struct QuestUIBinding
		{
			const char* key;
			IniValue    var;
		};
		std::vector<QuestUIBinding> questUIBindings;
	};
}
