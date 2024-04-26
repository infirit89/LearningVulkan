#include "Application.h"

#include <cassert>

#include <vector>
#include <iostream>

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
	}

	Application::~Application()
	{
		DestroyDebugUtilsMessanger(m_Instance, m_DebugMessanger, nullptr);
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
}
