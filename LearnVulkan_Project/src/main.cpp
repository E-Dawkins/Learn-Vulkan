#include "pch.h"

#include "app.h"

int main() {
	try {
		App::Init();
		App::GetInstance().Run();
		App::Shutdown();
	}
	catch (const std::exception& e) {
		App::Shutdown();

		std::cerr << e.what() << "\n";
		std::cin.get();
		return 1;
	}

	return 0;
}
