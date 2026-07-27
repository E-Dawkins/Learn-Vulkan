#include "pch.h"

#include "app.h"
#include "utils/config.h"
#include "utils/debug_logger.h"
#include "utils/input_manager.h"

static void SafeShutdown() {
	// We have this as a separate function as we need this
	// to run in the case of a successful (normal) shutdown
	// but also in the case of any thrown exceptions.

	App::Shutdown();

	InputManager::Shutdown();
	Config::Shutdown();
	DebugLogger::Shutdown();
}

int main() {
	try {
		// Global systems (non App relevant)
		DebugLogger::Init("saved\\logs");
		Config::Init("config");
		InputManager::Init();

		// Main app
		App::Init();
		App::GetInstance().Run();

		SafeShutdown();
	}
	catch (const std::exception& e) {
		SafeShutdown();

		std::cerr << e.what() << "\n";
		std::cout << "\nPress Enter to continue...\n";
		std::cin.get();
		return 1;
	}

	return 0;
}
