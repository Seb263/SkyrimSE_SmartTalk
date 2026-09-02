#include "Events.h"

namespace Events
{
	inline std::shared_mutex topicMutex;
	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESTopicInfoEvent* event, RE::BSTEventSource<RE::TESTopicInfoEvent>*)
	{
		if (!SettingsIni::bPauseEnabled) return continueEvent;

		static auto lastRun = std::chrono::steady_clock::now();
		const auto  now = std::chrono::steady_clock::now();
		if (now - lastRun < 1s) return continueEvent;

		if (event->type.any(RE::TESTopicInfoEvent::TopicInfoEventType::kTopicEnd)) {
			if (auto* speaker = MiscUtils::ResolveHandleAs<RE::TESObjectREFR>(event->speakerRef)) {
				std::unique_lock lock(topicMutex);

				const float minDelay = (SettingsIni::iPauseMinDelay / 1000.0f);
				const float maxDelay = (SettingsIni::iPauseMaxDelay / 1000.0f);

				NativeUtils::SetGameSetting("fGameplayVoiceFilePadding", MiscUtils::GetRandomNumber(minDelay, maxDelay));

				lastRun = now;
			}
		}

		return continueEvent;
	}
}
