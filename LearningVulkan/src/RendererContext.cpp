#include "RendererContext.h"

#include "VulkanUtils.h"

#include <GLFW/glfw3.h>

#include <array>
#include <set>
#include <algorithm>
#include <cassert>
#include <iostream>

namespace LearningVulkan
{
	static VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData)
	{
		std::cerr << "validation error: " << callbackData->pMessage << '\n';
		return VK_FALSE;
	}

	static void SetupDebugUtilsMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& messengerCreateInfo)
	{
		messengerCreateInfo.pfnUserCallback = DebugCallback;
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

	RendererContext::RendererContext(std::string_view applicationName)
	{
		CreateVulkanInstance(applicationName);
		SetupDebugMessenger();
	}

	RendererContext::~RendererContext()
	{
		DestroyDebugUtilsMessanger(m_Instance, m_DebugMessenger, nullptr);
		vkDestroyInstance(m_Instance, nullptr);
	}

	void RendererContext::CreateVulkanInstance(std::string_view applicationName)
	{
		VkApplicationInfo applicationInfo{};
		applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		applicationInfo.pApplicationName = applicationName.data();
		applicationInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo instanceCreateInfo{};
		instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceCreateInfo.pApplicationInfo = &applicationInfo;

		// get the required instance extensions
		uint32_t extensionCount = 0;
		const char** requiredExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);
		std::vector<const char*> extensions(requiredExtensions, requiredExtensions + extensionCount);

		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		instanceCreateInfo.enabledExtensionCount = extensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

		// there's an unavailable layer
		assert(CheckLayersAvailability() == true);

		// TODO: enable if we want to use the validation layer
		instanceCreateInfo.enabledLayerCount = VulkanUtils::Layers.size();
		instanceCreateInfo.ppEnabledLayerNames = VulkanUtils::Layers.data();

		VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};
		SetupDebugUtilsMessengerCreateInfo(debugMessengerCreateInfo);
		instanceCreateInfo.pNext = &debugMessengerCreateInfo;

		vkCreateInstance(&instanceCreateInfo, nullptr, &m_Instance);
	}

	bool RendererContext::CheckLayersAvailability()
	{
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> supportedLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, supportedLayers.data());

		std::set<std::string> requiredLayers(VulkanUtils::Layers.begin(), VulkanUtils::Layers.end());

		for (const auto& layer : supportedLayers)
			requiredLayers.erase(layer.layerName);

		return requiredLayers.empty();
	}

	void RendererContext::SetupDebugMessenger()
	{
		VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};
		SetupDebugUtilsMessengerCreateInfo(debugMessengerCreateInfo);
		assert(CreateDebugUtilsMessanger(m_Instance, &debugMessengerCreateInfo, nullptr, &m_DebugMessenger) == VK_SUCCESS);
	}
}
