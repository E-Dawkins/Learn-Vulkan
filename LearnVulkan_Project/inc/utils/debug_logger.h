#pragma once
#include "interfaces/singleton.h"

enum class LogVerbosity
{
	Info,
	Warning,
	Error
};

class DebugLogger : public ISingleton<DebugLogger>
{
private:
	std::filesystem::path mLogFolder;
	std::ofstream mLogFile;
	std::string mLastLog;

public:
	DebugLogger(const std::filesystem::path& _logFolder);

private:
	void OnInitialized() override;
	void OnCleanup() override;

public:
	// Writes '_msg' to log file in the format:
	// '[d DD MMM YYYY - HH:MM:SS] [_callSite] _verbosity: _msg'
	void WriteLog(const std::string& _msg, LogVerbosity _verbosity, const std::string& _callSite);

	// Retrieve last written log message
	static const std::string& GetLastLog();

private:
	static constexpr std::string GetLogVerbosityString(LogVerbosity _verbosity);
};

namespace DebugLoggerImpl {
	// Only need a demangle function in GCC / Clang
	#if defined(__GNUC__) || defined(__clang__)
		std::string DemangleClassName(const char* _name);
	#endif
}

// Try to get the call site ('ClassName::FuncName'), if compiler is recognized
#if defined(_WIN32)
	#define CALL_SITE() __FUNCTION__
#elif defined(__GNUC__) || defined(__clang__)
	#define CALL_SITE() \
		std::format("{}::{}", DebugLoggerImpl::DemangleClassName(typeid(*this).name()), __func__)
#else
	#define CALL_SITE() __func__
#endif

#define LOG_MSG(msg, verb) \
	DebugLogger::GetInstance().WriteLog(msg, verb, CALL_SITE());