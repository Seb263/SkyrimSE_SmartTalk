#pragma once

#include "SettingsIni.hpp"

#include "Utils/MiscUtils.hpp"
#include "Utils/NativeUtils.hpp"

#define FRAME_DELAY_MS() std::chrono::milliseconds(static_cast<int>(std::lround(ModUtils::GetFrameDelay() * 1000.0f)))

class ModUtils
{
public:

	static bool IsQuestDialogue(RE::MenuTopicManager::Dialogue* activeEntry)
	{
		using namespace ModData;

		if (!activeEntry || !activeEntry->parentTopic) return false;

		auto* topic = RE::TESForm::LookupByID<RE::TESTopic>(activeEntry->parentTopic->GetFormID());
		if (!topic) return false;

		auto hasQuestInfo = [](RE::TESTopic* t) -> bool {
			if (!t) return false;
			for (uint32_t i = 0; i < t->numTopicInfos; ++i) {
				const auto* info = t->topicInfos[i];
				if (!info) continue;
				if (std::find(questTopicInfoList.begin(), questTopicInfoList.end(), info->formID) != questTopicInfoList.end())
					return true;
			}
			return false;
		};

		bool hasQuestResponse = false;

		if (auto* branch = topic->ownerBranch) {
			bool hasInvisibleContinue = false;

			for (uint32_t i = 0; i < topic->numTopicInfos; ++i) {
				auto* subInfo = topic->topicInfos[i];
				if (!subInfo) continue;

				if (subInfo->data.flags.any(RE::TOPIC_INFO_DATA::TOPIC_INFO_FLAGS::kEndRunningScene)) {
					hasInvisibleContinue = true;
					break;
				}
			}

			if (hasInvisibleContinue) {
				for (auto* relatedTopic : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESTopic>()) {
					if (!relatedTopic || relatedTopic == topic || relatedTopic->ownerBranch != branch) continue;

					if (hasQuestInfo(relatedTopic)) {
						hasQuestResponse = true;
						break;
					}
				}
			}
		}

		if (!hasQuestResponse) hasQuestResponse = hasQuestInfo(topic);

		return hasQuestResponse;
	}

	template <typename TDuration, typename TCallback>
	static void WaitAndCall(TDuration delay, TCallback&& callback, const bool secureFrame = true)
	{
		std::jthread([delay, callback = std::forward<TCallback>(callback), secureFrame]() mutable {
			WaitForGameReady();
			auto failure = std::make_shared<std::atomic_bool>(false);
			const auto deadline = (std::chrono::steady_clock::now() + delay);

			while (true) {
				auto remaining = (deadline - std::chrono::steady_clock::now());
				std::this_thread::sleep_for(remaining > 100ms ? 100ms : (remaining > 0ns ? remaining : FRAME_DELAY_MS()));

				const bool last = (std::chrono::steady_clock::now() >= deadline);
				SKSE::GetTaskInterface()->AddTask([callback, secureFrame, failure, last, taskStart = std::chrono::steady_clock::now()]() {
					if (secureFrame && std::chrono::steady_clock::now() - taskStart > 300ms) *failure = true;
					if (last) {
						if (*failure) TRACE("WaitAndCall: Task was delayed and invalidated due to frame timing (>{}ms)", 300);
						else callback();
					}
				});
				if (last) break;
			}

		}).detach();
	}

	static bool WaitForGameReady(bool ignoreLoadingMenu = false)
	{
		bool wasPaused = false;

		while (true) {
			if (auto ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) {
				static auto loadingMenu = ui->GetMenu("Loading Menu");
				if (ignoreLoadingMenu && ui->numPausesGame == 1 && loadingMenu && loadingMenu->OnStack()) break;

				std::this_thread::sleep_for(FRAME_DELAY_MS());
				wasPaused = true;
				continue;
			}

			std::promise<void> p;
			auto f = p.get_future();

			SKSE::GetTaskInterface()->AddTask([&p]() { p.set_value(); });
        
			auto start = std::chrono::high_resolution_clock::now();
			f.get();

			if ((std::chrono::high_resolution_clock::now() - start) > 100ms) {
				wasPaused = true;
				continue;
			}

			break;
		}

		return wasPaused;
	}

	static float GetFrameDelay()
	{
		RE::BSTimer* bsTimer = RE::BSTimer::GetSingleton();
		if (!bsTimer) return 0.00694444f; // 144Hz

		float frame_delay = bsTimer->realTimeDelta / bsTimer->QGlobalTimeMultiplier();
		frame_delay = std::clamp(frame_delay, 0.004f, 0.1f);

		return frame_delay;
	}
};
