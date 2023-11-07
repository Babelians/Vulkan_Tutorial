#pragma once

//#include <vulkan/vulkan.h>
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <optional>
#include <set>

using namespace std;

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

struct QueueFamilyIndices
{
	optional<uint32_t> graphicsFamily;
	optional<uint32_t> presentFamily;

	bool isComplete()
	{
		return graphicsFamily.has_value()
			&& presentFamily.has_value();
	}
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	vector<VkSurfaceFormatKHR> formats;
	vector<VkPresentModeKHR> presentModes;
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

class HelloTriangleApplication
{
public:
	void run();
private:
	void initWindow(const char* title);
	void initVulkan();
	void mainLoop();
	void cleanup();

	void createInstance();
	void selectPhysicalDevice();
	void createDevice();
	void createSurface();

	bool checkValidationLayerSupport();
	bool isDeviceSuitable(VkPhysicalDevice pDevice);
	bool checkDeviceExtensionSupport(VkPhysicalDevice pDevice);
	QueueFamilyIndices findQueueFamiles(VkPhysicalDevice pDevice);
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice pDevice);

	GLFWwindow* window;
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkSurfaceKHR surface;

	vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
};
