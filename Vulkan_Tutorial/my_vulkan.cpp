#include "my_vulkan.h"

void Vulkan::run()
{
	initWindow("Ushinokoku");
	initVulkan();
	mainLoop();
	cleanup();
}

void Vulkan::initWindow(const char* title)
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(WIDTH, HEIGHT, title, nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void Vulkan::framebufferResizeCallback(GLFWwindow *window, int width, int height)
{
	auto app = reinterpret_cast<Vulkan*>(glfwGetWindowUserPointer(window));
	app->framebufferResized = true;
}

void Vulkan::initVulkan()
{
	createInstance();
	createSurface();
	selectPhysicalDevice();
	createDevice();
	createSwapChain();
	createImageViews();
	createRenderPass();
	createGraphicsPipeline();
	createFrameBuffers();
	createCommandPools();
	createVertexBuffer(vertices.data(), sizeof(vertices[0]) * vertices.size());
	createIndexBuffer(indices.data(), sizeof(indices[0]) * indices.size());
	createCommandBuffer();
	createSyncObjects();
}

void Vulkan::mainLoop()
{
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		drawFrame();
	}

	vkDeviceWaitIdle(device); // この後cleanup()
}

void Vulkan::cleanup()
{
	for (uint32_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++)
	{
		for (auto semaphore : { imageAvailableSemaphores[i], renderFinishedSemaphores[i] })
		{
			vkDestroySemaphore(device, semaphore, nullptr);
		}
		vkDestroyFence(device, inFlightFences[i], nullptr);
	}
	for (auto pool : commandPools)
	{
		vkDestroyCommandPool(device, pool, nullptr);
	}
	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyRenderPass(device, renderPass, nullptr);
	cleanupSwapChain();
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkDestroyBuffer(device, indexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
	vkFreeMemory(device, indexBufferMemory, nullptr);
	vkDeviceWaitIdle(device); // 作業が完了してから
	vkDestroyDevice(device, nullptr); // 破棄する
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr); // 一番最後に破棄するVulkanオブジェクト
	glfwDestroyWindow(window);
	glfwTerminate();
}

void Vulkan::cleanupSwapChain()
{
	for (auto framebuffer : swapChainFramebuffers)
	{
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}
	for (auto imageView : swapChainImageViews)
	{
		vkDestroyImageView(device, imageView, nullptr);
	}
	vkDestroySwapchainKHR(device, swapChain, nullptr);
}

void Vulkan::createInstance()
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

	VkInstanceCreateInfo instanceInfo{};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &appInfo;
	instanceInfo.enabledExtensionCount = glfwExtensionCount;
	instanceInfo.ppEnabledExtensionNames = glfwExtensions;
	instanceInfo.enabledLayerCount = 0;

	if (enableValidationLayers)
	{
		instanceInfo.enabledLayerCount = uint32_t( validationLayers.size() );
		instanceInfo.ppEnabledLayerNames = validationLayers.data();
	}

	if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS)
	{
		throw runtime_error("failed to createinstance");
	}

	// Vulkan拡張サポートチェック
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

	// バリデーションが利用可能かチェック
	if (enableValidationLayers && !checkValidationLayerSupport())
	{
		throw runtime_error("validation layers requested, but not available!");
	}
}


bool Vulkan::checkValidationLayerSupport()
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

void Vulkan::selectPhysicalDevice()
{
	// 全てのGPUを取得
	vector<VkPhysicalDevice> physDevs;
	{
		uint32_t count;
		vkEnumeratePhysicalDevices(instance, &count, nullptr);
		if (count == 0) throw runtime_error("failed to find GPUs with Vulkan Support!");
		physDevs.resize(count);
		vkEnumeratePhysicalDevices(instance, &count, physDevs.data());
	}

	// 適したGPUを選択
	for (const auto& p : physDevs)
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

bool Vulkan::isDeviceSuitable(VkPhysicalDevice physDev)
{
	// GPUの適合性をチェック
	VkPhysicalDeviceProperties properties{};
	VkPhysicalDeviceFeatures features{};
	vkGetPhysicalDeviceProperties(physDev, &properties);
	vkGetPhysicalDeviceFeatures(physDev, &features);
	
	// GPUに備わっているキューファミリを調べ、インデックスを作る
	QueueFamilyIndices indices = findQueueFamiles(physDev);

	// 必要なデバイス拡張が使えるか
	bool extensionSupported = checkDeviceExtensionSupport(physDev);

	// スワップチェインのサポートが十分か
	bool swapChainAdequate = false;
	if (extensionSupported) // ※スワップチェインはKHRであり拡張であるから
	{
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physDev);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	// 各適合性の論理積を返す
	return indices.isComplete() && extensionSupported && swapChainAdequate;
}

// 自作のキューファミリインデックスの構造体を返す
QueueFamilyIndices Vulkan::findQueueFamiles(VkPhysicalDevice physDev)
{
	QueueFamilyIndices indices{};

	// GPUのキューファミリ一覧を取得
	vector<VkQueueFamilyProperties> queueFamilyProps;
	{
		uint32_t count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physDev, &count, nullptr);
		queueFamilyProps.resize(count);
		vkGetPhysicalDeviceQueueFamilyProperties(physDev, &count, queueFamilyProps.data());
	}

	for (uint32_t i = 0; i < uint32_t( queueFamilyProps.size() ); i++)
	{
		// i番目のキューファミリインデックスがグラフィックキューか
		if (queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphicsFamily = i;

		// i番目のキューファミリインデックスがトランスファーキューか
		if (queueFamilyProps[i].queueFlags & VK_QUEUE_TRANSFER_BIT) indices.transferFamily = i;

		// i番目のキューファミリインデックスがイメージを扱えるか
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(physDev, i, surface, &presentSupport);
		if (presentSupport) indices.presentFamily = i;

		if (indices.isComplete()) break;
	}

	return indices;
}

void Vulkan::createDevice()
{
	// キューファミリ構造体取得
	QueueFamilyIndices queueIndices = findQueueFamiles(physicalDevice);

	vector<VkDeviceQueueCreateInfo> devQueueInfo; // キューファミリのCreateInfoの配列
	set<uint32_t> uniqueQueueFamilies = {    // キューファミリの配列
		queueIndices.graphicsFamily.value(),
		queueIndices.presentFamily.value(),
		queueIndices.transferFamily.value()
	};

	float queue_priority = 1.0f;
	for (uint32_t q : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCI{};
		queueCI.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCI.queueFamilyIndex = q;
		queueCI.queueCount = 1;
		queueCI.pQueuePriorities = &queue_priority;
		devQueueInfo.push_back(queueCI);
	}

	//サポートの選択
	VkPhysicalDeviceFeatures supportedFeatures;
	VkPhysicalDeviceFeatures requiredFeatures{};

	vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

	// サポートの有効化
	requiredFeatures.multiDrawIndirect = supportedFeatures.multiDrawIndirect;
	requiredFeatures.tessellationShader = VK_TRUE;
	requiredFeatures.geometryShader = VK_TRUE;

	VkDeviceCreateInfo deviceInfo{};
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pQueueCreateInfos = devQueueInfo.data();
	deviceInfo.queueCreateInfoCount = static_cast<uint32_t>( devQueueInfo.size() );
	deviceInfo.pEnabledFeatures = &requiredFeatures;
	deviceInfo.enabledExtensionCount = static_cast<uint32_t>( deviceExtensions.size() );
	deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if (enableValidationLayers)
	{
		deviceInfo.ppEnabledLayerNames = validationLayers.data();
		deviceInfo.enabledLayerCount = uint32_t(validationLayers.size());
	}
	else
	{
		deviceInfo.enabledLayerCount = 0;
	}

	// 論理デバイス作成
	if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS)
	{
		throw runtime_error("failed to create logical device!");
	}

	// グラフィックキューのハンドルを取得
	vkGetDeviceQueue(device, queueIndices.graphicsFamily.value(), 0, &graphicsQueue);

	// プレゼントキューのハンドルを取得
	vkGetDeviceQueue(device, queueIndices.presentFamily.value(), 0, &presentQueue);

	// データ転送キューのハンドルを取得
	vkGetDeviceQueue(device, queueIndices.transferFamily.value(), 0, &transferQueue);
}

void Vulkan::createSurface()
{
	// ※Window surface = プラットファームの抽象化
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
	{
		throw runtime_error("failed to create window surface!");
	}
}

bool Vulkan::checkDeviceExtensionSupport(VkPhysicalDevice pDevice)
{
	// このGPUで使用可能な拡張を調べる
	vector<VkExtensionProperties> availableExtensions;
	{
		uint32_t count = 0;
		vkEnumerateDeviceExtensionProperties(pDevice, nullptr, &count, nullptr);
		availableExtensions.resize(count);
		vkEnumerateDeviceExtensionProperties(pDevice, nullptr, &count, availableExtensions.data());
	}

	// device_extensionで定義したデバイス拡張のセット
	set<string>require_extensions(deviceExtensions.begin(), deviceExtensions.end());

	// 一致するものを削除
	for (const auto& extension : availableExtensions)
	{
		require_extensions.erase(extension.extensionName);
	}

	// 全て空ならTrue
	return require_extensions.empty();
}

SwapChainSupportDetails Vulkan::querySwapChainSupport(VkPhysicalDevice pDevice)
{
	// 次の1～3を格納する構造体
	SwapChainSupportDetails details;

	// 1, サーフェース能力の取得
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pDevice, surface, &details.capabilities);

	// 2, サーフェースフォーマットの取得
	{
		uint32_t count;
		vkGetPhysicalDeviceSurfaceFormatsKHR(pDevice, surface, &count, nullptr);
		if (1 <= count) { 
			details.formats.resize(count); 
			vkGetPhysicalDeviceSurfaceFormatsKHR(pDevice, surface, &count, details.formats.data());
		}
	}

	// 3, 利用可能なプレゼントモードの取得
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

VkSurfaceFormatKHR Vulkan::chooseSwapSurfaceFormat(const vector<VkSurfaceFormatKHR>& availableFormats)
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

VkPresentModeKHR Vulkan::chooseSwapPresentMode(const vector<VkPresentModeKHR>& availablePresentModes)
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

VkExtent2D Vulkan::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
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

void Vulkan::createSwapChain()
{
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

	if (0 < swapChainSupport.capabilities.maxImageCount && swapChainSupport.capabilities.maxImageCount < imageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapchainInfo{};
	swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainInfo.surface = surface;
	swapchainInfo.minImageCount = imageCount;
	swapchainInfo.imageFormat = surfaceFormat.format;
	swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapchainInfo.imageExtent = extent;
	swapchainInfo.imageArrayLayers = 1;
	swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = findQueueFamiles(physicalDevice);
	uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	if (indices.graphicsFamily != indices.presentFamily)
	{
		swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchainInfo.queueFamilyIndexCount = 2;
		swapchainInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainInfo.queueFamilyIndexCount = 0;
		swapchainInfo.pQueueFamilyIndices = nullptr;
	}
	swapchainInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainInfo.presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	swapchainInfo.clipped = VK_TRUE;
	swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapChain) != VK_SUCCESS)
	{
		throw runtime_error("failed to create swap chain!");
	}

	// SwapChainImageを保存しておく
	{
		uint32_t count;
		vkGetSwapchainImagesKHR(device, swapChain, &count, nullptr);
		swapChainImages.resize(count);
		vkGetSwapchainImagesKHR(device, swapChain, &count, swapChainImages.data());
	}

	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;
}

void Vulkan::createImageViews()
{
	swapChainImageViews.resize(swapChainImages.size());
	for (uint32_t i = 0; i < swapChainImages.size(); i++)
	{
		VkImageViewCreateInfo imageViewInfo{};
		imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewInfo.image = swapChainImages[i];
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = swapChainImageFormat;
		imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device, &imageViewInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create image views!");
		}
	}
}

void Vulkan::createRenderPass()
{
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0; //index of colorAttachment
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
	{
		throw runtime_error("failed to create render pass!");
	}
}

void Vulkan::createGraphicsPipeline()
{
	auto vertShaderCode = readFile("shaders/vert.spv");
	auto fragShaderCode = readFile("shaders/frag.spv");

	VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
	VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[]	= {vertShaderStageInfo, fragShaderStageInfo};

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInput{};
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount = 1;
	vertexInput.pVertexBindingDescriptions = &bindingDescription;
	vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapChainExtent.width);
	viewport.height = static_cast<float>(swapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;

	vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE; // VK_FALSEの時，以下は省略可
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0;
	pipelineLayoutInfo.pSetLayouts = nullptr;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		throw runtime_error("failed to create pipeline layout!");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInput;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0; //index
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
	{
		throw runtime_error("failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(device, vertShaderModule, nullptr);
	vkDestroyShaderModule(device, fragShaderModule, nullptr);
}

VkShaderModule Vulkan::createShaderModule(vector<char>& code)
{
	VkShaderModuleCreateInfo shaderModuleInfo{};
	shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleInfo.codeSize = code.size();
	shaderModuleInfo.pCode = reinterpret_cast<uint32_t*>(code.data());

	VkShaderModule shaderModule;

	if (vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		throw runtime_error("failed to create shader module!");
	}

	return shaderModule;
}

void Vulkan::createFrameBuffers()
{
	swapChainFramebuffers.resize(swapChainImageViews.size());

	for (size_t i = 0; i < swapChainImageViews.size(); i++)
	{
		vector<VkImageView> attachments = {
			swapChainImageViews[i]
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = attachments.size();
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = swapChainExtent.width;
		framebufferInfo.height = swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
		{
			throw runtime_error("failed to create swapchain framebuffer!");
		}
	}
}

void Vulkan::createCommandPool(VkCommandPool *commandPool, uint32_t queueIndex)
{
	VkCommandPoolCreateInfo commandPoolInfo{};
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolInfo.queueFamilyIndex = queueIndex;

	if (vkCreateCommandPool(device, &commandPoolInfo, nullptr, commandPool) != VK_SUCCESS)
	{
		throw runtime_error("failed to create commandPool !");
	}
	commandPools.push_back(*commandPool);
}

void Vulkan::createCommandPools()
{
	QueueFamilyIndices indices = findQueueFamiles(physicalDevice);
	createCommandPool(&commandPool, indices.graphicsFamily.value());
	createCommandPool(&transferPool, indices.transferFamily.value());
}

void Vulkan::createCommandBuffer()
{
	commandBuffers.resize(MAX_FRAME_IN_FLIGHT);
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = commandBuffers.size();

	if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
	{
		throw runtime_error("failed to create commanBuffers!");
	}
}

void Vulkan::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
	{
		throw runtime_error("failed to begin commandBuffer!");
	}

	VkClearValue clearValue{};
	clearValue.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = swapChainExtent;
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearValue;

	// ダイナミック
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapChainExtent.width);
	viewport.height = static_cast<float>(swapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;

	VkBuffer vertexBuffers[] = { vertexBuffer };
	size_t offsets[] = {0};

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(commandBuffer, indices.size(), 1, 0, 0, 0);
	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
	{
		throw runtime_error("failed to record command!");
	}
}

void Vulkan::drawFrame()
{
	vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
	uint32_t imageIndex = 0;
	VkResult imgResult = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
	if (imgResult == VK_ERROR_OUT_OF_DATE_KHR)
	{
		recreateSwapChain();
		return;
	}
	else if (imgResult != VK_SUCCESS && imgResult != VK_SUBOPTIMAL_KHR)
	{
		throw runtime_error("failed to aquire swap chain image!");
	}
	vkResetFences(device, 1, &inFlightFences[currentFrame]); // 非シグナル化
	vkResetCommandBuffer(commandBuffers[currentFrame], 0);
	recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

	vector<VkSemaphore> waitSemaphores = { imageAvailableSemaphores[currentFrame]};
	vector<VkSemaphore> signalSemaphores = {renderFinishedSemaphores[currentFrame]};
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = waitSemaphores.size();
	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
	submitInfo.signalSemaphoreCount = signalSemaphores.size();
	submitInfo.pSignalSemaphores = signalSemaphores.data();

	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
	{
		throw runtime_error("failed to submit draw command queue!");
	}

	vector<VkSwapchainKHR>swapChains = { swapChain };

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = signalSemaphores.size();
	presentInfo.pWaitSemaphores = signalSemaphores.data();
	presentInfo.swapchainCount = swapChains.size();
	presentInfo.pSwapchains = swapChains.data();
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);

	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || framebufferResized)
	{
		recreateSwapChain();
		framebufferResized = false;
	}
	else if (presentResult != VK_SUCCESS)
	{
		throw runtime_error("failed to present!");
	}

	currentFrame = (currentFrame + 1) % MAX_FRAME_IN_FLIGHT;
}

void Vulkan::createSyncObjects()
{
	imageAvailableSemaphores.resize(MAX_FRAME_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAME_IN_FLIGHT);
	inFlightFences.resize(MAX_FRAME_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (uint32_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++)
	{
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
		{
			throw runtime_error("failed to create synchronization Objects");
		}
	}
}

void Vulkan::recreateSwapChain()
{
	// ウィンドウサイズが０の時
	int width = 0, height = 0;
	glfwGetFramebufferSize(window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(device);
	cleanupSwapChain();
	createSwapChain();
	createImageViews();
	createFrameBuffers();
}

void Vulkan::createBuffer(size_t size, VkBuffer *buffer, VkDeviceMemory *deviceMemory, VkBufferUsageFlags usage, VkMemoryPropertyFlags props)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device, &bufferInfo, nullptr, buffer) != VK_SUCCESS)
	{
		throw runtime_error("failed to create vertex buffer!");
	}

	VkMemoryRequirements memReq;
	vkGetBufferMemoryRequirements(device, *buffer, &memReq);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, props);
	if (vkAllocateMemory(device, &allocInfo, nullptr, deviceMemory) != VK_SUCCESS)
	{
		throw runtime_error("failed to allocate vertex buffer memory!");
	}

	vkBindBufferMemory(device, *buffer, *deviceMemory, 0);
}

void Vulkan::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, size_t size)
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = transferPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS)
	{
		throw runtime_error("failed to allocate command buffer in copyBuffer()!");
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(transferQueue, 1, &submitInfo, nullptr);
	vkQueueWaitIdle(transferQueue);

	vkFreeCommandBuffers(device, transferPool, 1, &commandBuffer);
}

void Vulkan::createVertexBuffer(void *data, size_t size)
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(size, &stagingBuffer, &stagingBufferMemory, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	void* pointer;
	vkMapMemory(device, stagingBufferMemory, 0, size, 0, &pointer);
	memcpy(pointer, data, size);
	vkUnmapMemory(device, stagingBufferMemory);

	createBuffer(size, &vertexBuffer, &vertexBufferMemory, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	copyBuffer(stagingBuffer, vertexBuffer, size);

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Vulkan::createIndexBuffer(void* data, size_t size)
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(size, &stagingBuffer, &stagingBufferMemory, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	void* pointer;
	vkMapMemory(device, stagingBufferMemory, 0, size, 0, &pointer);
	memcpy(pointer, data, size);
	vkUnmapMemory(device, stagingBufferMemory);

	createBuffer(size, &indexBuffer, &indexBufferMemory, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	copyBuffer(stagingBuffer, indexBuffer, size);

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}

uint32_t Vulkan::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties physMemProps;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physMemProps);
	for (uint32_t i = 0; i < physMemProps.memoryTypeCount; i++)
	{
		if (typeFilter & (1 << i) && (physMemProps.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}
	throw runtime_error("failed to find suitable memory type!");
}