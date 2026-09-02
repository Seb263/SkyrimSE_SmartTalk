#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Features/Quest.hpp"

#include "Utils/ModUtils.hpp"

namespace Papyrus
{
	bool GetIniSettingsValueBool(RE::StaticFunctionTag*, const RE::BSFixedString path, const bool defaultValue = false)
	{
		return SettingsIni::SettingsManager().GetValue<bool>(path.c_str(), defaultValue);
	}

	int GetIniSettingsValueInt(RE::StaticFunctionTag*, const RE::BSFixedString path, const int defaultValue = 0)
	{
		return SettingsIni::SettingsManager().GetValue<int>(path.c_str(), defaultValue);
	}

	float GetIniSettingsValueFloat(RE::StaticFunctionTag*, const RE::BSFixedString path, const float defaultValue = 0.0f)
	{
		return SettingsIni::SettingsManager().GetValue<float>(path.c_str(), defaultValue);
	}

	RE::BSFixedString GetIniSettingsValueString(RE::StaticFunctionTag*, const RE::BSFixedString path, const RE::BSFixedString defaultValue = "")
	{
		return RE::BSFixedString(SettingsIni::SettingsManager().GetValue<std::string>(path.c_str(), std::string(defaultValue.c_str())));
	}

	void SetIniSettingsValueBool(RE::StaticFunctionTag*, const RE::BSFixedString path, const bool value)
	{
		SettingsIni::SettingsManager().SetValue(path.c_str(), value);
	}

	void SetIniSettingsValueInt(RE::StaticFunctionTag*, const RE::BSFixedString path, const int value)
	{
		SettingsIni::SettingsManager().SetValue(path.c_str(), value);
	}

	void SetIniSettingsValueFloat(RE::StaticFunctionTag*, const RE::BSFixedString path, const float value)
	{
		SettingsIni::SettingsManager().SetValue(path.c_str(), value);
	}

	void SetIniSettingsValueString(RE::StaticFunctionTag*, const RE::BSFixedString path, const RE::BSFixedString value)
	{
		SettingsIni::SettingsManager().SetValue(path.c_str(), std::string(value.c_str()));
	}

	void RequestRuntimeUpdate(RE::StaticFunctionTag*)
	{
		SettingsIni::SettingsManager().ReadSettings();

		if (SettingsIni::bAsynchronousStartup) {
			std::jthread(Quest::Initialize).detach();
		} else {
			Quest::Initialize();
		}
	}

	std::vector<uint32_t> GetSmartTalkVersion(RE::StaticFunctionTag*)
	{
		using namespace SKSE;
        const auto* plugin = PluginDeclaration::GetSingleton();
        auto version = plugin->GetVersion();

        uint32_t versionMajor = plugin->GetVersion().major();
        uint32_t versionMinor = plugin->GetVersion().minor();
        uint32_t versionPatch = plugin->GetVersion().patch();

		std::vector<uint32_t> versionVector;
		versionVector.push_back(versionMajor);
		versionVector.push_back(versionMinor);
		versionVector.push_back(versionPatch);

		return versionVector;
	}

	bool BindPapyrusFunctions(RE::BSScript::IVirtualMachine* vm)
	{
		vm->RegisterFunction("GetIniSettingsValueBool", "SmartTalk", GetIniSettingsValueBool);
		vm->RegisterFunction("GetIniSettingsValueInt", "SmartTalk", GetIniSettingsValueInt);
		vm->RegisterFunction("GetIniSettingsValueFloat", "SmartTalk", GetIniSettingsValueFloat);
		vm->RegisterFunction("GetIniSettingsValueString", "SmartTalk", GetIniSettingsValueString);
		vm->RegisterFunction("SetIniSettingsValueBool", "SmartTalk", SetIniSettingsValueBool);
		vm->RegisterFunction("SetIniSettingsValueInt", "SmartTalk", SetIniSettingsValueInt);
		vm->RegisterFunction("SetIniSettingsValueFloat", "SmartTalk", SetIniSettingsValueFloat);
		vm->RegisterFunction("SetIniSettingsValueString", "SmartTalk", SetIniSettingsValueString);
		vm->RegisterFunction("RequestRuntimeUpdate", "SmartTalk", RequestRuntimeUpdate);
		vm->RegisterFunction("GetSmartTalkVersion", "SmartTalk", GetSmartTalkVersion);
		return true;
	}
};
