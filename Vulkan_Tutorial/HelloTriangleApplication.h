#pragma once

#define NOMINMAX // windows.hのmin maxマクロの無効化
//#include <vulkan/vulkan.h>
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstdint> //uint32_t
#include <vector>
#include <cstring>
#include <optional>
#include <set>
#include <limits> // numeric_limit
#include <algorithm> // clamp
#include <fstream>
#include <string>

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
	void createSwapChain();
	void createImageViews();
	void createGraphicsPipeline();

	bool checkValidationLayerSupport();
	bool isDeviceSuitable(VkPhysicalDevice pDevice);
	bool checkDeviceExtensionSupport(VkPhysicalDevice pDevice);
	static vector<char> readFile(const string& filename)
	{
		ifstream file(filename, ios::ate | ios::binary);

		if (!file.is_open())
		{
			throw runtime_error("failed to open file");
		}

		size_t fileSize = (size_t)file.tellg();
		vector<char>buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();

		return buffer;
	}

	QueueFamilyIndices findQueueFamiles(VkPhysicalDevice pDevice);
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice pDevice);
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	VkShaderModule createShaderModule(vector<char> &code);

	GLFWwindow* window;
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapChain;
	vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	vector<VkImageView> swapChainImageViews;

	vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
};
