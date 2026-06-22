#include "pch.h"

#include "app.h"
#include "utils/debug_logger.h"

int main() {
	try {
		DebugLogger::Init();
		{
			App::Init();
			App::GetInstance().Run();
			App::Shutdown();
		}
		DebugLogger::Shutdown();
	}
	catch (const std::exception& e) {
		{
			App::Shutdown();
		}
		DebugLogger::Shutdown();

		std::cerr << e.what() << "\n";
		std::cout << "\nPress Enter to continue...\n";
		std::cin.get();
		return 1;
	}

	return 0;
}
