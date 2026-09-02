#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Features/Input.hpp"
#include "Features/Quest.hpp"
#include "Features/Sorting.hpp"

namespace Events
{
	class MainEvent
	{
	public:

		static void InstallHooks()
		{
			auto& trampoline = SKSE::GetTrampoline();
			SKSE::AllocTrampoline(1 << 4);

			REL::Relocation<uintptr_t> hook{ REL::RelocationID(67315, 68617) };
			_processInputHook = trampoline.write_call<5>(hook.address() + REL::Relocate(0x7B, 0x7B, 0x81), ProcessInputHookTemplate);
			logger::info("ProcessInputHook hooked at address: 0x{:X}", _processInputHook.address());

			REL::Relocation<std::uintptr_t> dialogueMenuVtbl{ RE::VTABLE_DialogueMenu[0] };
			_dialogueMenu = dialogueMenuVtbl.write_vfunc(0x4, DialogueMenuTemplate);
			logger::info("DialogueMenu hooked at virtual table index 0x4. Address: 0x{:X}", _dialogueMenu.address());
		};

		static void InitializeQuest()
		{
			if (!SettingsIni::bQuestEnable) return;

			if (SettingsIni::bAsynchronousStartup) {
				loadFuture = std::async(std::launch::async, &Quest::Initialize);
			} else {
				Quest::Initialize();
			}
		}

		static void InitializeQuestReady()
		{
			if (!SettingsIni::bQuestEnable) return;

			if (SettingsIni::bAsynchronousStartup && loadFuture.valid()) {
				loadFuture.get();
			}
		}

	private:

		static inline std::future<void> loadFuture;

		static void ProcessInputHookTemplate(RE::BSTEventSource<RE::InputEvent*>* dispatcher, RE::InputEvent* const* eventList)
		{
			if (dispatcher && eventList && *eventList) {
				if (Input::HandleDialogueInput(eventList)) {
					constexpr RE::InputEvent* const dummy[] = { nullptr };
					return _processInputHook(dispatcher, dummy);
				}
			}

			return _processInputHook(dispatcher, eventList);
		}
		static inline REL::Relocation<decltype(ProcessInputHookTemplate)> _processInputHook;

		static RE::UI_MESSAGE_RESULTS DialogueMenuTemplate(RE::DialogueMenu* a_this, RE::UIMessage& message)
		{
			if (message.type == RE::UI_MESSAGE_TYPE::kShow) {
				Input::QueueDialogueSkip();
			}

			if (message.type == RE::UI_MESSAGE_TYPE::kShow || message.type == RE::UI_MESSAGE_TYPE::kReshow || message.type == RE::UI_MESSAGE_TYPE::kUpdate) {
				auto* menu = RE::MenuTopicManager::GetSingleton();
				if (menu && menu->dialogueList) {
					if (SettingsIni::iQuestUIDialogueSortMode > 0) {
						Sorting::ApplyDialogueSorting(menu);
					}
					if (SettingsIni::bQuestEnable) {
						Quest::UpdateDialogueResponses(menu->dialogueList);
					}
				}
			}

			return _dialogueMenu(a_this, message);
		}
		static inline REL::Relocation<decltype(DialogueMenuTemplate)> _dialogueMenu;
	};
};
