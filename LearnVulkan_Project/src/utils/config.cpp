#include "pch.h"
#include "utils/config.h"

#include "utils/debug_logger.h"

#include <fstream>

Config::Config(const std::filesystem::path& _configFolder) {
	LoadAllConfigs(_configFolder);
}

void Config::LoadAllConfigs(const std::filesystem::path& _configFolder) {
	LOG_MSG("Loading all '*.ini' files from " + _configFolder.string(), LogVerbosity::Info);

	// This will only create the directory if it does *not* already exist... very handy :)
	std::filesystem::create_directory(_configFolder);

	for (const auto& entry : std::filesystem::recursive_directory_iterator(_configFolder)) {
		if (entry.path().extension() == ".ini") {
			LoadSingleConfig(entry.path());
		}
	}
}

void Config::LoadSingleConfig(const std::filesystem::path& _filePath) {
	LOG_MSG("Loading config file: " + _filePath.filename().string(), LogVerbosity::Info);

	// New config file
	const std::string stem = _filePath.stem().string();
	mConfigs[stem] = ConfigFile{ .name = stem };

	// Get ref for writing to
	ConfigFile& currentConfig = mConfigs[stem];

	std::string currentSection = "";

	std::ifstream file(_filePath);
	std::string line;
	while (std::getline(file, line)) {
		// Ignore empty lines
		if (line.empty()) continue;

		// Ignore lines beginning with '//'
		if (line.starts_with("//")) continue;

		// There *should* only be sections or vars left to process
		if (line.starts_with('[')) {
			currentSection = WriteSection(currentConfig, line);
		}
		else {
			ProcessVar(currentConfig, currentSection, line);
		}
	}
}

std::string Config::WriteSection(ConfigFile& _config, const std::string& _line) {
	// Double check section ends with a closing bracket
	size_t closeBracketPos = _line.find(']');
	if (closeBracketPos == _line.npos) {
		LOG_MSG(std::format("Section name incomplete. Config = '{}', Partial Section = '{}'", _config.name, _line), LogVerbosity::Warning);
		return "";
	}

	// Format of section = '[sectionName]'
	const std::string sectionName = _line.substr(1, closeBracketPos - 1);

	// Only insert if it doesn't already exist. This allows
	// us to add to sections throughout a config file.
	if (!_config.sections.contains(sectionName)) {
		_config.sections.insert({ sectionName, {} });
	}

	return sectionName;
}

void Config::ProcessVar(ConfigFile& _config, const std::string& _section, std::string _line) {
	// Remove any trailing comments
	size_t commentPos = _line.find("//");
	if (commentPos != _line.npos) {
		_line.erase(commentPos);
	}

	// Remove all spaces, except if they are within quotation marks
	bool bWithinQuotes = false;
	auto is_space = [&bWithinQuotes](unsigned char _c) {
		if (_c == '"') {
			bWithinQuotes = !bWithinQuotes;
			return true; // trim quotes
		}

		// Ignore spaces that are within quotes
		return !bWithinQuotes && std::isspace(_c);
	};
	std::erase_if(_line, is_space);

	// Format of var = 'varName=varValue'
	size_t equalPos = _line.find('=');
	if (equalPos == _line.npos) {
		LOG_MSG(std::format("Malformed config var. Section = '{}', Partial Var = '{}'", _section, _line), LogVerbosity::Warning);
		return;
	}

	const std::string varName = _line.substr(0, equalPos);
	const std::string varValue = _line.substr(equalPos + 1);

	ConfigFile::ConfigSection& section = _config.sections[_section];
	section[varName] = varValue;
}

void Config::DeconstructVarPath(std::string _varPath, std::string& _configName, std::string& _sectionName, std::string& _varName) {
	// Path format = 'configName|sectionName|varName'

	size_t offset = _varPath.find('|');
	if (offset != _varPath.npos) {
		_configName = _varPath.substr(0, offset);
		_varPath.erase(0, offset + 1);
	}

	offset = _varPath.find('|');
	if (offset != _varPath.npos) {
		_sectionName = _varPath.substr(0, offset);
		_varPath.erase(0, offset + 1);
	}

	_varName = _varPath; // remainder must be the var name
}
