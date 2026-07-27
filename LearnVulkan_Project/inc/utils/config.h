#pragma once
#include "interfaces/singleton.h"

#include <unordered_map>

template<typename T>
concept ValidVarType =
	   std::is_same_v<T, std::string>
	|| std::is_integral_v<T> // size_t, int, bool, etc.
	|| std::is_floating_point_v<T>; // double, float, etc.

struct ConfigFile
{
	// var name -> var value
	using ConfigSection = std::unordered_map<std::string, std::string>;

	std::string name;
	std::unordered_map<std::string, ConfigSection> sections;
};

class Config : public ISingleton<Config>
{
private:
	std::unordered_map<std::string, ConfigFile> mConfigs;

public:
	Config(const std::filesystem::path& _configFolder);

	template<ValidVarType T>
	static T GetValue(const std::string& _varPath, const T& _default = {}) {
		std::string configName = "", sectionName = "", varName = "";
		Config::DeconstructVarPath(_varPath, configName, sectionName, varName);

		return Config::GetInstance().GetConfigValue<T>(configName, sectionName, varName, _default);
	}

private:
	void LoadAllConfigs(const std::filesystem::path& _configFolder);
	void LoadSingleConfig(const std::filesystem::path& _filePath);
	std::string WriteSection(ConfigFile& _config, const std::string& _line);
	void ProcessVar(ConfigFile& _config, const std::string& _section, std::string _line);

	static void DeconstructVarPath(std::string _varPath, std::string& _configName, std::string& _sectionName, std::string& _varName);

	std::string GetConfigValue_Raw(const std::string& _configName, const std::string& _sectionName, const std::string& _varName) {
		if (!mConfigs.contains(_configName)) return "";
		const ConfigFile& config = mConfigs.at(_configName);

		if (!config.sections.contains(_sectionName)) return "";
		const ConfigFile::ConfigSection& section = config.sections.at(_sectionName);

		if (!section.contains(_varName)) return "";

		return section.at(_varName);
	}

	template<ValidVarType T>
	T GetConfigValue(const std::string& _configName, const std::string& _sectionName, const std::string& _varName, const T& _default) {
		const std::string rawValue = GetConfigValue_Raw(_configName, _sectionName, _varName);

		// Could not find var, return default value
		if (rawValue.empty()) {
			return _default;
		}

		if constexpr (std::is_same_v<T, std::string>) {
			return rawValue;
		}
		else if constexpr (std::is_integral_v<T>) {
			// Can always down cast from size_t to other integral types
			return static_cast<T>(std::stoull(rawValue));
		}
		else if constexpr (std::is_floating_point_v<T>) {
			// Can always down cast from long double to other floating point types
			return static_cast<T>(std::stold(rawValue));
		}
	}
};