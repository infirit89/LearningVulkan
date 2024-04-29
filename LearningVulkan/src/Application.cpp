#include "Application.h"

#include <cassert>

#include <vector>
#include <iostream>
#include <map>
#include <set>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace LearningVulkan 
{
	static constexpr size_t s_ValidationLayersSize = 1;
	static const char* s_ValidationLayers[s_ValidationLayersSize] =
	{
		"VK_LAYER_KHRONOS_validation"
	};

	static VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
											const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData) 
	{
		std::cerr << "validation error: " << callbackData->pMessage << '\n';
		return VK_FALSE;
	}

	static VkResult CreateDebugUtilsMessanger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* messangerCreateInfo, 
											const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* debugMessanger) 
	{
		PFN_vkCreateDebugUtilsMessengerEXT func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
		assert(func);

		return func(instance, messangerCreateInfo, allocator, debugMessanger);
	}

	static void DestroyDebugUtilsMessanger(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* allocator)
	{
		PFN_vkDestroyDebugUtilsMessengerEXT func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
		assert(func);

		func(instance, debugMessenger, allocator);
	}

	static void SetupDebugUtilsMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& messengerCreateInfo) 
	{
		messengerCreateInfo.pfnUserCallback = debugCallback;
		messengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		messengerCreateInfo.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
			| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
			| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

		messengerCreateInfo.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
			| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
			| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	}

	Application::Application()
	{
		m_Window = new Window(640, 480, "i cum hard uwu");
		SetupRenderer();
	}

	Application::~Application()
	{
		vkDestroyDevice(m_Device, nullptr);
		DestroyDebugUtilsMessanger(m_Instance, m_DebugMessanger, nullptr);
		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
		vkDestroyInstance(m_Instance, nullptr);

		delete m_Window;
	}

	void Application::Run()
	{
		while (m_Window->IsOpen())
		{
			m_Window->PollEvents();
		}
	}

	std::vector<const char*> Application::GetRequiredExtensions()
	{
		const char** requiredExtensions;
		uint32_t requiredExtensionCount;
		requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionCount);

		std::vector<const char*> extensions(requiredExtensions, requiredExtensions + requiredExtensionCount);

		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		return extensions;
	}

	bool Application::CheckValidationLayerSupport()
	{
		uint32_t supportedLayerCount;
		vkEnumerateInstanceLayerProperties(&supportedLayerCount, nullptr);
		std::vector<VkLayerProperties> supportedLayers(supportedLayerCount);
		vkEnumerateInstanceLayerProperties(&supportedLayerCount, supportedLayers.data());

		std::cout << "supported layers:\n";
		for (const VkLayerProperties& layer : supportedLayers)
			std::cout << '\t' << layer.layerName << '\n';
		
		for (const char* layerName : s_ValidationLayers) 
		{
			bool layerFound = false;

			for (const VkLayerProperties& layer : supportedLayers)
			{
				if (strcmp(layerName, layer.layerName) == 0) 
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound) 
			{
				std::cerr << layerName << " not found in the supported layers";
				return false;
			}
		}

		return true;
	}

	void Application::SetupDebugMessanger()
	{
		VkDebugUtilsMessengerCreateInfoEXT debugMessangerCreateInfo{};
		SetupDebugUtilsMessengerCreateInfo(debugMessangerCreateInfo);
		
		assert(CreateDebugUtilsMessanger(m_Instance, &debugMessangerCreateInfo, nullptr, &m_DebugMessanger) == VK_SUCCESS);
	}

	void Application::SetupRenderer()
	{
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pNext = nullptr;
		appInfo.pApplicationName = "Learning Vulkan";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo instanceInfo{};
		instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceInfo.pNext = nullptr;
		instanceInfo.pApplicationInfo = &appInfo;

		std::vector<const char*> extensionsNames = GetRequiredExtensions();

		uint32_t supportedExtensionCount;
		vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, nullptr);

		std::vector<VkExtensionProperties> supportedExtensions(supportedExtensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, supportedExtensions.data());
		instanceInfo.enabledExtensionCount = extensionsNames.size();
		instanceInfo.ppEnabledExtensionNames = extensionsNames.data();

		if (!CheckValidationLayerSupport())
			return;

		instanceInfo.enabledLayerCount = s_ValidationLayersSize;
		instanceInfo.ppEnabledLayerNames = s_ValidationLayers;

		VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo{};
		SetupDebugUtilsMessengerCreateInfo(messengerCreateInfo);
		instanceInfo.pNext = &messengerCreateInfo;

		std::cout << "supported extensions:\n";
		for (const VkExtensionProperties& extension : supportedExtensions)
			std::cout << '\t' << extension.extensionName << '\n';


		assert(vkCreateInstance(&instanceInfo, nullptr, &m_Instance) == VK_SUCCESS);
		SetupDebugMessanger();

		SetupSurface();
		m_PhysicalDevice = PickPhysicalDevice();
		SetupLogicalDevice();
	}

	VkPhysicalDevice Application::PickPhysicalDevice()
	{
		uint32_t physicalDeviceCount = 0;
		vkEnumeratePhysicalDevices(m_Instance, &physicalDeviceCount, nullptr);

		assert(physicalDeviceCount > 0);

		std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
		vkEnumeratePhysicalDevices(m_Instance, &physicalDeviceCount, physicalDevices.data());

		std::multimap<uint32_t, VkPhysicalDevice> deviceCandidates;

		for (const auto& physicalDevice : physicalDevices) 
		{
			uint32_t score = RateDeviceSuitability(physicalDevice);
			deviceCandidates.insert(std::make_pair(score, physicalDevice));
		}

		if (deviceCandidates.rbegin()->first > 0)
			return deviceCandidates.rbegin()->second;
		else 
		{
			std::cerr << "No suitable device found" << '\n';
			return nullptr;
		}
	}

	uint32_t Application::RateDeviceSuitability(VkPhysicalDevice physicalDevice)
	{
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);


		uint32_t score = 0;

		std::cout << deviceProperties.deviceName << '\n';
		if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			score++;

		QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(physicalDevice);
		if (!queueFamilyIndices.GraphicsFamily.has_value())
			score = 0;

		if (!queueFamilyIndices.PresentationFamily.has_value())
			score = 0;

		return score;
	}
	QueueFamilyIndices Application::FindQueueFamilies(VkPhysicalDevice physicalDevice)
	{
		QueueFamilyIndices queueFamilyIndices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

		uint32_t familyIndex = 0;
		for (const auto& queueFamily : queueFamilies) 
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				queueFamilyIndices.GraphicsFamily = familyIndex;

			VkBool32 presentationSupport = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, familyIndex, m_Surface, &presentationSupport);
			if (presentationSupport)
				queueFamilyIndices.PresentationFamily = familyIndex;

			familyIndex++;
		}

		return queueFamilyIndices;
	}
	void Application::SetupLogicalDevice()
	{
		QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);


		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueIndices = { indices.GraphicsFamily.value(), indices.PresentationFamily.value() };

		for (const uint32_t queueIndex : uniqueQueueIndices)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};

			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.queueFamilyIndex = queueIndex;

			float queuePriority = 1.0f;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.emplace_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};

		VkDeviceCreateInfo deviceCreateInfo{};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.enabledExtensionCount = 0;

		deviceCreateInfo.enabledLayerCount = s_ValidationLayersSize;
		deviceCreateInfo.ppEnabledLayerNames = s_ValidationLayers;

		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.queueCreateInfoCount = queueCreateInfos.size();

		assert(vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device) == VK_SUCCESS);

		// the queue index is zero as we're creating only one queue from this family
		vkGetDeviceQueue(m_Device, indices.GraphicsFamily.value(), 0, &m_GraphicsQueue);

		vkGetDeviceQueue(m_Device, indices.PresentationFamily.value(), 0, &m_PresentQueue);
	}

	void Application::SetupSurface()
	{
		/*VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.hwnd = glfwGetWin32Window(m_Window->GetNativeWindow());
		surfaceCreateInfo.hinstance = GetModuleHandle(nullptr);

		assert(vkCreateWin32SurfaceKHR(m_Instance, &surfaceCreateInfo, nullptr, &m_Surface) == VK_SUCCESS);*/

		assert(glfwCreateWindowSurface(m_Instance, m_Window->GetNativeWindow(), nullptr, &m_Surface) == VK_SUCCESS);
	}
}
