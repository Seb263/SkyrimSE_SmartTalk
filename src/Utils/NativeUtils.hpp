#pragma once

class NativeUtils
{
	public:

	template <typename T>
	static std::optional<T> GetGameINISetting(const std::string& name, std::optional<T> defaultValue = std::nullopt)
	{
		auto settings = RE::INISettingCollection::GetSingleton();
		if (!settings) return defaultValue;

		if (auto* setting = settings->GetSetting(name.c_str())) {

			static auto getNative = [](RE::Setting* setting) -> std::optional<std::variant<bool, float, int, unsigned int, std::string>> {
				switch (setting->GetType()) {
					case RE::Setting::Type::kBool: return setting->GetBool();
					case RE::Setting::Type::kFloat: return setting->GetFloat();
					case RE::Setting::Type::kInteger: return setting->GetInteger();
					case RE::Setting::Type::kUnsignedInteger: return setting->GetUnsignedInteger();
					case RE::Setting::Type::kString: return std::string(setting->GetString());
					default: return std::nullopt;
				}
			};

			static auto convertTo = [](auto& nativeValue, auto defaultVal) -> T {
				try {
					if (!nativeValue.has_value()) return defaultVal;

					const auto& v = nativeValue.value();

					if constexpr (std::is_same_v<T, std::string>) {
						if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? "true" : "false";
						if (std::holds_alternative<float>(v)) return std::to_string(std::get<float>(v));
						if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
						if (std::holds_alternative<unsigned int>(v)) return std::to_string(std::get<unsigned int>(v));
						if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
					}

					if constexpr (std::is_same_v<T, bool>) {
						if (std::holds_alternative<std::string>(v)) {
							std::string s = std::get<std::string>(v);
							std::transform(s.begin(), s.end(), s.begin(), ::tolower);
							if (s == "1" || s == "true" || s == "yes") return true;
							if (s == "0" || s == "false" || s == "no") return false;
							return defaultVal;
						}
						if (std::holds_alternative<float>(v)) return std::get<float>(v) != 0.0f;
						if (std::holds_alternative<int>(v)) return std::get<int>(v) != 0;
						if (std::holds_alternative<unsigned int>(v)) return std::get<unsigned int>(v) != 0;
					}

					if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
						if (std::holds_alternative<std::string>(v)) return static_cast<T>(std::stoi(std::get<std::string>(v)));
						if (std::holds_alternative<float>(v)) return static_cast<T>(std::get<float>(v));
						if (std::holds_alternative<int>(v)) return static_cast<T>(std::get<int>(v));
						if (std::holds_alternative<unsigned int>(v)) return static_cast<T>(std::get<unsigned int>(v));
					}

					if constexpr (std::is_floating_point_v<T>) {
						if (std::holds_alternative<std::string>(v)) return std::stof(std::get<std::string>(v));
						if (std::holds_alternative<float>(v)) return static_cast<T>(std::get<float>(v));
						if (std::holds_alternative<int>(v)) return static_cast<T>(std::get<int>(v));
						if (std::holds_alternative<unsigned int>(v)) return static_cast<T>(std::get<unsigned int>(v));
					}

					return defaultVal;
				} catch (...) {
					return defaultVal;
				}
			};

			auto nativeValue = getNative(setting);
			return convertTo(nativeValue, defaultValue.value_or(T{}));
		}

		return defaultValue;
	}

	static bool SetGameSetting(const std::string& settingName, const std::variant<bool, float, int32_t, uint32_t, std::string>& newValue)
	{
		auto* gsc = RE::GameSettingCollection::GetSingleton();
		if (!gsc || settingName.empty()) return false;

		auto* setting = gsc->GetSetting(settingName.c_str());
		if (!setting) {
			logger::warn("SetGameSetting: setting \"{}\" not found", settingName);
			return false;
		}

		using SettingType = RE::Setting::Type;
		auto settingType = setting->GetType();

		switch (settingType) {
			case SettingType::kBool: if (auto value = std::get_if<bool>(&newValue)) { setting->data.b = *value; return true; } break;
			case SettingType::kFloat: if (auto value = std::get_if<float>(&newValue)) { setting->data.f = *value; return true; } break;
			case SettingType::kInteger: if (auto value = std::get_if<int32_t>(&newValue)) { setting->data.i = *value; return true; } break;
			case SettingType::kUnsignedInteger: if (auto value = std::get_if<uint32_t>(&newValue)) { setting->data.u = *value; return true; } break;
			case SettingType::kString:
				if (auto value = std::get_if<std::string>(&newValue)) {
					free(setting->data.s);
					setting->data.s = _strdup(value->c_str());
					return true;
				} break;
			default: return false;
		}

		return false;
	}
};
