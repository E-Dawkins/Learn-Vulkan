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

void IFileReader::ReadFileAsAil(const std::filesystem::path& _filepath, AilReader& _outReader) {
	std::ifstream file(_filepath);

	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file! File path: " + _filepath.string());
	}

	_outReader.Reset();
	_outReader.Parse(file);
}

AilReader::~AilReader() {
	Reset();
}

void AilReader::Reset() {
	delete mRootNode;
	mRootNode = nullptr;

	while (!mNodeStack.empty()) {
		mNodeStack.pop();
	}
}

void AilReader::Parse(std::ifstream& _file) {
	mRootNode = new AilNode();
	mRootNode->name = "root";
	mNodeStack.push(mRootNode);

	for (std::string line; std::getline(_file, line);) {
		Trim(line);

		// Skip empty lines
		if (line.empty()) {
			continue;
		}

		// End of 'type'
		if (line == "}") {
			mNodeStack.pop();
			continue;
		}

		if (line.starts_with("type:")) {
			size_t bracePos = line.find("{");
			line = line.substr(5, bracePos - 5);

			AilNode* typeNode = new AilNode();
			typeNode->name = line;

			mNodeStack.top()->subnodes.push_back(typeNode);
			mNodeStack.push(typeNode);
		}
		else {
			size_t initialPos = line.find(',');

			// Split by ','
			if (initialPos != std::string::npos) {
				AilNode* elemNode = new AilNode();

				mNodeStack.top()->subnodes.push_back(elemNode);

				for (size_t pos = initialPos; pos != std::string::npos; pos = line.find(',')) {
					AilNode* dataNode = new AilNode();
					dataNode->value = line.substr(0, pos);

					elemNode->subnodes.push_back(dataNode);

					line.erase(0, pos + 1); // +1 for delimiter
				}

				// Final element
				if (!line.empty()) {
					AilNode* dataNode = new AilNode();
					dataNode->value = line;

					elemNode->subnodes.push_back(dataNode);
				}
			}
			else {
				initialPos = line.find('=');

				AilNode* dataNode = new AilNode();

				if (initialPos != std::string::npos) {
					dataNode->name = line.substr(0, initialPos);
					dataNode->value = line.substr(initialPos + 1);
				}
				else {
					dataNode->value = line;
				}

				mNodeStack.top()->subnodes.push_back(dataNode);
			}
		}
	}
}

void AilReader::PrintNode(AilNode* _node, size_t _indent) {
	if (!_node) {
		return;
	}

	if (_indent > 0) {
		for (size_t i = 0; i < _indent - 1; i++) {
			std::cout << '\t';
		}

		if (_node->name.empty()) _node->name = "''";
		if (_node->value.empty()) _node->value = "''";

		std::cout << "" << _node->name << "\t";
		std::cout << "" << _node->value << "\n";
	}

	for (AilNode* sn : _node->subnodes) {
		PrintNode(sn, _indent + 1);
	}
}

AilNode* AilReader::GetNode(std::string _nodePath) const {
	AilNode* current = mRootNode;

	auto find_by_index = [&](const std::string& _nameToFind) {
		if (std::all_of(_nameToFind.begin(), _nameToFind.end(), ::isdigit)) {
			size_t index = std::stoull(_nameToFind);

			if (index < current->subnodes.size()) {
				current = current->subnodes[index];
				return true;
			}
		}

		return false;
	};

	auto find_by_name = [&](const std::string& _nameToFind) {
		for (AilNode* sn : current->subnodes) {
			if (sn->name == _nameToFind) {
				current = sn;
				return true;
			}
		}

		return false;
	};

	for (size_t pipePos = _nodePath.find('|'); pipePos != std::string::npos; pipePos = _nodePath.find('|')) {
		std::string nameToFind = _nodePath.substr(0, pipePos);
		_nodePath.erase(0, pipePos + 1);

		if (!find_by_index(nameToFind)) {
			if (!find_by_name(nameToFind)) {
				return nullptr;
			}
		}
	}

	// Final path element
	if (!find_by_index(_nodePath)) {
		if (!find_by_name(_nodePath)) {
			return nullptr;
		}
	}

	return current;
}

std::string AilReader::GetAsStr(const std::string& _nodePath, bool _trimQuotations) const {
	AilNode* node = GetNode(_nodePath);

	if (node) {
		if (_trimQuotations) {
			if (node->value.front() == '"') node->value.erase(0, 1);
			if (node->value.back() == '"') node->value.erase(node->value.size() - 1);
		}

		return node->value;
	}
	
	return "";
}

size_t AilReader::GetAsInt(const std::string& _nodePath) const {
	std::string nodeVal = GetAsStr(_nodePath);

	if (!nodeVal.empty()) {
		return std::stoull(nodeVal);
	}

	return 0ull;
}

float AilReader::GetAsFloat(const std::string& _nodePath) const {
	std::string nodeVal = GetAsStr(_nodePath);

	if (!nodeVal.empty()) {
		return std::stof(nodeVal);
	}

	return 0.f;
}

glm::vec4 AilReader::GetAsVec4(const std::string& _nodePath) const {
	AilNode* node = GetNode(_nodePath);

	if (node) {
		glm::vec4 out = {};
		for (glm::length_t i = 0; i < std::min(node->subnodes.size(), 4ull); i++) {
			out[i] = std::stof(node->subnodes[i]->value);
		}
		return out;
	}

	return glm::vec4();
}

glm::vec3 AilReader::GetAsVec3(const std::string& _nodePath) const {
	return glm::vec3(GetAsVec4(_nodePath));
}

glm::vec2 AilReader::GetAsVec2(const std::string& _nodePath) const {
	return glm::vec2(GetAsVec4(_nodePath));
}

// Following trim functions taken from here:
// https://stackoverflow.com/questions/216823/how-can-i-trim-a-stdstring
void AilReader::LTrim(std::string& _str) {
	_str.erase(_str.begin(), std::find_if(_str.begin(), _str.end(), [](unsigned char _char) {
		return !std::isspace(_char);
	}));
}

void AilReader::RTrim(std::string& _str) {
	_str.erase(std::find_if(_str.rbegin(), _str.rend(), [](unsigned char _char) {
		return !std::isspace(_char);
	}).base(), _str.end());
}

void AilReader::Trim(std::string& _str) {
	RTrim(_str);
	LTrim(_str);
}
