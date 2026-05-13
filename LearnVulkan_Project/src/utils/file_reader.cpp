#include "pch.h"

#include "utils/file_reader.h"

void IFileReader::EnforceFileExtension(const std::filesystem::path& _filePath, const std::string& _extension) {
	if (_filePath.extension() != _extension) {
		throw std::runtime_error(std::format("Expected *{} extension for file \"{}\"!\n", _extension, _filePath.filename().string()));
	}
}

void IFileReader::ReadWholeFile(const std::filesystem::path& _filepath, std::ios_base::openmode _mode, std::vector<char>& _outBuffer) {
	std::ifstream file(_filepath, _mode);

	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file! File path: " + _filepath.string());
	}

	// Since we started reading from the EOF, it is trivial to get the file size
	size_t fileSize = (size_t)(file.tellg());
	_outBuffer.resize(fileSize);

	// Return back to start of file, and read the entire thing at once
	file.seekg(0);
	file.read(_outBuffer.data(), fileSize);

	file.close();
}
