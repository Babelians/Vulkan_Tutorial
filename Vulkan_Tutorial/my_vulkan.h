#pragma once

#define NOMINMAX // windows.h��min max�}�N���̖�����
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
#include <glm/glm.hpp>
#include <array>

using namespace std;

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;
const uint32_t MAX_FRAME_IN_FLIGHT = 2;

struct QueueFamilyIndices
{
	optional<uint32_t> graphicsFamily;
	optional<uint32_t> presentFamily;
	optional<uint32_t> transferFamily;

	bool isComplete()
	{
		return graphicsFamily.has_value() &&
			   presentFamily.has_value()  &&
			   transferFamily.has_value();
	}
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	vector<VkSurfaceFormatKHR> formats;
	vector<VkPresentModeKHR> presentModes;
};

struct Vertex
{
	glm::vec2 pos;
	glm::vec3 color;

	static VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	static array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
	{
		array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);
		return attributeDescriptions;
	}
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

class Vulkan
{
public:
	void run();
private:
	void initWindow(const char* title);
	void initVulkan();
	void mainLoop();
	void cleanup();
	void cleanupSwapChain();

	void createInstance();
	void selectPhysicalDevice();
	void createDevice();
	void createSurface();
	void createSwapChain();
	void createImageViews();
	void createGraphicsPipeline();
	void createRenderPass();
	void createFrameBuffers();
	void createCommandPools();
	void createCommandPool(VkCommandPool *commandPool, uint32_t index);
	void createCommandBuffer();
	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void drawFrame();
	void createSyncObjects();
	void recreateSwapChain();
	void createBuffer(size_t size, VkBuffer *buffer, VkDeviceMemory *deviceMemory, VkBufferUsageFlags usage, VkMemoryPropertyFlags props);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, size_t size);
	void createVertexBuffer(void *data, size_t size);
	void createIndexBuffer(void* data, size_t size);

	bool checkValidationLayerSupport();
	bool isDeviceSuitable(VkPhysicalDevice pDevice);
	bool checkDeviceExtensionSupport(VkPhysicalDevice pDevice);
	static void framebufferResizeCallback(GLFWwindow *window, int width, int height);
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);


	QueueFamilyIndices findQueueFamiles(VkPhysicalDevice pDevice);
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice pDevice);
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	VkShaderModule createShaderModule(vector<char> &code);

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

	GLFWwindow* window;
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkQueue transferQueue;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapChain;
	vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	vector<VkImageView> swapChainImageViews;
	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	vector<VkFramebuffer>swapChainFramebuffers;
	VkCommandPool commandPool;
	VkCommandPool transferPool;
	vector<VkCommandPool>commandPools;
	vector<VkCommandBuffer> commandBuffers;
	vector<VkSemaphore> imageAvailableSemaphores;
	vector<VkSemaphore> renderFinishedSemaphores;
	vector<VkFence> inFlightFences;
	bool framebufferResized = false;
	uint32_t currentFrame = 0;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;

	vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	vector<Vertex> vertices{
		{{-0.5f,-0.5f},{1.0f,0.0f,0.0f}},
		{{0.5f,-0.5f},{0.0f,1.0f,0.0f}},
		{{0.5f,0.5f},{0.0f,0.0f,1.0f}},
		{{-0.5f,0.5f},{1.0f,1.0f,1.0f}}
	};

	vector<uint32_t>indices{
		0, 1, 2, 2, 3, 0
	};
};
