#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Utils/MiscUtils.hpp"

class Quest
{
public:

	inline static std::string cachePath = "Data/SKSE/Plugins/SmartTalk_QuestCache.json";

	using TopicMap = std::unordered_map<uint32_t, std::vector<RE::FormID>>;
	using PluginMap = std::unordered_map<std::string, uint32_t>;
	using PluginList = std::vector<std::pair<std::string, uint32_t>>;
	using TopicScriptsMap = std::unordered_map<RE::FormID, std::pair<RE::FormID, std::string>>;

	static void Initialize()
	{
		using namespace ModData;

		const auto start = std::chrono::high_resolution_clock::now();
		logger::info("Initializing quest topic cache ({})...", (SettingsIni::bAsynchronousStartup ? "asynchronous" : "synchronous"));

		auto globalHash = GenerateGlobalHash();
		ApplyQuestFilterSettingsToHash(globalHash);
		std::vector<RE::FormID> cachedQuestTopics;
		json jCache;

		if (SettingsIni::bQuestUseCache && std::filesystem::exists(cachePath)) {
			try {
				std::ifstream fileStream(cachePath);
				jCache = json::parse(fileStream);
				logger::info("Parsing cached JSON Data In \"{}\"", cachePath);
			} catch (const std::exception& e) {
				logger::warn("Error while processing cached JSON file '{}':\n  -> {}", cachePath, e.what());
			}
		}

		const auto topicMap = CollectDialogueTopicsByMod();
		const auto pluginMap = GetPluginMapFromTopicMap(topicMap);
		bool loaded = false;

		if (!jCache.is_null() && jCache.contains("GlobalHash") && jCache["GlobalHash"].get<uint64_t>() == globalHash) {
			logger::info("Global hash matches. Loading quest topics from cache.");
			loaded = LoadQuestTopicsFromJSON(jCache, cachedQuestTopics, pluginMap);
		}

		if (!loaded) {	
			const auto pluginList = SortPluginMapByLoadOrder(pluginMap);
			auto pluginsHash = ComputePluginsHashFromPluginMap(pluginMap);
			ApplyQuestFilterSettingsToHash(pluginsHash);

			if (!jCache.is_null() && jCache.contains("PluginsHash") && jCache["PluginsHash"].get<uint64_t>() == pluginsHash) {
				logger::info("Plugins hash matches. Loading quest topics from cache and updating global hash.");
				loaded = LoadQuestTopicsFromJSON(jCache, cachedQuestTopics, pluginMap);
				jCache["GlobalHash"] = globalHash;
				std::ofstream out(cachePath, std::ios::binary | std::ios::trunc);
				if (out.is_open()) out << jCache.dump();
			}

			if (!loaded) {
				logger::info("No valid cache. Generating quest topics from scratch...");

				std::unordered_map<std::string, TopicScriptsMap> pluginScriptsMap;
				for (const auto& [pluginName, modIndex] : pluginList) {
					TopicScriptsMap topicScripts;
					ParsePlugin("Data/" + pluginName, topicScripts, pluginMap);
					pluginScriptsMap[pluginName] = std::move(topicScripts);
				}

				std::unordered_set<RE::FormID> analyzedFormIDs;
				for (int i = static_cast<int>(pluginList.size()) - 1; i >= 0; --i) {
					const auto& [pluginName, modIndex] = pluginList[i];
					auto it = pluginScriptsMap.find(pluginName);
					if (it == pluginScriptsMap.end()) continue;

					for (const auto& [topicInfoFormID, topicData] : it->second) {
						const auto& [topicFormID, scriptName] = topicData;
						if (!topicInfoFormID || scriptName.empty()) continue;
						if (analyzedFormIDs.find(topicInfoFormID) != analyzedFormIDs.end()) continue;

						if (ScriptHasQuestStageAction(scriptName, topicInfoFormID, topicFormID)) cachedQuestTopics.push_back(topicInfoFormID);

						analyzedFormIDs.insert(topicInfoFormID);
					}
				}

				if (SettingsIni::bQuestUseCache) {
					GenerateCompiledJSON(jCache, pluginList, cachedQuestTopics);
					jCache["GlobalHash"] = globalHash;
					jCache["PluginsHash"] = pluginsHash;
					std::ofstream out(cachePath, std::ios::binary | std::ios::trunc);
					if (out.is_open()) out << jCache.dump();
				}
			}
		}

		questTopicInfoList = std::move(cachedQuestTopics);

		TRACE("Total quest-related topics found: {}", questTopicInfoList.size());

		const auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed = end - start;
		logger::info("Quest topic cache initialization DONE after {} seconds", elapsed.count());
	}

	static void UpdateDialogueResponses(RE::BSSimpleList<RE::MenuTopicManager::Dialogue*>* dialogueList)
	{
		using namespace ModData;

		const auto menu = RE::MenuTopicManager::GetSingleton();
		if (!menu || !menu->dialogueList) return;

		auto* speaker = MiscUtils::ResolveHandleAs<RE::TESObjectREFR>(menu->speaker);
		if (!speaker) return;

		std::vector<int> uiFlagsVector;

		for (auto* activeEntry : *menu->dialogueList) {
			uiFlagsVector.push_back(ModUtils::IsQuestDialogue(activeEntry) ? 1 : 0);
		}

		SetUIVariablesVector(uiFlagsVector);
	}

private:

	static inline std::vector<int> uiFlags = {};

	struct RecordHeader
	{
		char       name[4];
		uint32_t   size;
		uint32_t   flags;
		RE::FormID formID;
		uint32_t   timestamp;
		uint16_t   versionControlInfo;
		uint16_t   internalVersion;
	};

	struct SetEntryTextHandler : RE::GFxFunctionHandler
	{
		void Call(Params& params) override
		{
			if (params.argCount < 2 || uiFlags.empty()) return;

			RE::GFxValue* args = params.args;
			RE::GFxValue& aEntryClip = args[0];
			RE::GFxValue& aEntryObject = args[1];
			RE::GFxValue result;
			
			params.thisPtr->Invoke("SetEntryTextLegacy", &result, params.args, params.argCount);

			if (!aEntryClip.IsObject() || !aEntryObject.IsObject()) return;

			int idx = 0;
			RE::GFxValue topicIndexVal;
			if (aEntryObject.GetMember("topicIndex", &topicIndexVal) &&
				topicIndexVal.GetType() == RE::GFxValue::ValueType::kNumber) {
				idx = static_cast<int>(topicIndexVal.GetNumber());
			}

			const bool isQuestLine = idx < static_cast<int>(uiFlags.size()) && (uiFlags)[idx] == 1;

			RE::GFxValue textField;
			if (aEntryClip.GetMember("textField", &textField)) {
				RE::GFxValue questMarker;
				const bool hasMarker = aEntryClip.GetMember("questMarker_mc", &questMarker) && 
					questMarker.GetType() == RE::GFxValue::ValueType::kDisplayObject;

				if (isQuestLine) {
					textField.SetMember("textColor", RE::GFxValue(SettingsIni::cQuestUIQuestEntryColor));

					if (SettingsIni::bQuestUIDisplayIcon && !hasMarker && !SettingsIni::sQuestUIQuestMarkerClip.empty()) {
						RE::GFxValue depthVal;
						aEntryClip.Invoke("getNextHighestDepth", &depthVal, nullptr, 0);
						int32_t depth = static_cast<int32_t>(depthVal.GetSInt());

						RE::GFxValue args[]{
							RE::GFxValue(SettingsIni::sQuestUIQuestMarkerClip.c_str()),
							RE::GFxValue("questMarker_mc"),
							depthVal
						};

						RE::GFxValue questMarker;
						if (!aEntryClip.Invoke("attachMovie", &questMarker, args, 3) || !questMarker.IsObject()) {
							questMarker = AddQuestMarker(aEntryClip, depth);
						}

						if (questMarker.IsObject()) {
							questMarker.SetMember("_x", RE::GFxValue(0));

							RE::GFxValue textFieldY, textFieldHeight, questMarkerHeight;
							textField.GetMember("_y", &textFieldY);
							textField.GetMember("_height", &textFieldHeight);
							questMarker.GetMember("_height", &questMarkerHeight);

							const double y = textFieldY.GetNumber() + (textFieldHeight.GetNumber() / 2 - questMarkerHeight.GetNumber() / 2);
							questMarker.SetMember("_y", RE::GFxValue(y));

							RE::GFxValue markerWidth;
							questMarker.GetMember("_width", &markerWidth);
							textField.SetMember("_x", RE::GFxValue(markerWidth.GetNumber() + 8));
						}
					}
				} else {
					textField.SetMember("_x", RE::GFxValue(0));
					if (hasMarker) questMarker.Invoke("removeMovieClip");
				}
			}

			if (!SettingsIni::sQuestUISetEntryTextCustomFunction.empty()) SetEntryTextCustom(params, isQuestLine);
		}
	};

	static RE::GFxValue AddQuestMarker(RE::GFxValue& parentClip, int32_t depth)
	{
		RE::GFxValue empty;

		if (!parentClip.IsObject()) return empty;

		RE::GFxValue markerMC;
		if (!parentClip.CreateEmptyMovieClip(&markerMC, "questMarker_mc", depth)) return empty;

		markerMC.SetMember("_xscale", RE::GFxValue(100.0f));
		markerMC.SetMember("_yscale", RE::GFxValue(100.0f));

		struct Point { float x, y; };

		Point poly1[] = {
			{ 9.48f, 5.57f },
			{ 6.89f, 0.0f },
			{ 4.29f, 5.57f },
			{ 6.89f, 10.99f },
			{ 9.48f, 5.57f }
		};

		Point poly2[] = {
			{ 9.82f, 7.99f },
			{ 6.89f, 14.16f },
			{ 4.0f, 7.99f },
			{ 0.0f, 8.25f },
			{ 6.89f, 26.41f },
			{ 13.78f, 8.25f },
			{ 9.82f, 7.99f }
		};

		auto drawPath = [&](Point* pts, size_t count) {
			if (count == 0) return;
			RE::GFxValue moveToArgs[2]{ pts[0].x, pts[0].y };
			markerMC.Invoke("moveTo", nullptr, moveToArgs, 2);
			for (size_t i = 1; i < count; ++i) {
				RE::GFxValue lineToArgs[2]{ pts[i].x, pts[i].y };
				markerMC.Invoke("lineTo", nullptr, lineToArgs, 2);
			}
		};

		auto drawPathOffset = [&](Point* pts, size_t count, float dx, float dy) {
			if (count == 0) return;
			RE::GFxValue moveToArgs[2]{ pts[0].x + dx, pts[0].y + dy };
			markerMC.Invoke("moveTo", nullptr, moveToArgs, 2);
			for (size_t i = 1; i < count; ++i) {
				RE::GFxValue lineToArgs[2]{ pts[i].x + dx, pts[i].y + dy };
				markerMC.Invoke("lineTo", nullptr, lineToArgs, 2);
			}
		};

		uint32_t shadowColor = 0xFF000000;
		uint32_t shadowAlpha = 80;
		RE::GFxValue argsShadowFill[2]{ shadowColor & 0x00FFFFFF, shadowAlpha };
		markerMC.Invoke("beginFill", nullptr, argsShadowFill, 2);

		float offsetX = 1.5f;
		float offsetY = 1.5f;
		drawPathOffset(poly1, sizeof(poly1) / sizeof(poly1[0]), offsetX, offsetY);
		drawPathOffset(poly2, sizeof(poly2) / sizeof(poly2[0]), offsetX, offsetY);
		markerMC.Invoke("endFill", nullptr, nullptr, 0);

		uint32_t fillColor = 0xFFBBBDBF;
		uint32_t fillAlpha = 255;
		RE::GFxValue argsFill[2]{ fillColor & 0x00FFFFFF, fillAlpha };
		markerMC.Invoke("beginFill", nullptr, argsFill, 2);

		drawPath(poly1, sizeof(poly1) / sizeof(poly1[0]));
		drawPath(poly2, sizeof(poly2) / sizeof(poly2[0]));
		markerMC.Invoke("endFill", nullptr, nullptr, 0);

		return markerMC;
	}

	static bool SetEntryTextCustom(RE::GFxFunctionHandler::Params& params, const bool isQuest)
	{
		if (!params.thisPtr || params.argCount < 2) return false;

		RE::GFxValue GFxIsQuest(isQuest);

		std::vector<RE::GFxValue> newArgs;
		newArgs.reserve(params.argCount + 1);
		for (uint32_t i = 0; i < params.argCount; i++) {
			newArgs.push_back(RE::GFxValue(params.args[i]));
		}
		newArgs.push_back(GFxIsQuest);

		RE::GFxValue result;
		return params.thisPtr->Invoke(
			SettingsIni::sQuestUISetEntryTextCustomFunction.c_str(),
			&result, newArgs.data(), static_cast<uint32_t>(newArgs.size())
		);
	}

	static uint64_t GenerateGlobalHash()
	{
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) return 0;

		uint64_t hash = 0xcbf29ce484222325ULL; // FNV-1a 64 bits

		for (auto* file : dataHandler->files) {
			if (!file) continue;

			const char* name = file->fileName;
			while (*name) {
				hash ^= static_cast<uint64_t>(*name++);
				hash *= 0x100000001b3ULL;
			}

			uint64_t size = (static_cast<uint64_t>(file->fileData.fileSizeHi) << 32) | file->fileData.fileSizeLo;
			for (int i = 0; i < 8; ++i) {
				hash ^= (size >> (i * 8)) & 0xFF;
				hash *= 0x100000001b3ULL;
			}

			uint64_t modTime = (static_cast<uint64_t>(file->fileData.lastWriteTime.hi) << 32) | file->fileData.lastWriteTime.lo;
			for (int i = 0; i < 8; ++i) {
				hash ^= (modTime >> (i * 8)) & 0xFF;
				hash *= 0x100000001b3ULL;
			}

			const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
			const auto versionPacked = static_cast<uint32_t>(plugin->GetVersion().pack());
			for (int i = 0; i < 4; ++i) {
				hash ^= (versionPacked >> (i * 8)) & 0xFF;
				hash *= 0x100000001b3ULL;
			}
		}

		return hash;
	}

	static void ApplyQuestFilterSettingsToHash(uint64_t& hash)
	{
		// Include key INI settings to ensure cache regeneration when they change
		hash ^= static_cast<uint64_t>(SettingsIni::iQuestPapyrusFilterMode);
		hash *= 0x100000001b3ULL;

		hash ^= static_cast<uint64_t>(SettingsIni::bQuestRequireLinkedQuest ? 1ULL : 0ULL);
		hash *= 0x100000001b3ULL;

		hash ^= static_cast<uint64_t>(SettingsIni::iQuestLinkedQuestFilter);
		hash *= 0x100000001b3ULL;
	}

	static bool LoadQuestTopicsFromJSON(const json& jCache, std::vector<RE::FormID>& outQuestTopics, const PluginMap& pluginMap)
	{
		outQuestTopics.clear();

		if (!jCache.contains("QuestTopics")) return false;

		for (auto& [pluginName, arr] : jCache["QuestTopics"].items()) {
			if (pluginMap.find(pluginName) == pluginMap.end()) continue;

			for (auto& formIDVal : arr) {
				uint32_t localFormID = formIDVal.get<uint32_t>();

				auto it = pluginMap.find(pluginName);
				if (it == pluginMap.end()) {
					logger::warn("Plugin {} not found in pluginMap.", pluginName);
					continue;
				}

				uint32_t modIndex = it->second;
				uint32_t fullFormID = localFormID;

				if (modIndex < 0xFE) fullFormID |= (modIndex << 24);
				else fullFormID |= ((0xFE000 | modIndex) << 12);

				outQuestTopics.push_back(fullFormID);
			}
		}

		return true;
	}

	static void GenerateCompiledJSON(json& jCache, const PluginList& pluginList, const std::vector<RE::FormID>& questFormIDs)
	{
		jCache["QuestTopics"] = json::object();

		for (auto formID : questFormIDs) {
			const bool isLight = formID >= 0xFE000000;
			const uint32_t pluginIndex = isLight ? (formID >> 12) : (formID >> 24);

			auto it = std::find_if(pluginList.begin(), pluginList.end(),
				[pluginIndex, isLight](const std::pair<std::string, uint32_t>& p) {
					if (isLight) return ((0xFE000 | p.second) == pluginIndex);
					else return p.second == pluginIndex;
				});

			if (it != pluginList.end()) {
				const std::string& pluginName = it->first;
				const uint32_t localFormID = formID & 0xFFFFFF;

				if (!jCache["QuestTopics"].contains(pluginName)) jCache["QuestTopics"][pluginName] = json::array();
				jCache["QuestTopics"][pluginName].push_back(localFormID);
			} else {
				logger::warn("FormID {:08X} has unknown plugin index [{:02X}].", formID, pluginIndex);
			}
		}

		const auto path = std::filesystem::path(cachePath);
		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);

		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out.is_open()) {
			TRACE("Failed to open {} for writing.", path.string());
			return;
		}

		out << jCache.dump();
		TRACE("Compiled cache saved to {}.", path.string());
	}

	static uint64_t HashFileFNV1a64(const std::filesystem::path& filePath)
	{
		std::ifstream file(filePath, std::ios::binary);
		if (!file.is_open()) return 0;

		constexpr uint64_t fnvOffset = 14695981039346656037ULL;
		constexpr uint64_t fnvPrime  = 1099511628211ULL;

		uint64_t hash = fnvOffset;
		char buffer[4096];

		while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
			std::streamsize n = file.gcount();
			for (std::streamsize i = 0; i < n; ++i) {
				hash ^= static_cast<uint8_t>(buffer[i]);
				hash *= fnvPrime;
			}
		}

		return hash;
	}

	static uint64_t ComputePluginsHashFromPluginMap(const PluginMap& pluginMap)
	{
		uint64_t hash = 0xcbf29ce484222325ULL; // initial arbirtary value

		for (const auto& [pluginName, _] : pluginMap) {
			std::filesystem::path path = "Data/" + pluginName;
			uint64_t fileHash = HashFileFNV1a64(path);

			hash ^= fileHash;
			hash *= 0x100000001b3ULL;  // FNV prime
		}

		return hash;
	}

	static TopicMap CollectDialogueTopicsByMod()
	{
		TopicMap result;

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) return result;

		for (auto* topic : dataHandler->GetFormArray<RE::TESTopic>()) {
			if (!topic || !topic->sourceFiles.array || topic->sourceFiles.array->empty()) continue;

			const auto* file = topic->sourceFiles.array->back();
			if (!file) continue;

			const uint32_t modIndex = file->GetPartialIndex();
			const uint32_t rawFormID = file->IsLight() ? (topic->formID & 0xFFF) : (topic->formID & 0xFFFFFF);

			result[modIndex].push_back(rawFormID);
		}

		if (debugVerboseMode > 1) {
			for (const auto& [modIndex, formIDs] : result) {
				TRACE("Mod with Index [{:02X}] has {} Topic Infos.", modIndex, formIDs.size());
			}
		}

		return result;
	}

	static RE::TESFile* GetTESFileFromModIndex(uint32_t modIndex)
	{
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) return nullptr;

		// Classic ESP/ESM plugins
		static auto** loadedMods = dataHandler->GetLoadedMods();
		static std::size_t numMods = dataHandler->GetLoadedModCount();
		for (std::size_t i = 0; i < numMods; ++i) {
			auto* file = loadedMods[i];
			if (file && file->compileIndex == modIndex) return file;
		}

		// Light plugins (ESL/ESP-FE)
		static auto** loadedLightMods = dataHandler->GetLoadedLightMods();
		static std::size_t numLightMods = dataHandler->GetLoadedLightModCount();
		for (std::size_t i = 0; i < numLightMods; ++i) {
			auto* file = loadedLightMods[i];
			if (file && ((0xFE000 | file->smallFileCompileIndex) == modIndex)) return file;
		}

		return nullptr;
	}

	static PluginMap GetPluginMapFromTopicMap(const TopicMap& topicMap)
	{
		PluginMap pluginMapUnsorted;

		for (const auto& [modIndex, _] : topicMap) {
			if (auto* file = GetTESFileFromModIndex(modIndex)) {
				const std::string pluginName(file->GetFilename());
				pluginMapUnsorted[pluginName] = modIndex;
			}
		}

		return pluginMapUnsorted;
	}

	static PluginList SortPluginMapByLoadOrder(const PluginMap& pluginMapUnsorted)
	{
		PluginList sortedMap;

		if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
			for (auto* file : dataHandler->files) {
				if (!file) continue;
				std::string name(file->GetFilename());

				auto it = pluginMapUnsorted.find(name);
				if (it != pluginMapUnsorted.end()) {
					sortedMap.emplace_back(name, it->second);
				}
			}
		}

		return sortedMap;
	}

	static bool ParsePlugin(const std::string& pluginPath, TopicScriptsMap& outMap, const PluginMap& pluginMap)
	{
		if (!std::filesystem::exists(pluginPath)) {
			logger::error("File not found: {}", pluginPath);
			return false;
		}

		std::ifstream file(pluginPath, std::ios::binary);
		if (!file.is_open()) {
			logger::error("Unable to open file: {}", pluginPath);
			return false;
		}

		const auto masters = GetMastersFromFile(file, pluginPath);
		const size_t fileSize = std::filesystem::file_size(pluginPath);

		const std::string pluginName = std::filesystem::path(pluginPath).filename().string();
		const auto it = pluginMap.find(pluginName);
		
		const uint32_t pluginGlobalIndex = (it != pluginMap.end()) ? it->second : 0xFFFFFFFF;
		const auto localToGlobal = BuildLocalToGlobalMap(masters, pluginMap, pluginGlobalIndex);

		ParseRecords(file, fileSize, outMap);

		outMap = CorrectFormIDs(outMap, localToGlobal, masters, pluginName);

		return true;
	}

	static std::vector<uint32_t> BuildLocalToGlobalMap(const std::vector<std::string>& masters, const PluginMap& pluginMap, uint32_t pluginGlobalIndex)
	{
		std::vector<uint32_t> localToGlobal;
		localToGlobal.reserve(masters.size() + 1);

		for (const auto& masterName : masters) {
			auto it = pluginMap.find(masterName);
			if (it != pluginMap.end()) {
				localToGlobal.push_back(it->second);
			} else { // Dialogue unrelated master
				localToGlobal.push_back(0xFFFFFFFF);
			}
		}

		localToGlobal.push_back(pluginGlobalIndex);

		return localToGlobal;
	}

	static TopicScriptsMap CorrectFormIDs(const TopicScriptsMap& outMap, const std::vector<uint32_t>& localToGlobal, const std::vector<std::string>& masters, const std::string& pluginName)
	{
		TopicScriptsMap correctedMap;

		auto FixFormID = [&](uint32_t formID) -> uint32_t {
			uint32_t localIndex = (formID >> 24) & 0xFF;
			uint32_t fixedIndex = (localIndex < localToGlobal.size()) ? localToGlobal[localIndex] : localToGlobal.back();

			uint32_t globalFormID = 0;
			if (fixedIndex >= 0xFE000) { // Light plugin
				uint32_t lightID = fixedIndex - 0xFE000;
				globalFormID = ((0xFE000 | lightID) << 12) | (formID & 0xFFF);
			} else {
				globalFormID = (fixedIndex << 24) | (formID & 0xFFFFFF);
			}
			return globalFormID;
		};

		for (const auto& [infoFormID, pairData] : outMap) {
			const auto& [topicFormID, scriptName] = pairData;

			uint32_t correctedInfoFormID = FixFormID(infoFormID);
			uint32_t correctedTopicFormID = FixFormID(topicFormID);

			if (correctedInfoFormID >= 0xFF000000 || correctedTopicFormID >= 0xFF000000) continue;
			correctedMap[correctedInfoFormID] = { correctedTopicFormID, scriptName };
		}

		return correctedMap;
	}

	static std::vector<std::string> GetMastersFromFile(std::ifstream& file, const std::string& filename)
	{
		file.seekg(0);

		RecordHeader header{};
		if (!file.read(reinterpret_cast<char*>(&header), sizeof(header))) return {};

		if (std::string(header.name, 4) != "TES4") {
			logger::warn("No TES4 found in {}", filename);
			return {};
		}

		std::vector<char> data(header.size);
		file.read(data.data(), header.size);

		std::vector<std::string> masters;
		size_t pos = 0;

		while (pos + 6 <= data.size()) {
			std::string subName(data.data() + pos, 4);
			uint16_t subSize = *reinterpret_cast<const uint16_t*>(&data[pos + 4]);
			if (pos + 6 + subSize > data.size()) break;

			if (subName == "MAST") {
				std::string masterName(data.data() + pos + 6, subSize);
				if (!masterName.empty() && masterName.back() == '\0')
					masterName.pop_back();
				masters.push_back(masterName);
			}

			pos += 6 + subSize;
		}

		return masters;
	}

	static inline RE::FormID latestTopicFormID = 0x0;
	static void ParseRecords(std::ifstream& file, size_t groupSize, TopicScriptsMap& outMap)
	{
		std::streampos startPos = file.tellg();
		RecordHeader rec{};
		int iter = 0;

		while ((file.tellg() - startPos) < static_cast<std::streamoff>(groupSize) && file.read(reinterpret_cast<char*>(&rec), sizeof(rec))) {
			std::string recName(rec.name, 4);
			iter++;

			if (recName == "GRUP") {
				uint32_t subGroupSize = rec.size;
				ParseRecords(file, subGroupSize, outMap);
			} else if (recName == "INFO") {
				ParseTopicInfoRecord(file, rec, outMap);
			} else {
				if (recName == "DIAL" && MiscUtils::IsFormIDValid(rec.formID)) latestTopicFormID = rec.formID;
				file.seekg(rec.size, std::ios::cur);
			}
		}
	}

	static void ParseTopicInfoRecord(std::ifstream& file, const RecordHeader& rec, TopicScriptsMap& outMap)
	{
		std::vector<char> data(rec.size);
		file.read(data.data(), rec.size);

		size_t pos = 0;
		while (pos + 6 <= data.size()) {
			std::string subName(data.data() + pos, 4);
			uint16_t subSize = *reinterpret_cast<const uint16_t*>(&data[pos + 4]);
			if (pos + 6 + subSize > data.size()) break;

			if (subName == "VMAD") {
				const char* vmadPtr = data.data() + pos + 6;
				size_t vmadSize = subSize;

				if (vmadSize < 4) break;

				size_t offset = 4;
				size_t blockIndex = 0;

				while (offset < vmadSize) {
					const char* blockPtr = vmadPtr + offset;
					size_t      remaining = vmadSize - offset;

					if (blockIndex == 2) { // Script name is in block 2
						size_t strLen = strnlen(blockPtr, remaining);
						if (strLen > 0 && strLen < remaining) {
							outMap[rec.formID] = { latestTopicFormID, std::string(blockPtr, strLen) };
							break;
						}
					}

					size_t strLen = strnlen(blockPtr, remaining);
					if (strLen > 0 && strLen < remaining) {
						offset += strLen + 1;
					} else if (remaining >= 4) {
						offset += 4;
					} else {
						break;
					}

					blockIndex++;
				}
			}

			pos += 6 + subSize;
		}
	}

	static bool ScriptHasQuestStageAction(const std::string& scriptName, RE::FormID topicInfoFormID, RE::FormID topicFormID)
	{
		const std::string filePath = "Scripts/" + scriptName + ".pex";
		RE::BSResourceNiBinaryStream stream(filePath);
		if (!stream.good()) return false;

		std::vector<std::uint8_t> buffer;
		std::uint8_t byte;
		while (stream.get(byte)) {
			buffer.push_back(byte);
		}

		if (buffer.empty()) return false;

		auto containsInsensitive = [&](std::string_view keyword) -> bool {
			for (size_t i = 0; i + keyword.size() <= buffer.size(); ++i) {
				bool match = true;
				for (size_t j = 0; j < keyword.size(); ++j) {
					if (std::tolower(buffer[i + j]) != std::tolower(keyword[j])) {
						match = false;
						break;
					}
				}
				if (match) return true;
			}
			return false;
		};

		const std::string_view kwQuest = "Quest";
		const std::string_view kwStart = "Start";
		const std::string_view kwStop  = "Stop";
		const std::string_view kwSetStage = "SetStage";
		const std::string_view kwGetOwningQuest = "GetOwningQuest";

		const bool hasQuestRef = containsInsensitive(kwQuest);
		const bool hasStartOrStop = containsInsensitive(kwStart) || containsInsensitive(kwStop);
		const bool hasSetStage = containsInsensitive(kwSetStage);
		const bool hasGetOwningQuest = containsInsensitive(kwGetOwningQuest);

		switch (SettingsIni::iQuestPapyrusFilterMode) {
		case 0: // Start / Stop
			if (!(hasQuestRef && hasStartOrStop)) return false;
			break;
		case 1: // SetStage
			if (!hasSetStage) return false;
			break;
		case 2: // Both
			if (!(hasQuestRef && hasStartOrStop) && !hasSetStage) return false;
			break;
		default:
			return false;
		}

		for (size_t i = 0; i + kwSetStage.size() < buffer.size(); ++i) {
			bool matchSetStage = true;
			for (size_t j = 0; j < kwSetStage.size(); ++j) {
				if (std::tolower(buffer[i + j]) != std::tolower(kwSetStage[j])) {
					matchSetStage = false;
					break;
				}
			}
			if (!matchSetStage) continue;

			size_t startSearch = (i >= 32) ? i - 32 : 0;
			for (size_t k = startSearch; k < i; ++k) {
				bool matchQuest = true;
				for (size_t l = 0; l < kwGetOwningQuest.size() && k + l < i; ++l) {
					if (std::tolower(buffer[k + l]) != std::tolower(kwGetOwningQuest[l])) {
						matchQuest = false;
						break;
					}
				}

				if (!matchQuest) continue;

				auto* topic = RE::TESForm::LookupByID<RE::TESTopic>(topicFormID);
				if (!topic || !topic->ownerQuest) return false;

				auto* quest = topic->ownerQuest;
				bool typeNotNull = quest->GetType() != RE::QUEST_DATA::Type::kNone;
				bool hasObjective = false;
				for (auto* obj : quest->objectives) {
					if (obj) { hasObjective = true; break; }
				}

				switch (SettingsIni::iQuestLinkedQuestFilter) {
				case 0: // All quest types
					break;
				case 1: // Non-null type
					if (!typeNotNull) return false;
					break;
				case 2: // At least one objective
					if (!hasObjective) return false;
					break;
				case 3: // Non-null type OR at least one objective
					if (!typeNotNull && !hasObjective) return false;
					break;
				default:
					break;
				}
				
				return true;

				break;
			}
		}

		return (!SettingsIni::bQuestRequireLinkedQuest || hasGetOwningQuest);
	}

	static std::pair<std::string, std::string> SplitPathAndFunction(const std::string& fullPath)
	{
		size_t lastDot = fullPath.find_last_of('.');
		if (lastDot == std::string::npos) {
			return { "", fullPath };
		}
		std::string path = fullPath.substr(0, lastDot);
		std::string funcName = fullPath.substr(lastDot + 1);
		return { path, funcName };
	}

	static void HookSetEntryText(RE::GFxMovie* movie)
	{
		if (!movie) return;

		auto [path, func] = SplitPathAndFunction(SettingsIni::sQuestUISetEntryTextFunction);
		if (path.empty() || func.empty()) return;

		RE::GFxValue topicList;
		if (!movie->GetVariable(&topicList, path.c_str())) return;

		RE::GFxValue hookedFlag;
		if (topicList.GetMember("_isSetEntryTextHooked", &hookedFlag) && hookedFlag.IsBool() && hookedFlag.GetBool()) return;

		topicList.SetMember("_isSetEntryTextHooked", true);

		RE::GFxValue oldFunc;
		if (topicList.GetMember(func.c_str(), &oldFunc)) topicList.SetMember("SetEntryTextLegacy", oldFunc);

		RE::GFxValue funcVal;
		auto handler = new SetEntryTextHandler();
		movie->CreateFunction(&funcVal, handler);
		topicList.SetMember(func.c_str(), funcVal);
	}
	
	static bool SetUIVariablesVector(std::vector<int> funcUiFlags)
	{
		if (const auto ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
			RE::BSSpinLockGuard lock(ui->processMessagesLock);
			if (const auto menu = ui->GetMenu(RE::DialogueMenu::MENU_NAME); menu) {
				if (!menu || !menu->uiMovie || !menu->uiMovie->GetVisible()) return false;

				if (!SettingsIni::sQuestUIQuestColorVariable.empty()) {
					menu->uiMovie->SetVariable(SettingsIni::sQuestUIQuestColorVariable.c_str(), SettingsIni::cQuestUIQuestEntryColor);
				}
				if (!SettingsIni::sQuestUIQuestIconVisibleVariable.empty()) {
					menu->uiMovie->SetVariable(SettingsIni::sQuestUIQuestIconVisibleVariable.c_str(), SettingsIni::bQuestUIDisplayIcon);
				}

				uiFlags = funcUiFlags;
				HookSetEntryText(menu->uiMovie.get());

				return true;
			}
		}

		return false;
	}
};
