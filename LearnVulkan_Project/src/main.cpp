#include "pch.h"

#include "app.h"

int main() {
	App app;

	try {
		app.Run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << "\n";
		std::cin.get();
		return 1;
	}

	return 0;
}
