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
	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr); // 一番最後に破棄するVulkanオブジェクト
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
	// 全てのGPUを取得
	vector<VkPhysicalDevice> pDevices;
	{
		uint32_t count;
		vkEnumeratePhysicalDevices(instance, &count, nullptr);
		if (count == 0) throw runtime_error("failed to find GPUs with Vulkan Support!");
		pDevices.resize(count);
		vkEnumeratePhysicalDevices(instance, &count, pDevices.data());
	}

	// 適したGPUを選択
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
	// GPUの適合性をチェック
	VkPhysicalDeviceProperties properties{};
	VkPhysicalDeviceFeatures features{};
	vkGetPhysicalDeviceProperties(pDevice, &properties);
	vkGetPhysicalDeviceFeatures(pDevice, &features);
	
	// GPUに備わっているキューファミリを調べ、インデックスを作る
	QueueFamilyIndices indices = findQueueFamiles(pDevice);

	// 必要なデバイス拡張が使えるか
	bool extensionSupported = checkDeviceExtensionSupport(pDevice);

	// スワップチェインのサポートが十分か
	bool swapChainAdequate = false;
	if (extensionSupported) // ※スワップチェインはKHRであり拡張であるから
	{
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(pDevice);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	// 各適合性の論理積を返す
	return indices.isComplete() && extensionSupported && swapChainAdequate;
}

// 自作のキューファミリインデックスの構造体を返す
QueueFamilyIndices HelloTriangleApplication::findQueueFamiles(VkPhysicalDevice pDevice)
{
	QueueFamilyIndices indices{};

	// GPUのキューファミリ一覧を取得
	vector<VkQueueFamilyProperties> queueFamilyProps;
	{
		uint32_t count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(pDevice, &count, nullptr);
		queueFamilyProps.resize(count);
		vkGetPhysicalDeviceQueueFamilyProperties(pDevice, &count, queueFamilyProps.data());
	}

	for (uint32_t i = 0; i < uint32_t( queueFamilyProps.size() ); i++)
	{
		// i番目のキューファミリインデックスがグラフィックキューか
		if (queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = i;
		}

		// i番目のキューファミリインデックスがイメージを扱えるか
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(pDevice, i, surface, &presentSupport);
		if (presentSupport) indices.presentFamily = i;

		if (indices.isComplete()) break;
	}

	return indices;
}

void HelloTriangleApplication::createDevice()
{
	// キューファミリ構造体取得
	QueueFamilyIndices qIndices = findQueueFamiles(physicalDevice);

	vector<VkDeviceQueueCreateInfo> queueCIs; // キューファミリのCreateInfoの配列
	set<uint32_t> uniqueQueueFamilies = {    // キューファミリの配列
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

	//デバイス拡張の選択
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

	// 論理デバイス作成
	if (vkCreateDevice(physicalDevice, &deviceCI, nullptr, &device) != VK_SUCCESS)
	{
		throw runtime_error("failed to create logical device!");
	}

	// グラフィックキューのハンドルを取得
	vkGetDeviceQueue(device, qIndices.graphicsFamily.value(), 0, &graphicsQueue);

	// プレゼントキューのハンドルを取得
	vkGetDeviceQueue(device, qIndices.presentFamily.value(), 0, &presentQueue);
}

void HelloTriangleApplication::createSurface()
{
	// ※Window surface = プラットファームの抽象化
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
	{
		throw runtime_error("failed to create window surface!");
	}
}

bool HelloTriangleApplication::checkDeviceExtensionSupport(VkPhysicalDevice pDevice)
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

SwapChainSupportDetails HelloTriangleApplication::querySwapChainSupport(VkPhysicalDevice pDevice)
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
