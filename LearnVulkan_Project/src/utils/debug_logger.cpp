#include "pch.h"
#include "utils/debug_logger.h"

#include "utils/config.h"

#include <chrono>
#include <format>

static std::vector<LogMessage> gPreInitMessages;

DebugLogger::DebugLogger()
{
	mLogFolder = Config::GetValue<std::string>("engine|systems|log_folder", "saved\\logs");
	verbosityLevelFile = static_cast<LogVerbosity>(Config::GetValue<uint8_t>("engine|systems|log_verbosity_level_file", 0));
	verbosityLevelConsole = static_cast<LogVerbosity>(Config::GetValue<uint8_t>("engine|systems|log_verbosity_level_console", 1));

	// Clamp loaded verbosity levels
	verbosityLevelFile = std::clamp(verbosityLevelFile, LogVerbosity::Info, LogVerbosity::Error);
	verbosityLevelConsole = std::clamp(verbosityLevelConsole, LogVerbosity::Info, LogVerbosity::Error);
}

void DebugLogger::OnInitialized() {
	const std::filesystem::path filePath(mLogFolder / "temp.log");

	// Create parent directory if it doesn't already exist
	if (!std::filesystem::exists(filePath.parent_path())) {
		std::filesystem::create_directories(filePath.parent_path());
	}

	// Open/create file for output (ios::out) + wipe file if not empty (ios::trunc)
	mLogFile = std::ofstream(filePath, std::ios::out | std::ios::trunc);

	LOG_MSG("Init", LogVerbosity::Info);

	// Write all messages that were invoked before we initialized
	LOG_MSG("--> Pre-Init Messages Start", LogVerbosity::Info);
	{
		for (const LogMessage& preMsg : gPreInitMessages) {
			WriteLog(preMsg);
		}

		gPreInitMessages.clear();
	}
	LOG_MSG("--> Pre-Init Messages End", LogVerbosity::Info);
}

void DebugLogger::OnCleanup() {
	LOG_MSG("Cleanup", LogVerbosity::Info);

	// File should auto-close, but can't hurt to be explicit
	mLogFile.close();
}

void DebugLogger::WriteLog(const LogMessage& _msg) {
	if (DebugLogger::HasValidInstance()) {
		// Always write to file (given high enough verbosity)
		if (_msg.verbosity >= DebugLogger::GetInstance().verbosityLevelFile) {
			DebugLogger::GetInstance().WriteFormattedMessage(_msg, DebugLogger::GetInstance().mLogFile);
		}

		// Only write to console when requested (given high enough verbosity)
		if (_msg.writeToConsole && _msg.verbosity >= DebugLogger::GetInstance().verbosityLevelConsole) {
			DebugLogger::GetInstance().WriteFormattedMessage(_msg, std::cout);
		}
	}
	else {
		// 'WriteLog' called before we have initialized, store for later
		gPreInitMessages.emplace_back(_msg);
	}
}

const std::string& DebugLogger::GetLastLog() {
	return GetInstance().mLastLog;
}

void DebugLogger::WriteFormattedMessage(const LogMessage& _msg, std::ostream& _stream) {
	std::string curMsg;

	// Prepend message with current date + time
	{
		using namespace std::chrono;

		const auto time = current_zone()->to_local(system_clock::now());
		curMsg += std::format("[{:%a %d %b %Y - %X}] ", time);
	}

	// Prepend message with 'WriteLog' call site
	{
		curMsg += std::format("[{}] ", _msg.callSite);
	}

	// Prepend message with verbosity string, i.e. 'LogInfo: '
	{
		curMsg += GetLogVerbosityString(_msg.verbosity) + ": ";
	}

	// Finally ... the message itself
	{
		curMsg += _msg.message;
	}

	_stream << curMsg << std::endl;

	// ... and store for later retrieval (why? someone might want it)
	mLastLog = curMsg;
}

constexpr std::string DebugLogger::GetLogVerbosityString(LogVerbosity _verbosity) {
	switch (_verbosity) {
		case LogVerbosity::Info:		return "LogInfo";
		case LogVerbosity::Warning:		return "LogWarning";
		case LogVerbosity::Error:		return "LogError";
		default:						return "LogUnknown";
	}
}

#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
std::string DebugLoggerImpl::DemangleClassName(const char* _name) {
	int status = 0;
	char* demangled = abi::__cxa_demangle(_name, nullptr, nullptr, &status);

	// Was demangling a success?
	if (status == 0 && demangled) {
		std::string result = demangled;
		std::free(demangled);
		return result;
	}

	// Fallback, couldn't demangle name
	return _name;
}
#endif
