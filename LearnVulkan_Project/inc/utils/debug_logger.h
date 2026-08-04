#pragma once
#include "interfaces/singleton.h"

enum class LogVerbosity : uint8_t
{
	Info = 0,
	Warning,
	Error
};

struct LogMessage
{
	std::string message;
	std::string callSite;
	LogVerbosity verbosity;
	bool writeToConsole;
};

class DebugLogger : public ISingleton<DebugLogger>
{
public:
	// Only messages with at least this level of verbosity will be written to log file
	LogVerbosity verbosityLevelFile = LogVerbosity::Info;

	// Only messages with at least this level of verbosity will be written to the console
	LogVerbosity verbosityLevelConsole = LogVerbosity::Warning;

private:
	std::filesystem::path mLogFolder;
	std::ofstream mLogFile;
	std::string mLastLog;

public:
	DebugLogger();

private:
	void OnInitialized() override;
	void OnCleanup() override;

public:
	// Writes '_msg' to log file (and optionally console) in the format:
	// '[d DD MMM YYYY - HH:MM:SS] [_callSite] _verbosity: _message'
	static void WriteLog(const LogMessage& _msg);

	// Retrieve last written log message
	static const std::string& GetLastLog();

private:
	void WriteFormattedMessage(const LogMessage& _msg, std::ostream& _stream);
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
	DebugLogger::WriteLog(LogMessage{ msg, CALL_SITE(), verb, false });

#define LOG_MSG_CON(msg, verb) \
	DebugLogger::WriteLog(LogMessage{ msg, CALL_SITE(), verb, true });