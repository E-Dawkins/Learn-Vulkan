#pragma once
#include <filesystem>

class IFileReader
{
protected:
	void EnforceFileExtension(const std::filesystem::path& _filePath, const std::string& _extension);

	void ReadWholeFile(const std::filesystem::path& _filepath, std::ios_base::openmode _mode, std::vector<char>& _outBuffer);
};