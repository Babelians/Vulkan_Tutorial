#include <iostream>

#include "my_vulkan.hpp"

int main()
{
	Vulkan app;

	try {
		app.run();
	}
	catch (const exception& e)
	{
		cerr << e.what() << endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
