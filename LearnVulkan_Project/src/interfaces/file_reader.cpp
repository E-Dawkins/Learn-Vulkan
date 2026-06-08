#include "pch.h"

#include "interfaces/file_reader.h"

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

AilNode::AilNode(const std::string& _name, const std::string& _value) 
	: mName(_name), mValue(_value) {}

OptionalNode<const AilNode> AilNode::TryGetSubnode(size_t _index) const {
	if (_index < subnodes.size()) {
		return subnodes[_index];
	}

	return std::nullopt;
}

OptionalNode<const AilNode> AilNode::TryGetSubnode(const std::string& _name) const {
	for (const AilNode& sn : subnodes) {
		if (sn.mName == _name) {
			return sn;
		}
	}

	return std::nullopt;
}

glm::vec4 AilNode::ValAsVec4() const {
	glm::vec4 out = {};

	for (glm::length_t i = 0; i < std::min(subnodes.size(), 4ull); i++) {
		out[i] = std::stof(subnodes[i].mValue);
	}

	return out;
}

std::string AilNode::GetAsStr(bool _trimQuotations) const {
	size_t start = 0, count = mValue.size();

	if (_trimQuotations) {
		if (mValue.front() == '"') {
			start++;
			count--;
		}
		if (mValue.back() == '"') count--;
	}

	return mValue.substr(start, count);
}

AilReader::~AilReader() {
	Reset();
}

void AilReader::Reset() {
	mRootNode = AilNode();
}

void AilReader::Parse(std::ifstream& _file) {
	mRootNode = AilNode("root");

	std::stack<AilNode*> nodeStack;
	nodeStack.push(&mRootNode);

	for (std::string line; std::getline(_file, line);) {
		Trim(line);

		// Skip empty lines
		if (line.empty()) {
			continue;
		}

		// End of 'type'
		if (line == "}") {
			nodeStack.pop();
			continue;
		}

		if (line.starts_with("type:")) {
			size_t bracePos = line.find("{");
			line = line.substr(5, bracePos - 5);

			nodeStack.top()->subnodes.emplace_back(AilNode(line));
			AilNode& typeNode = nodeStack.top()->subnodes.back();

			nodeStack.push(&typeNode);
		}
		else {
			size_t initialPos = line.find(',');

			// Split by ','
			if (initialPos != std::string::npos) {
				nodeStack.top()->subnodes.emplace_back(AilNode());
				AilNode& elemNode = nodeStack.top()->subnodes.back();

				for (size_t pos = initialPos; pos != std::string::npos; pos = line.find(',')) {
					elemNode.subnodes.emplace_back(AilNode("", line.substr(0, pos)));

					line.erase(0, pos + 1); // +1 for delimiter
				}

				// Final element
				if (!line.empty()) {
					elemNode.subnodes.emplace_back(AilNode("", line));
				}
			}
			else {
				initialPos = line.find('=');

				if (initialPos != std::string::npos) {
					nodeStack.top()->subnodes.emplace_back(AilNode(line.substr(0, initialPos), line.substr(initialPos + 1)));
				}
				else {
					nodeStack.top()->subnodes.emplace_back(AilNode("", line));
				}
			}
		}
	}
}

void AilReader::PrintNode(const AilNode& _node, size_t _indent) {
	if (_indent > 0) {
		for (size_t i = 0; i < _indent - 1; i++) {
			std::cout << '\t';
		}

		std::string name = _node.GetRawName(), value = _node.GetRawValue();
		if (name.empty()) name = "''";
		if (value.empty()) value = "''";

		std::cout << "" << name << "\t";
		std::cout << "" << value << "\n";
	}

	for (const AilNode& sn : _node.subnodes) {
		PrintNode(sn, _indent + 1);
	}
}

OptionalNode<const AilNode> AilReader::TryGetNode(std::string _nodePath) const {
	const AilNode* current = &mRootNode;

	auto find_by_index = [&](const std::string& _nameToFind) {
		if (std::all_of(_nameToFind.begin(), _nameToFind.end(), ::isdigit)) {
			size_t index = std::stoull(_nameToFind);

			if (index < current->subnodes.size()) {
				current = &current->subnodes[0];
				return true;
			}
		}

		return false;
	};

	auto find_by_name = [&](const std::string& _nameToFind) {
		for (const AilNode& sn : current->subnodes) {
			if (sn.GetRawName() == _nameToFind) {
				current = &sn;
				return true;
			}
		}

		return false;
	};

	for (size_t pipePos = _nodePath.find('|'); pipePos != std::string::npos; pipePos = _nodePath.find('|')) {
		std::string nameToFind = _nodePath.substr(0, pipePos);
		_nodePath.erase(0, pipePos + 1);

		if (!find_by_index(nameToFind) && !find_by_name(nameToFind)) {
			return std::nullopt;
		}
	}

	// Final path element
	if (!find_by_index(_nodePath) && !find_by_name(_nodePath)) {
		return std::nullopt;
	}
	
	return *current;
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
