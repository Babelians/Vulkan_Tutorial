#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>

#include "HelloTriangleApplication.h"

using namespace std;
using namespace glm;

#pragma comment(lib, "vulkan-1.lib")

int main()
{
	HelloTriangleApplication app;

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
