#include "HelloTriangleApplication.h"

void HelloTriangleApplication::run()
{
	initWindow("Ushinokoku");
	initVulkan();
	mainLoop();
	cleanup();
}

void HelloTriangleApplication::initWindow(const char* title)
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(WIDTH, HEIGHT, title, nullptr, nullptr);
}

void HelloTriangleApplication::initVulkan()
{
	createInstance();
	createSurface();
	selectPhysicalDevice();
	createDevice();
	createSwapChain();
	createImageViews();
	createGraphicsPipeline();
}

void HelloTriangleApplication::mainLoop()
{
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}
}

void HelloTriangleApplication::cleanup()
{
	for (auto imageView : swapChainImageViews)
	{
		vkDestroyImageView(device, imageView, nullptr);
	}
	vkDestroySwapchainKHR(device, swapChain, nullptr);
	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr); // ��ԍŌ�ɔj������Vulkan�I�u�W�F�N�g
	glfwDestroyWindow(window);
	glfwTerminate();
}

void HelloTriangleApplication::createInstance()
{
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Triangle";
	appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	VkInstanceCreateInfo instanceCI{};
	instanceCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCI.pApplicationInfo = &appInfo;
	instanceCI.enabledExtensionCount = glfwExtensionCount;
	instanceCI.ppEnabledExtensionNames = glfwExtensions;
	instanceCI.enabledLayerCount = 0;

	if (enableValidationLayers)
	{
		instanceCI.enabledLayerCount = uint32_t( validationLayers.size() );
		instanceCI.ppEnabledLayerNames = validationLayers.data();
	}

	if (vkCreateInstance(&instanceCI, nullptr, &instance) != VK_SUCCESS)
	{
		throw runtime_error("failed to createinstance");
	}

	// Vulkan�g���T�|�[�g�`�F�b�N
	vector<VkExtensionProperties> extensions;
	{
		uint32_t count = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
		extensions.resize(count);
		vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());
	}

	/*for (const auto& e : extensions)
	{
		cout << "\t" << e.extensionName << "\n";
	}*/

	// �o���f�[�V���������p�\���`�F�b�N
	if (enableValidationLayers && !checkValidationLayerSupport())
	{
		throw runtime_error("validation layers requested, but not available!");
	}
}


bool HelloTriangleApplication::checkValidationLayerSupport()
{
	vector<VkLayerProperties> availableLayers;
	{
		uint32_t count;
		vkEnumerateInstanceLayerProperties(&count, nullptr);
		availableLayers.resize(count);
		vkEnumerateInstanceLayerProperties(&count, availableLayers.data());
	}

	for (const char* layer_name : validationLayers)
	{
		bool layerFound = false;

		for (const auto& l : availableLayers)
		{
			if (strcmp(layer_name, l.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound) return false;
	}

	return true;
}

void HelloTriangleApplication::selectPhysicalDevice()
{
	// �S�Ă�GPU���擾
	vector<VkPhysicalDevice> pDevices;
	{
		uint32_t count;
		vkEnumeratePhysicalDevices(instance, &count, nullptr);
		if (count == 0) throw runtime_error("failed to find GPUs with Vulkan Support!");
		pDevices.resize(count);
		vkEnumeratePhysicalDevices(instance, &count, pDevices.data());
	}

	// �K����GPU��I��
	for (const auto& p : pDevices)
	{
		if (isDeviceSuitable(p))
		{
			physicalDevice = p;
			break;
		}
	}

	if (physicalDevice == VK_NULL_HANDLE)
	{
		throw runtime_error("failed to find a asuitable GPU!");
	}
}

bool HelloTriangleApplication::isDeviceSuitable(VkPhysicalDevice pDevice)
{
	// GPU�̓K�������`�F�b�N
	VkPhysicalDeviceProperties properties{};
	VkPhysicalDeviceFeatures features{};
	vkGetPhysicalDeviceProperties(pDevice, &properties);
	vkGetPhysicalDeviceFeatures(pDevice, &features);
	
	// GPU�ɔ�����Ă���L���[�t�@�~���𒲂ׁA�C���f�b�N�X�����
	QueueFamilyIndices indices = findQueueFamiles(pDevice);

	// �K�v�ȃf�o�C�X�g�����g���邩
	bool extensionSupported = checkDeviceExtensionSupport(pDevice);

	// �X���b�v�`�F�C���̃T�|�[�g���\����
	bool swapChainAdequate = false;
	if (extensionSupported) // ���X���b�v�`�F�C����KHR�ł���g���ł��邩��
	{
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(pDevice);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	// �e�K�����̘_���ς�Ԃ�
	return indices.isComplete() && extensionSupported && swapChainAdequate;
}

// ����̃L���[�t�@�~���C���f�b�N�X�̍\���̂�Ԃ�
QueueFamilyIndices HelloTriangleApplication::findQueueFamiles(VkPhysicalDevice pDevice)
{
	QueueFamilyIndices indices{};

	// GPU�̃L���[�t�@�~���ꗗ���擾
	vector<VkQueueFamilyProperties> queueFamilyProps;
	{
		uint32_t count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(pDevice, &count, nullptr);
		queueFamilyProps.resize(count);
		vkGetPhysicalDeviceQueueFamilyProperties(pDevice, &count, queueFamilyProps.data());
	}

	for (uint32_t i = 0; i < uint32_t( queueFamilyProps.size() ); i++)
	{
		// i�Ԗڂ̃L���[�t�@�~���C���f�b�N�X���O���t�B�b�N�L���[��
		if (queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = i;
		}

		// i�Ԗڂ̃L���[�t�@�~���C���f�b�N�X���C���[�W�������邩
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(pDevice, i, surface, &presentSupport);
		if (presentSupport) indices.presentFamily = i;

		if (indices.isComplete()) break;
	}

	return indices;
}

void HelloTriangleApplication::createDevice()
{
	// �L���[�t�@�~���\���̎擾
	QueueFamilyIndices qIndices = findQueueFamiles(physicalDevice);

	vector<VkDeviceQueueCreateInfo> queueCIs; // �L���[�t�@�~����CreateInfo�̔z��
	set<uint32_t> uniqueQueueFamilies = {    // �L���[�t�@�~���̔z��
		qIndices.graphicsFamily.value(),
		qIndices.presentFamily.value() 
	};

	float queue_priority = 1.0f;
	for (uint32_t q : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCI{};
		queueCI.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCI.queueFamilyIndex = q;
		queueCI.queueCount = 1;
		queueCI.pQueuePriorities = &queue_priority;
		queueCIs.push_back(queueCI);
	}

	//�f�o�C�X�g���̑I��
	VkPhysicalDeviceFeatures deviceFeatures{};

	VkDeviceCreateInfo deviceCI{};
	deviceCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCI.pQueueCreateInfos = queueCIs.data();
	deviceCI.queueCreateInfoCount = static_cast<uint32_t>( queueCIs.size() );
	deviceCI.pEnabledFeatures = &deviceFeatures;
	deviceCI.enabledExtensionCount = static_cast<uint32_t>( deviceExtensions.size() );
	deviceCI.ppEnabledExtensionNames = deviceExtensions.data();

	if (enableValidationLayers)
	{
		deviceCI.ppEnabledLayerNames = validationLayers.data();
		deviceCI.enabledLayerCount = uint32_t(validationLayers.size());
	}
	else
	{
		deviceCI.enabledLayerCount = 0;
	}

	// �_���f�o�C�X�쐬
	if (vkCreateDevice(physicalDevice, &deviceCI, nullptr, &device) != VK_SUCCESS)
	{
		throw runtime_error("failed to create logical device!");
	}

	// �O���t�B�b�N�L���[�̃n���h�����擾
	vkGetDeviceQueue(device, qIndices.graphicsFamily.value(), 0, &graphicsQueue);

	// �v���[���g�L���[�̃n���h�����擾
	vkGetDeviceQueue(device, qIndices.presentFamily.value(), 0, &presentQueue);
}

void HelloTriangleApplication::createSurface()
{
	// ��Window surface = �v���b�g�t�@�[���̒��ۉ�
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
	{
		throw runtime_error("failed to create window surface!");
	}
}

bool HelloTriangleApplication::checkDeviceExtensionSupport(VkPhysicalDevice pDevice)
{
	// ����GPU�Ŏg�p�\�Ȋg���𒲂ׂ�
	vector<VkExtensionProperties> availableExtensions;
	{
		uint32_t count = 0;
		vkEnumerateDeviceExtensionProperties(pDevice, nullptr, &count, nullptr);
		availableExtensions.resize(count);
		vkEnumerateDeviceExtensionProperties(pDevice, nullptr, &count, availableExtensions.data());
	}

	// device_extension�Œ�`�����f�o�C�X�g���̃Z�b�g
	set<string>require_extensions(deviceExtensions.begin(), deviceExtensions.end());

	// ��v������̂��폜
	for (const auto& extension : availableExtensions)
	{
		require_extensions.erase(extension.extensionName);
	}

	// �S�ċ�Ȃ�True
	return require_extensions.empty();
}

SwapChainSupportDetails HelloTriangleApplication::querySwapChainSupport(VkPhysicalDevice pDevice)
{
	// ����1�`3���i�[����\����
	SwapChainSupportDetails details;

	// 1, �T�[�t�F�[�X�\�͂̎擾
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pDevice, surface, &details.capabilities);

	// 2, �T�[�t�F�[�X�t�H�[�}�b�g�̎擾
	{
		uint32_t count;
		vkGetPhysicalDeviceSurfaceFormatsKHR(pDevice, surface, &count, nullptr);
		if (1 <= count) { 
			details.formats.resize(count); 
			vkGetPhysicalDeviceSurfaceFormatsKHR(pDevice, surface, &count, details.formats.data());
		}
	}

	// 3, ���p�\�ȃv���[���g���[�h�̎擾
	{
		uint32_t count;
		vkGetPhysicalDeviceSurfacePresentModesKHR(pDevice, surface, &count, nullptr);
		if (1 <= count) {
			details.presentModes.resize(count);
			vkGetPhysicalDeviceSurfacePresentModesKHR(pDevice, surface, &count, details.presentModes.data() );
		}
	}

	return details;
}

VkSurfaceFormatKHR HelloTriangleApplication::chooseSwapSurfaceFormat(const vector<VkSurfaceFormatKHR>& availableFormats)
{
	for (const auto& a : availableFormats)
	{
		if (a.format == VK_FORMAT_B8G8R8A8_SRGB && a.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
		{
			return a;
		}
	}

	return availableFormats[0];
}

VkPresentModeKHR HelloTriangleApplication::chooseSwapPresentMode(const vector<VkPresentModeKHR>& availablePresentModes)
{
	for (const auto& a : availablePresentModes)
	{
		if (a == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return a;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D HelloTriangleApplication::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t> (width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

void HelloTriangleApplication::createSwapChain()
{
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

	if (0 < swapChainSupport.capabilities.maxImageCount && swapChainSupport.capabilities.maxImageCount < imageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapchainCI{};
	swapchainCI.flags = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCI.surface = surface;
	swapchainCI.minImageCount = imageCount;
	swapchainCI.imageFormat = surfaceFormat.format;
	swapchainCI.imageColorSpace = surfaceFormat.colorSpace;
	swapchainCI.imageExtent = extent;
	swapchainCI.imageArrayLayers = 1;
	swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = findQueueFamiles(physicalDevice);
	uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	if (indices.graphicsFamily != indices.presentFamily)
	{
		swapchainCI.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchainCI.queueFamilyIndexCount = 2;
		swapchainCI.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCI.queueFamilyIndexCount = 0;
		swapchainCI.pQueueFamilyIndices = nullptr;
	}
	swapchainCI.preTransform = swapChainSupport.capabilities.currentTransform;
	swapchainCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCI.presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	swapchainCI.clipped = VK_TRUE;
	swapchainCI.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapChain) != VK_SUCCESS)
	{
		throw runtime_error("failed to create swap chain!");
	}

	// SwapChainImage��ۑ����Ă���
	{
		uint32_t count;
		vkGetSwapchainImagesKHR(device, swapChain, &count, nullptr);
		swapChainImages.resize(count);
		vkGetSwapchainImagesKHR(device, swapChain, &count, swapChainImages.data());
	}

	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;
}

void HelloTriangleApplication::createImageViews()
{
	swapChainImageViews.resize(swapChainImages.size());
	for (uint32_t i = 0; i < swapChainImages.size(); i++)
	{
		VkImageViewCreateInfo imageViewCI{};
		imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCI.image = swapChainImages[i];
		imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCI.format = swapChainImageFormat;
		imageViewCI.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCI.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCI.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCI.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCI.subresourceRange.baseMipLevel = 0;
		imageViewCI.subresourceRange.levelCount = 1;
		imageViewCI.subresourceRange.baseArrayLayer = 0;
		imageViewCI.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device, &imageViewCI, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create image views!");
		}
	}
}

void HelloTriangleApplication::createGraphicsPipeline()
{
	auto vertShaderCode = readFile("shaders/vert.spv");
	auto fragShaderCode = readFile("shaders/frag.spv");
}

VkShaderModule HelloTriangleApplication::createShaderModule(vector<char>& code)
{
	VkShaderModuleCreateInfo shaderModuleCI{};
	shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCI.codeSize = code.size();
	shaderModuleCI.pCode = reinterpret_cast<uint32_t*>(code.data());

	VkShaderModule shaderModule;

	if (vkCreateShaderModule(device, &shaderModuleCI, nullptr, &shaderModule) != VK_SUCCESS)
	{
		throw runtime_error("failed to create shader module!");
	}

	return shaderModule;
}