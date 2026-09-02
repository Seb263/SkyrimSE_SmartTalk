#pragma once

namespace ModData
{
	constexpr std::string_view MOD_NAME = "Smart Talk";

	struct PluginForm
	{
		std::string_view name;
		void**           formPtr;
		uint32_t         formID;
		std::string_view pluginName;
		bool             optional = false;
	};
	
	struct DefaultForm
	{
		void**           formPtr;
		std::string      formStr;
	};

	// Properties storing game form references

	static inline const std::vector<PluginForm> pluginForms = {};

	inline RE::TESDataHandler* TESdataHandler;

	inline std::vector<RE::FormID> questTopicInfoList;
}
