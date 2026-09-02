#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Utils/NativeUtils.hpp"

namespace ModData
{
	class DataHandler
	{
	public:
		static DataHandler* GetSingleton()
		{
			static DataHandler singleton;
			return &singleton;
		}

		void LoadData()
		{
			static bool loadingStarted = false;

			if (loadingStarted) return;
			loadingStarted = true;

			TESdataHandler = RE::TESDataHandler::GetSingleton();
			
			ApplyGameSettings();
			LoadPluginsForms();
		}

	private:
		static void LoadPluginsForms()
		{
			logger::info("Loading Plugins Forms Data...");

			for (const auto& formInfo : pluginForms) {
				*formInfo.formPtr = TESdataHandler->LookupForm(formInfo.formID, formInfo.pluginName.data());
				if (!*formInfo.formPtr && !formInfo.optional) {
					REPORT_AND_FAIL("ERROR: Form \"{}\" not found in \"{}\".", formInfo.pluginName, formInfo.name, formInfo.pluginName);
				}
			}

			logger::info("Loading Plugins Forms Data: DONE");
		}

		static inline void ApplyGameSettings()
		{
			logger::info("Applying Game Settings...");
			//RE::TESTopic::fullName
			for (auto* topic : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESTopic>()) {
				if (topic->fullName.empty()) continue;
				//topic->fullName = topic->formID + " : " + topic->fullName;

				//TRACE("  -> Topic : {:08X} | Question : {} | subtopic count : {}", topic->formID, topic->fullName, topic->numTopicInfos);

				/*std::string newName = std::format("[QUEST]{:08X} : {}", topic->formID, topic->fullName.c_str());
				topic->fullName = newName;*/


				for (std::uint32_t i = 0; i < topic->numTopicInfos; ++i) {
					RE::TESTopicInfo* info = topic->topicInfos[i];
					if (!info) continue;

					//TRACE("  -> SubTopic : {:08X}", info->formID);

					// ici tu peux accéder à info->...
				}

	



				//topic->ownerQuest

				//TRACE("topic name : {}", topic->fullName);
			}


			// NativeUtils::SetGameSetting("iArrowInventoryChance", 0);
			//NativeUtils::SetGameSetting("fGameplayVoiceFilePadding", 0.0f);

			logger::info("Applying Game Settings: DONE");
		}
	};
}
