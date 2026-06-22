#include "pch.h"
#include "utils/debug_logger.h"

#include <chrono>
#include <format>

void DebugLogger::OnInitialized() {
	const std::filesystem::path filePath("saved\\logs\\temp.log");

	// Create parent directory if it doesn't already exist
	if (!std::filesystem::exists(filePath.parent_path())) {
		std::filesystem::create_directories(filePath.parent_path());
	}

	// Open/create file for output (ios::out) + wipe file if not empty (ios::trunc)
	mLogFile = std::ofstream(filePath, std::ios::out | std::ios::trunc);

	LOG_MSG("Init", LogVerbosity::Info);
}

void DebugLogger::OnCleanup() {
	LOG_MSG("Cleanup", LogVerbosity::Info);

	// File should auto-close, but can't hurt to be explicit
	mLogFile.close();
}

void DebugLogger::WriteLog(const std::string& _msg, LogVerbosity _verbosity, const std::string& _callSite) {
	std::string curMsg;

	// Prepend message with current date + time
	{
		using namespace std::chrono;

		const auto time = current_zone()->to_local(system_clock::now());
		curMsg += std::format("[{:%a %d %b %Y - %X}] ", time);
	}

	// Prepend message with 'WriteLog' call site
	{
		curMsg += std::format("[{}] ", _callSite);
	}

	// Prepend message with verbosity string, i.e. 'LogInfo: '
	{
		curMsg += GetLogVerbosityString(_verbosity) + ": ";
	}

	// Finally ... the message itself
	{
		curMsg += _msg;
	}

	mLogFile << curMsg << std::endl;

	// ... and store for later retrieval (why? someone might want it)
	mLastLog = curMsg;
}

const std::string& DebugLogger::GetLastLog() {
	return GetInstance().mLastLog;
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
