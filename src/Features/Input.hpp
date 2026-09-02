#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Utils/ModUtils.hpp"

class Input
{
public:
	
	static bool HandleDialogueInput(RE::InputEvent* const* eventList)
	{
		const auto ui = RE::UI::GetSingleton();
		if (!ui || !ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) return false;

		RE::BSSpinLockGuard lock(ui->processMessagesLock);

		const auto menu = ui->GetMenu(RE::DialogueMenu::MENU_NAME);
		if (!menu) return false;

		if (ButtonEvent(eventList)) return SkipDialogueText(menu, {0, 2});
		return false;
	}

	static void QueueDialogueSkip()
	{
		ModUtils::WaitAndCall(100ms, []() {
			SKSE::GetTaskInterface()->AddUITask([]() {
				const auto ui = RE::UI::GetSingleton();
				if (!ui || !ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) return;

				RE::BSSpinLockGuard lock(ui->processMessagesLock);

				const auto menu = ui->GetMenu(RE::DialogueMenu::MENU_NAME);
				if (!menu || IsDialogueGoodbye()) return;

				if (SettingsIni::bSkipOnInteraction) SkipDialogueText(menu, { 0 });
				else SkipDialogueText(menu, { 0 }, true);
			});
		});
	}

private:

	static bool ButtonEvent(RE::InputEvent* const* eventList)
	{
		for (auto* current = *eventList; current; current = current->next) {
			if (auto* buttonEvent = current->AsButtonEvent()) {

				if (buttonEvent->GetUserEvent() == RE::UserEvents::GetSingleton()->click ||
					buttonEvent->GetUserEvent() == RE::UserEvents::GetSingleton()->accept ||
					(buttonEvent->HasIDCode() && buttonEvent->GetIDCode() == 57)) {
					
					if (SettingsIni::bSkipImmediateOnInput && buttonEvent->IsDown()) return true;
					else if (SettingsIni::bSkipHoldToSkip && buttonEvent->HeldDuration() > (SettingsIni::iSkipHoldDuration / 1000.0f)) {
						static auto lastRun = std::chrono::steady_clock::now();
						const auto now = std::chrono::steady_clock::now();
						const auto cooldown = std::chrono::milliseconds(SettingsIni::iSkipAutoSkipInterval);						

						if (now - lastRun < cooldown) return false;
						lastRun = now;
							
						return true;
					}
				}
			}
		}

		return false;
	}
	
	static bool SkipDialogueText(RE::GPtr<RE::IMenu> menu, const std::vector<int>& allowedIndices, const bool allowOnly = false)
	{
		if (!menu || !menu->uiMovie || !menu->uiMovie->GetVisible()) return false;

		RE::GFxValue selectedIndex;
		menu->uiMovie->GetVariable(&selectedIndex, "_root.DialogueMenu_mc.eMenuState");

		if (selectedIndex.GetType() == RE::GFxValue::ValueType::kUndefined) return false;

		const int selectedIndexInt = static_cast<int>(selectedIndex.GetSInt());
		if (std::find(allowedIndices.begin(), allowedIndices.end(), selectedIndexInt) == allowedIndices.end()) return false;

		menu->uiMovie->SetVariable("_root.DialogueMenu_mc.bAllowProgress", true);

		if (!allowOnly) {
			RE::GFxValue fadedIn;
			menu->uiMovie->GetVariable(&fadedIn, "_root.DialogueMenu_mc.bFadedIn");
			if (!static_cast<int>(fadedIn.GetBool())) return false;

			if (SettingsIni::iSkipPapyrusHandle > 0 && IsDialogueGoodbye()) {
				switch (SettingsIni::iSkipPapyrusHandle) {
				case 1:
					// Prevent skipping
					break;
				case 2:
					menu->uiMovie->InvokeNoReturn("_root.DialogueMenu_mc.onCancelPress", nullptr, 0);
					break;	
				case 3:
					SkipDialogueGoodbye(menu);
					break;
				}
			} else {
				menu->uiMovie->InvokeNoReturn("_root.DialogueMenu_mc.SkipText", nullptr, 0);
			}
		}

		return true;
	}

	static void SkipDialogueGoodbye(RE::GPtr<RE::IMenu> menu)
	{
		SKSE::GetTaskInterface()->AddUITask([menu]() {
			static std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> topicTimers;

			if (!menu || !menu->uiMovie || !menu->uiMovie->GetVisible()) return;

			auto* topicManager = RE::MenuTopicManager::GetSingleton();
			if (!topicManager || !topicManager->forceGoodbye) return;

			auto* currentTopic = topicManager->currentTopicInfo;
			if (!currentTopic) return;

			const auto formID = currentTopic->formID;
			const auto now = std::chrono::steady_clock::now();

			auto it = topicTimers.find(formID);
			if (it == topicTimers.end()) {
				topicTimers[formID] = now + std::chrono::milliseconds(500);
				return;
			}

			if (now < it->second) return;

			menu->uiMovie->InvokeNoReturn("_root.DialogueMenu_mc.SkipText", nullptr, 0);
			topicTimers[formID] = now + std::chrono::milliseconds(500);

			ModUtils::WaitAndCall(500ms, [formID = currentTopic->formID]() {
				if (!formID) return;
				topicTimers.erase(formID);
			}, false);
		});
	}

	static bool IsDialogueGoodbye()
	{
		auto* topicManager = RE::MenuTopicManager::GetSingleton();
		if (!topicManager || !topicManager->forceGoodbye || !topicManager->lastSelectedDialogue) return false;

		auto* currentDial = topicManager->lastSelectedDialogue;
		if (!currentDial->currentResponse) return false;

		RE::DialogueResponse* latestResposne = nullptr;
		for (auto& resp : currentDial->responses) {
			if (!resp) continue;
			latestResposne = resp;
		}

		return (latestResposne == currentDial->currentResponse->item);
	}
};
