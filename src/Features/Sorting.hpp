#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Utils/ModUtils.hpp"

class Sorting
{
public:

	static void ApplyDialogueSorting(RE::MenuTopicManager* menu)
	{
		if (!menu || !menu->dialogueList) return;

		std::vector<RE::MenuTopicManager::Dialogue*> dialogues;
		for (auto it = menu->dialogueList->begin(); it != menu->dialogueList->end(); ++it) {
			dialogues.push_back(*it);
		}

		if (dialogues.empty()) return;

		switch (SettingsIni::iQuestUIDialogueSortMode) {
			case 0: // Default sorting
				break;

			case 1: // Quest dialogues first (original order)
				ApplyQuestGrouping(menu, dialogues, true, false);
				break;

			case 2: // Quest dialogues last (original order)
				ApplyQuestGrouping(menu, dialogues, false, false);
				break;

			case 3: // Global alphabetical order
				ApplyAlphabetical(menu, dialogues);
				break;

			case 4: // Quest first + alphabetical in each group
				ApplyQuestGrouping(menu, dialogues, true, true);
				break;

			case 5: // Quest last + alphabetical in each group
				ApplyQuestGrouping(menu, dialogues, false, true);
				break;

			default:
				logger::error("Sorting: Unknown mode ({})", SettingsIni::iQuestUIDialogueSortMode);
				break;
		}
	}

private:

	static bool AlphabeticalCompare(const RE::MenuTopicManager::Dialogue* a, const RE::MenuTopicManager::Dialogue* b)
	{
		if (!a || !b) return false;
		const char* ta = a->topicText.c_str();
		const char* tb = b->topicText.c_str();
		if (!ta || !tb) return false;
		return std::strcmp(ta, tb) < 0;
	}

	static void ApplyAlphabetical(RE::MenuTopicManager* menu, std::vector<RE::MenuTopicManager::Dialogue*>& dialogues)
	{
		std::sort(dialogues.begin(), dialogues.end(), AlphabeticalCompare);
		RebuildDialogueList(menu, dialogues);
	}

	static void ApplyQuestGrouping(RE::MenuTopicManager* menu, std::vector<RE::MenuTopicManager::Dialogue*>& dialogues, bool questFirst, bool alphabetical)
	{
		std::vector<RE::MenuTopicManager::Dialogue*> questDialogues;
		std::vector<RE::MenuTopicManager::Dialogue*> normalDialogues;

		for (auto* dlg : dialogues) {
			if (ModUtils::IsQuestDialogue(dlg)) {
				questDialogues.push_back(dlg);
			} else {
				normalDialogues.push_back(dlg);
			}
		}

		if (alphabetical) {
			std::sort(questDialogues.begin(), questDialogues.end(), AlphabeticalCompare);
			std::sort(normalDialogues.begin(), normalDialogues.end(), AlphabeticalCompare);
		}

		std::vector<RE::MenuTopicManager::Dialogue*> result;
		if (questFirst) {
			result.insert(result.end(), questDialogues.begin(), questDialogues.end());
			result.insert(result.end(), normalDialogues.begin(), normalDialogues.end());
		} else {
			result.insert(result.end(), normalDialogues.begin(), normalDialogues.end());
			result.insert(result.end(), questDialogues.begin(), questDialogues.end());
		}

		RebuildDialogueList(menu, result);
	}

	static void RebuildDialogueList(RE::MenuTopicManager* menu, const std::vector<RE::MenuTopicManager::Dialogue*>& ordered)
    {
        if (!menu || !menu->dialogueList) return;

        if (IsAlreadyOrdered(menu->dialogueList, ordered)) return;

        auto* oldList = menu->dialogueList;
        auto* newList = new RE::BSSimpleList<RE::MenuTopicManager::Dialogue*>();

        auto prev = newList->begin();
        bool firstInserted = false;

        for (auto* dlg : ordered) {
            if (!dlg) continue;
            if (!firstInserted) {
                newList->emplace_front(dlg);
                prev = newList->begin();
                firstInserted = true;
            } else {
                prev = newList->insert_after(prev, dlg);
            }
        }

        delete oldList;
        menu->dialogueList = newList;
    }

	static bool IsAlreadyOrdered(RE::BSSimpleList<RE::MenuTopicManager::Dialogue*>* list, const std::vector<RE::MenuTopicManager::Dialogue*>& ordered)
    {
        auto it = list->begin();
        for (auto* dlg : ordered) {
            if (!dlg) continue;
            if (it == list->end() || *it != dlg) return false;
            ++it;
        }
        return it == list->end();
    }
};
