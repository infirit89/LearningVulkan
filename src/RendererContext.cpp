#include "RendererContext.h"
#include "Image.h"
#include "PhysicalDevice.h"
#include "VulkanUtils.h"
#include "Application.h"
#include "vulkan/vulkan_core.h"

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <array>
#include <set>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <fstream>
#include <filesystem>
#include <chrono>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <stb_image.h>

#include "Vertex.h"

namespace LearningVulkan
{
    namespace
    {
        VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData)
        {
            switch (messageSeverity)
            {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                std::cerr << "validation trace: " << callbackData->pMessage << '\n';
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                std::cerr << "validation info: " << callbackData->pMessage << '\n';
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                std::cerr << "validation warning: " << callbackData->pMessage << '\n';
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                std::cerr << "validation error: " << callbackData->pMessage << '\n';
                break;
            }

            return VK_FALSE;
        }

        void SetupDebugUtilsMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& messengerCreateInfo)
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

        VkResult CreateDebugUtilsMessanger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* messangerCreateInfo,
            const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* debugMessanger)
        {
            PFN_vkCreateDebugUtilsMessengerEXT func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
            assert(func);

            return func(instance, messangerCreateInfo, allocator, debugMessanger);
        }

        void DestroyDebugUtilsMessanger(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* allocator)
        {
            PFN_vkDestroyDebugUtilsMessengerEXT func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(
                    instance, "vkDestroyDebugUtilsMessengerEXT"));
            assert(func);

            func(instance, debugMessenger, allocator);
        }

        std::vector<char> ReadShaderFile(const std::filesystem::path& shaderPath)
        {
            std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);

            assert(file.is_open());

            size_t fileSize = file.tellg();
            std::vector<char> fileData(fileSize);
            file.seekg(0);
            file.read(fileData.data(), fileSize);

            return fileData;
        }
    }

    VkInstance RendererContext::m_Instance;
    VkSurfaceKHR RendererContext::m_Surface;
    LogicalDevice* RendererContext::m_LogicalDevice;
    VkCommandPool RendererContext::m_TransientTransferCommandPool;
    VkCommandPool RendererContext::m_TransientGraphicsCommandPool;

    static float fov = 45.0f;

    static void MouseScrollCallback(GLFWwindow* window, double x, double y)
    {
        fov -= y;
    }

    RendererContext::RendererContext(std::string_view applicationName)
    {
        CreateVulkanInstance(applicationName);
        SetupDebugMessenger();
        CreateSurface();

        m_PhysicalDevice = PhysicalDevice::GetSuitablePhysicalDevice();
        assert(m_PhysicalDevice != nullptr);
        m_LogicalDevice = m_PhysicalDevice->CreateLogicalDevice();

        const Window* window = Application::Get()->GetWindow();

        glfwSetScrollCallback(window->GetNativeWindow(), MouseScrollCallback);

        m_Swapchain = new Swapchain(
            m_LogicalDevice,
            window->GetWidth(),
            window->GetHeight(),
            PresentMode::Mailbox);

        CreateDepthResources();

        CreateRenderPass();

        m_Framebuffers.reserve(m_Swapchain->GetImageViews().size());
        for (size_t i = 0; i < m_Swapchain->GetImageViews().size(); i++)
        {
            const VkExtent2D& extent = m_Swapchain->GetExtent();

            std::vector attachments
            {
                m_Swapchain->GetImageViews().at(i),
                m_DepthImage->GetVulkanImageView(),
            };

            Framebuffer* framebuffer = new Framebuffer(
                m_RenderPass, attachments,
                extent.width, extent.height);
            m_Framebuffers.push_back(framebuffer);
        }

        m_PerFrameData.resize(m_Swapchain->GetImageViews().size());
        for (size_t i = 0; i < m_Swapchain->GetImageViews().size(); ++i)
            CreatePerFrameObjects(i);

        const QueueFamilyIndices& queueFamilyIndices =
            m_PhysicalDevice->GetQueueFamilyIndices();
        m_TransientTransferCommandPool = CreateCommandPool(
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            queueFamilyIndices.TransferFamily.value());
        m_TransientGraphicsCommandPool = CreateCommandPool(
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            queueFamilyIndices.GraphicsFamily.value());

        CreateTexture();
        CreateCameraDescriptorSetLayout();
        CreateDescriptorPool();
        CreateDescriptorSets();

        CreateGraphicsPipeline();
        AddCube();
        AddCube(glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f)));

        CreateVertexBuffer();
        CreateIndexBuffer();

    }

    RendererContext::~RendererContext()
    {
        vkDestroyCommandPool(m_LogicalDevice->GetVulkanDevice(),
                             m_TransientTransferCommandPool, nullptr);
        vkDestroyCommandPool(m_LogicalDevice->GetVulkanDevice(),
                             m_TransientGraphicsCommandPool, nullptr);

        for (const PerFrameData& data : m_PerFrameData)
        {
            vkDestroySemaphore(m_LogicalDevice->GetVulkanDevice(),
                               data.SwapchainImageAcquireSemaphore, nullptr);
            vkDestroySemaphore(m_LogicalDevice->GetVulkanDevice(),
                               data.QueueReadySemaphore, nullptr);
            vkDestroyFence(m_LogicalDevice->GetVulkanDevice(),
                            data.PresentFence, nullptr);
            vkFreeCommandBuffers(m_LogicalDevice->GetVulkanDevice(),
                            data.CommandPool, 1, &data.CommandBuffer);
            vkDestroyCommandPool(m_LogicalDevice->GetVulkanDevice(),
                            data.CommandPool, nullptr);
            delete data.CameraUniformBuffer;
        }

        vkDestroyPipeline(m_LogicalDevice->GetVulkanDevice(),
                          m_Pipeline, nullptr);
        vkDestroyPipelineLayout(m_LogicalDevice->GetVulkanDevice(),
                            m_PipelineLayout, nullptr);
        
        for (const auto& framebuffer : m_Framebuffers)
            delete framebuffer;

        vkDestroyRenderPass(m_LogicalDevice->GetVulkanDevice(),
                            m_RenderPass, nullptr);

        vkDestroyDescriptorPool(m_LogicalDevice->GetVulkanDevice(),
                                m_DescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_LogicalDevice->GetVulkanDevice(),
                                     m_CameraDescriptorSetLayout, nullptr);

        delete m_DepthImage;
        delete m_Swapchain;

        delete m_VertexBuffer;

        delete m_IndexBuffer;

        delete m_TestImageSampler;
        delete m_TestImage;
        delete m_LogicalDevice;

        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);

        delete m_PhysicalDevice;

        DestroyDebugUtilsMessanger(m_Instance, m_DebugMessenger, nullptr);
        vkDestroyInstance(m_Instance, nullptr);
    }

    void RendererContext::Resize(uint32_t width, uint32_t height)
    {
        m_LogicalDevice->WaitIdle();

        m_Swapchain->Resize(width, height);
        delete m_DepthImage;
        CreateDepthResources();

        uint32_t index = 0;
        for (const auto& framebuffer : m_Framebuffers)
        {
            std::vector attachments
            {
                m_Swapchain->GetImageViews().at(index++),
                m_DepthImage->GetVulkanImageView(),
            };
            framebuffer->Resize(attachments, width, height);
        }
    }

    void RendererContext::CreateVulkanInstance(
        std::string_view applicationName)
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
        const char** requiredExtensions = 
            glfwGetRequiredInstanceExtensions(&extensionCount);
        std::vector<const char*> extensions(
            requiredExtensions, requiredExtensions + extensionCount);

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

        std::set<std::string> requiredLayers(
            VulkanUtils::Layers.begin(), VulkanUtils::Layers.end());

        for (const auto& layer : supportedLayers)
            requiredLayers.erase(layer.layerName);

        return requiredLayers.empty();
    }

    void RendererContext::SetupDebugMessenger()
    {
        VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};
        SetupDebugUtilsMessengerCreateInfo(debugMessengerCreateInfo);
        assert(CreateDebugUtilsMessanger(m_Instance,
                                 &debugMessengerCreateInfo,
                                 nullptr, &m_DebugMessenger) == VK_SUCCESS);
    }

    void RendererContext::CreateSurface()
    {
        const Window* window = Application::Get()->GetWindow();
        assert(glfwCreateWindowSurface(m_Instance, window->GetNativeWindow(),
                                       nullptr, &m_Surface) == VK_SUCCESS);
    }

    void RendererContext::CreateRenderPass()
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_Swapchain->GetSurfaceFormat().format;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = m_DepthImage->GetFormat();
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = 
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        VkAttachmentReference attachmentRef;
        attachmentRef.attachment = 0;
        attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef;
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpassDescription{};
        subpassDescription.colorAttachmentCount = 1;
        subpassDescription.pDepthStencilAttachment = &depthAttachmentRef;
        subpassDescription.pColorAttachments = &attachmentRef;

        std::array attachments
        {
            colorAttachment, depthAttachment
        };

        VkRenderPassCreateInfo renderPassCreateInfo{};
        renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.attachmentCount = attachments.size();
        renderPassCreateInfo.pAttachments = attachments.data();
        renderPassCreateInfo.subpassCount = 1;
        renderPassCreateInfo.pSubpasses = &subpassDescription;

        VkSubpassDependency subpassDependency{};
        subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        subpassDependency.dstSubpass = 0;
        subpassDependency.srcStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        subpassDependency.srcAccessMask = 0;
        subpassDependency.dstStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        subpassDependency.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        renderPassCreateInfo.dependencyCount = 1;
        renderPassCreateInfo.pDependencies = &subpassDependency;

        assert(vkCreateRenderPass(m_LogicalDevice->GetVulkanDevice(),
                                &renderPassCreateInfo, nullptr,
                                &m_RenderPass) == VK_SUCCESS);
    }

    VkCommandPool RendererContext::CreateCommandPool(
        VkCommandPoolCreateFlags commandPoolFlags,
        uint32_t queueFamilyIndex) const
    {
        VkCommandPoolCreateInfo commandPoolCreateInfo{};
        commandPoolCreateInfo.sType =
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.flags = commandPoolFlags;
        commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

        VkCommandPool commandPool;
        assert(vkCreateCommandPool(m_LogicalDevice->GetVulkanDevice(),
                                   &commandPoolCreateInfo, nullptr,
                                   &commandPool) == VK_SUCCESS);
        return commandPool;
    }

    VkCommandBuffer RendererContext::AllocateCommandBuffer(
        VkCommandPool commandPool)
    {
        VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
        commandBufferAllocateInfo.sType =
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.commandBufferCount = 1;
        commandBufferAllocateInfo.commandPool = commandPool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        
        VkCommandBuffer commandBuffer;
        assert(vkAllocateCommandBuffers(m_LogicalDevice->GetVulkanDevice(),
                                        &commandBufferAllocateInfo,
                                        &commandBuffer) == VK_SUCCESS);
        return commandBuffer;
    }

    void RendererContext::RecordCommandBuffer(
        uint32_t imageIndex, VkCommandBuffer commandBuffer)
    {
        
        VkCommandBufferBeginInfo commandBufferInfo{};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        assert(vkBeginCommandBuffer(commandBuffer,
                                    &commandBufferInfo) == VK_SUCCESS);

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.framebuffer = m_Framebuffers.at(imageIndex)
                                        ->GetVulkanFramebuffer();
        renderPassInfo.renderPass = m_RenderPass;
        renderPassInfo.renderArea.extent = m_Swapchain->GetExtent();
        renderPassInfo.renderArea.offset = { 0, 0 };

        std::array<VkClearValue, 2> clearColor{};
        clearColor[0].color = {0.1f, 0.1f, 0.1f, 1.0f};
        clearColor[1].depthStencil= { 1.0f, 0, };
        renderPassInfo.clearValueCount = clearColor.size();
        renderPassInfo.pClearValues = clearColor.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo,
                             VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_Pipeline);

        VkDeviceSize deviceSizes[] = { 0 };
        VkBuffer vertexBuffer = m_VertexBuffer->GetVulkanBuffer();
        vkCmdBindVertexBuffers(commandBuffer, 0, 1,
                               &vertexBuffer, deviceSizes);

        vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->GetVulkanBuffer(),
                             0, VK_INDEX_TYPE_UINT32);

        VkViewport viewport;
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = m_Swapchain->GetExtent().width;
        viewport.height = m_Swapchain->GetExtent().height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor;
        scissor.offset = { 0, 0 };
        scissor.extent = m_Swapchain->GetExtent();
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        //vkCmdDraw(commandBuffer, m_Vertices.size(), 1, 0, 0);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_PipelineLayout, 0, 1,
                                &m_DescriptorSets.at(imageIndex), 0, nullptr);

        vkCmdDrawIndexed(commandBuffer, m_Indices.size(), 1, 0, 0, 0);

        vkCmdEndRenderPass(commandBuffer);
        vkEndCommandBuffer(commandBuffer);
    }

    void RendererContext::CreateSyncObjects(
        VkSemaphore& swapchainImageAcquireSemaphore,
        VkSemaphore& queueReadySemaphore, VkFence& presentFence)
    {
        VkSemaphoreCreateInfo semaphoreCreateInfo{};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        assert(vkCreateSemaphore(m_LogicalDevice->GetVulkanDevice(),
                                 &semaphoreCreateInfo, nullptr,
                                 &swapchainImageAcquireSemaphore) == VK_SUCCESS);

        assert(vkCreateSemaphore(m_LogicalDevice->GetVulkanDevice(),
                                 &semaphoreCreateInfo, nullptr,
                                 &queueReadySemaphore) == VK_SUCCESS);

        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        assert(vkCreateFence(m_LogicalDevice->GetVulkanDevice(),
                             &fenceCreateInfo, nullptr,
                             &presentFence) == VK_SUCCESS);
    }

    void RendererContext::CreatePerFrameObjects(uint32_t frameIndex)
    {
        PerFrameData& data = m_PerFrameData.at(frameIndex);
        const QueueFamilyIndices& queueFamilyIndices =
                                    m_PhysicalDevice->GetQueueFamilyIndices();
        data.CommandPool = CreateCommandPool(
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            queueFamilyIndices.GraphicsFamily.value());
        data.CommandBuffer = AllocateCommandBuffer(data.CommandPool);
        CreateSyncObjects(
            data.SwapchainImageAcquireSemaphore,
            data.QueueReadySemaphore,
            data.PresentFence);

        data.CameraUniformBuffer = new GPUBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            sizeof(CameraData), 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        data.CameraUniformBufferMemory = data.CameraUniformBuffer->MapMemory();
    }

    void RendererContext::DrawFrame()
    {
        const PerFrameData& currentFrameData = m_PerFrameData.at(m_FrameIndex);
        assert(vkWaitForFences(m_LogicalDevice->GetVulkanDevice(), 1,
                               &currentFrameData.PresentFence,
                               VK_TRUE, UINT64_MAX) == VK_SUCCESS);
        vkResetFences(m_LogicalDevice->GetVulkanDevice(), 1,
                      &currentFrameData.PresentFence);

        uint32_t imageIndex;
        m_Swapchain->AcquireNextImage(
                    currentFrameData.SwapchainImageAcquireSemaphore,
                    imageIndex);

        vkResetCommandBuffer(currentFrameData.CommandBuffer, 0);

        UpdateUniformBuffer(m_FrameIndex);

        RecordCommandBuffer(imageIndex, currentFrameData.CommandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &currentFrameData.CommandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &currentFrameData.QueueReadySemaphore;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = 
                            &currentFrameData.SwapchainImageAcquireSemaphore;

        VkPipelineStageFlags waitStages = 
                            { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.pWaitDstStageMask = &waitStages;

        assert(vkQueueSubmit(m_LogicalDevice->GetGraphicsQueue(), 1,
                             &submitInfo,
                             currentFrameData.PresentFence) == VK_SUCCESS);
        m_Swapchain->Present(currentFrameData.QueueReadySemaphore, imageIndex);

        m_FrameIndex = (m_FrameIndex + 1) % m_PerFrameData.size();
    }

    void RendererContext::CreateGraphicsPipeline()
    {
        VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
        graphicsPipelineCreateInfo.sType =
                            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        graphicsPipelineCreateInfo.renderPass = m_RenderPass;

        std::vector<char> vertexShaderData = ReadShaderFile(
                                        "assets/shaders/bin/BasicVert.spv");
        std::vector<char> fragmentShaderData = ReadShaderFile(
                                        "assets/shaders/bin/BasicFrag.spv");

        VkShaderModule vertexShaderModule = CreateShader(vertexShaderData);
        VkShaderModule fragmentShaderModule = CreateShader(fragmentShaderData);

#pragma region Shader Stages
            VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo{};
            vertexShaderStageCreateInfo.sType = 
                        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertexShaderStageCreateInfo.module = vertexShaderModule;
            vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            vertexShaderStageCreateInfo.pName = "main";

            VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo{};
            fragmentShaderStageCreateInfo.sType =
                        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragmentShaderStageCreateInfo.module = fragmentShaderModule;
            fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            fragmentShaderStageCreateInfo.pName = "main";

            std::array shaderStages =
            {
                vertexShaderStageCreateInfo,
                fragmentShaderStageCreateInfo
            };

            graphicsPipelineCreateInfo.pStages = shaderStages.data();
            graphicsPipelineCreateInfo.stageCount = shaderStages.size();
#pragma endregion


#pragma region Vertex Input State
            VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo{};
            vertexInputCreateInfo.sType =
                    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

            vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
            VkVertexInputBindingDescription vertexBindingDescription =
                                            Vertex::GetBindingDescription();
            vertexInputCreateInfo.pVertexBindingDescriptions = 
                                                    &vertexBindingDescription;

            std::array vertexAttributeDescription = 
                                            Vertex::GetAttributeDescriptions();
            vertexInputCreateInfo.vertexAttributeDescriptionCount =
                                            vertexAttributeDescription.size();
            vertexInputCreateInfo.pVertexAttributeDescriptions =
                                            vertexAttributeDescription.data();

            graphicsPipelineCreateInfo.pVertexInputState =
                                                        &vertexInputCreateInfo;
#pragma endregion

#pragma region Input Assembly State
            VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
            inputAssemblyStateCreateInfo.sType = 
                VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssemblyStateCreateInfo.topology =
                                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            graphicsPipelineCreateInfo.pInputAssemblyState =
                                                &inputAssemblyStateCreateInfo;
#pragma endregion

#pragma region Dynamic States
            std::array dynamicStates =
            {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR
            };

            VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
            dynamicStateCreateInfo.sType = 
                        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicStateCreateInfo.dynamicStateCount = dynamicStates.size();
            dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();
            
            graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
#pragma endregion

#pragma region Viewport State
            VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
            viewportStateCreateInfo.sType = 
                        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            // not setting the actual viewport and scissor states just how many 
            // there will be
            // that's because both are dynamic and will be specified later
            viewportStateCreateInfo.scissorCount = 1;
            viewportStateCreateInfo.viewportCount = 1;

            graphicsPipelineCreateInfo.pViewportState = 
                                                    &viewportStateCreateInfo;
#pragma endregion

#pragma region Rasterization State
            VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
            rasterizationStateCreateInfo.sType = 
                    VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

            // if true, then the fragments outside the depth range 
            // (the near and far planes) will be clamped, 
            // else they will be discarded
            // using this requires a gpu feature
            rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
            rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;

            rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
            rasterizationStateCreateInfo.lineWidth = 1.0f;
            rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
            rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
            rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;

            graphicsPipelineCreateInfo.pRasterizationState = 
                                                &rasterizationStateCreateInfo;
#pragma endregion

#pragma region Multisample State
            VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{};
            multisampleStateCreateInfo.sType = 
                    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampleStateCreateInfo.rasterizationSamples = 
                                                        VK_SAMPLE_COUNT_1_BIT;

            graphicsPipelineCreateInfo.pMultisampleState = 
                                                &multisampleStateCreateInfo;
#pragma endregion


#pragma region Color Blending
            VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};

            // specifies to which color components to write to
            colorBlendAttachmentState.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT
                | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT
                | VK_COLOR_COMPONENT_A_BIT;

            colorBlendAttachmentState.blendEnable = VK_FALSE;
            /* color blending op
            VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState2{};
            pipelineColorBlendAttachmentState2.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            pipelineColorBlendAttachmentState2.blendEnable = VK_TRUE;
            pipelineColorBlendAttachmentState2.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            pipelineColorBlendAttachmentState2.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            pipelineColorBlendAttachmentState2.colorBlendOp = VK_BLEND_OP_ADD;
            pipelineColorBlendAttachmentState2.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            pipelineColorBlendAttachmentState2.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            pipelineColorBlendAttachmentState2.alphaBlendOp = VK_BLEND_OP_ADD;
            */


            VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
            colorBlendStateCreateInfo.sType = 
                    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlendStateCreateInfo.attachmentCount = 1;
            colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;
            colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;

            graphicsPipelineCreateInfo.pColorBlendState = 
                                                    &colorBlendStateCreateInfo;
#pragma  endregion

#pragma region Pipeline Layout
            VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
            pipelineLayoutCreateInfo.sType = 
                                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutCreateInfo.setLayoutCount = 1;
            pipelineLayoutCreateInfo.pSetLayouts = &m_CameraDescriptorSetLayout;

            assert(vkCreatePipelineLayout(m_LogicalDevice->GetVulkanDevice(),
                                          &pipelineLayoutCreateInfo, nullptr,
                                          &m_PipelineLayout) == VK_SUCCESS);

            graphicsPipelineCreateInfo.layout = m_PipelineLayout;
#pragma endregion

#pragma region Render Pass;
            graphicsPipelineCreateInfo.renderPass = m_RenderPass;
            graphicsPipelineCreateInfo.subpass = 0;
#pragma endregion

#pragma region Depth Stencil State
            VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilCreateInfo{};
            pipelineDepthStencilCreateInfo.sType = 
                    VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            pipelineDepthStencilCreateInfo.depthTestEnable = VK_TRUE;
            pipelineDepthStencilCreateInfo.depthWriteEnable = VK_TRUE;
            pipelineDepthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;

            graphicsPipelineCreateInfo.pDepthStencilState = 
                                            &pipelineDepthStencilCreateInfo;
#pragma endregion

        assert(vkCreateGraphicsPipelines(m_LogicalDevice->GetVulkanDevice(),
                                         VK_NULL_HANDLE, 1,
                                         &graphicsPipelineCreateInfo, nullptr,
                                         &m_Pipeline) == VK_SUCCESS);

        vkDestroyShaderModule(m_LogicalDevice->GetVulkanDevice(),
                              vertexShaderModule, nullptr);
        vkDestroyShaderModule(m_LogicalDevice->GetVulkanDevice(),
                              fragmentShaderModule, nullptr);
    }

    VkShaderModule RendererContext::CreateShader(
        const std::vector<char>& shaderData)
    {
        VkShaderModuleCreateInfo shaderModuleCreateInfo{};
        shaderModuleCreateInfo.sType = 
                                VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleCreateInfo.codeSize = shaderData.size();
        shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(
                                                            shaderData.data());

        VkShaderModule shaderModule;
        assert(vkCreateShaderModule(m_LogicalDevice->GetVulkanDevice(),
                                    &shaderModuleCreateInfo, nullptr,
                                    &shaderModule) == VK_SUCCESS);
        return shaderModule;
    }

    void RendererContext::CreateVertexBuffer()
    {
        VkDeviceSize bufferSize = sizeof(Vertex) * m_Vertices.size();
        GPUBuffer stagingBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, bufferSize,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        void* data = stagingBuffer.MapMemory();
        memcpy(data, m_Vertices.data(), sizeof(Vertex) * m_Vertices.size());
        stagingBuffer.UnmapMemory();

        m_VertexBuffer = new GPUBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                       bufferSize,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        CopyBuffer(stagingBuffer.GetVulkanBuffer(),
                   m_VertexBuffer->GetVulkanBuffer(), bufferSize);
    }

    void RendererContext::CopyBuffer(
        VkBuffer sourceBuffer, VkBuffer destinationBuffer, VkDeviceSize size)
    {
        VkCommandBuffer copyCommandBuffer =
                        AllocateCommandBuffer(m_TransientTransferCommandPool);
        BeginCommandBuffer(copyCommandBuffer, 
                           VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        VkBufferCopy bufferCopy{};
        bufferCopy.size = size;
        vkCmdCopyBuffer(copyCommandBuffer, sourceBuffer, 
                        destinationBuffer, 1, &bufferCopy);

        EndCommandBuffer(copyCommandBuffer);

        // NOTE: using fences here would allow for multiple transfer operations to happen, potentially allowing the driver to optimize
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &copyCommandBuffer;
        assert(vkQueueSubmit(m_LogicalDevice->GetTransferQueue(), 1,
                             &submitInfo, VK_NULL_HANDLE) == VK_SUCCESS);

        vkQueueWaitIdle(m_LogicalDevice->GetTransferQueue());

        vkFreeCommandBuffers(m_LogicalDevice->GetVulkanDevice(),
                             m_TransientTransferCommandPool, 1,
                             &copyCommandBuffer);
    }

    void RendererContext::CreateIndexBuffer()
    {
        VkDeviceSize bufferSize = sizeof(uint32_t) * m_Indices.size();
        GPUBuffer stagingBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, bufferSize,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        void* data = stagingBuffer.MapMemory();
        memcpy(data, m_Indices.data(), bufferSize);
        stagingBuffer.UnmapMemory();

        m_IndexBuffer = new GPUBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                      bufferSize,
                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        CopyBuffer(stagingBuffer.GetVulkanBuffer(),
                   m_IndexBuffer->GetVulkanBuffer(), bufferSize);
    }

    void RendererContext::CreateCameraDescriptorSetLayout()
    {
        VkDescriptorSetLayoutBinding descriptorSetLayoutBinding{};
        descriptorSetLayoutBinding.binding = 0;
        descriptorSetLayoutBinding.descriptorCount = 1;
        descriptorSetLayoutBinding.descriptorType = 
                                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding textureDescriptorSetLayoutBinding{};
        textureDescriptorSetLayoutBinding.binding = 1;
        textureDescriptorSetLayoutBinding.descriptorCount = 1;
        textureDescriptorSetLayoutBinding.stageFlags = 
                                                VK_SHADER_STAGE_FRAGMENT_BIT;
        textureDescriptorSetLayoutBinding.descriptorType = 
                                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        std::array descriptorSetLayoutBindings = {
            descriptorSetLayoutBinding,
            textureDescriptorSetLayoutBinding,
        };

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.sType = 
                        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.bindingCount = 
                                            descriptorSetLayoutBindings.size();
        descriptorSetLayoutCreateInfo.pBindings = 
                                            descriptorSetLayoutBindings.data();

        assert(vkCreateDescriptorSetLayout(
                m_LogicalDevice->GetVulkanDevice(), 
                &descriptorSetLayoutCreateInfo, nullptr, 
                &m_CameraDescriptorSetLayout) == VK_SUCCESS);
    }

    static glm::vec3 position = { 0.0f, 0.0f, 4.0f };
    static glm::vec3 front = { 0.0f, 0.0f, 1.0f };
    static glm::vec3 right = { 1.0f, 0.0f, 0.0f };
    static glm::vec3 up = { 0.0f, 1.0f, 0.0f };
    static double lastMouseX, lastMouseY;
    static  bool first = true;
    static double pitch = 0.0, yaw = -90.0;

    static glm::mat4 testlookat(
        glm::vec3 position, glm::vec3 direction, glm::vec3 worldUp)
    {
        glm::vec3 front = glm::normalize(position - direction);
        glm::vec3 right = glm::normalize(
                            glm::cross(direction, 
                                       glm::normalize(worldUp)));

        glm::vec3 up =  glm::normalize(glm::cross(front, right));
        glm::mat4 directionMat = glm::mat4(1.0f);
        directionMat[0][0] = right.x;
        directionMat[1][0] = right.y;
        directionMat[2][0] = right.z;

        directionMat[0][1] = up.x;
        directionMat[1][1] = up.y;
        directionMat[2][1] = up.z;

        directionMat[0][2] = front.x;
        directionMat[1][2] = front.y;
        directionMat[2][2] = front.z;

        glm::mat4 positionMat(1.0f);
        positionMat[3][0] = -position.x;
        positionMat[3][1] = -position.y;
        positionMat[3][2] = -position.z;


        return directionMat * positionMat;
    }

    void RendererContext::UpdateUniformBuffer(uint32_t frameIndex)
    {
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();

        float time = std::chrono::duration<float>(startTime - currentTime)
                                                                    .count();

        GLFWwindow* window = 
                            Application::Get()->GetWindow()->GetNativeWindow();

        glm::vec3 velocity = {0.0f, 0.0f, 0.0f};

        if (glfwGetKey(window, GLFW_KEY_W))
            velocity += front;
        else if (glfwGetKey(window, GLFW_KEY_S))
            velocity -= front;

        if (glfwGetKey(window, GLFW_KEY_A))
            velocity -= right;
        else if(glfwGetKey(window, GLFW_KEY_D))
            velocity += right;

        if(glfwGetKey(window, GLFW_KEY_Q))
            velocity -= up;
        else if (glfwGetKey(window, GLFW_KEY_E))
            velocity += up;

        if(velocity != glm::vec3(0.0f))
            velocity = glm::normalize(velocity);

        position += velocity * Application::Get()->GetDeltaTime() * 6.0f;
        
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        if(first)
        {
            lastMouseX = mouseX;
            lastMouseY = mouseY;
            first = false;
        }

        double xoffset = (mouseX - lastMouseX) * 0.1f;
        double yoffset = (mouseY - lastMouseY) * 0.1f;
        lastMouseX = mouseX;
        lastMouseY = mouseY;

        pitch += yoffset;
        yaw += xoffset;

        if (pitch > 89.0f)
            pitch = 89.0f;
        if (pitch < -89.0f)
            pitch = -89.0f;

        glm::vec3 direction = { 0.0f, 0.0f, 0.0f };
        direction.x = glm::cos(glm::radians(yaw)) * 
                                                glm::cos(glm::radians(pitch));
        direction.y = glm::sin(glm::radians(pitch));
        direction.z = glm::sin(glm::radians(yaw)) * 
                                                glm::cos(glm::radians(pitch));
        front = glm::normalize(direction);

        if (glfwGetKey(window, GLFW_KEY_R))
        {
            position = { 0.0f, 0.0f, 4.0f };
            direction = { 0.0f, 0.0f, -1.0f };
        }

        right = glm::normalize(glm::cross(front, {0.0f, 1.0f, 0.0f}));
        up = glm::normalize(glm::cross(right, front));

        CameraData cameraData;

        cameraData.View = glm::lookAt(position, position + front, up);
        auto swapchainExtent = m_Swapchain->GetExtent();
        cameraData.Projection = glm::perspective(
                                    glm::radians(fov), 
                                    swapchainExtent.width / 
                                    (float)swapchainExtent.height, 
                                    0.1f, 50.0f);
        
        const PerFrameData& data = m_PerFrameData.at(frameIndex);
        memcpy(data.CameraUniformBufferMemory, &cameraData, sizeof(CameraData));
    }

    void RendererContext::CreateDescriptorPool()
    {
        VkDescriptorPoolSize descriptorPoolSize;
        descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorPoolSize.descriptorCount = m_PerFrameData.size();

        VkDescriptorPoolSize textureDescriptorPoolSize;
        textureDescriptorPoolSize.type = 
                                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        textureDescriptorPoolSize.descriptorCount = m_PerFrameData.size();

        std::array descriptorPoolSizes = {
            descriptorPoolSize,
            textureDescriptorPoolSize,
        };

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
        descriptorPoolCreateInfo.sType = 
                                VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolCreateInfo.poolSizeCount = descriptorPoolSizes.size();
        descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
        descriptorPoolCreateInfo.maxSets = m_PerFrameData.size();

        assert(vkCreateDescriptorPool(m_LogicalDevice->GetVulkanDevice(), 
                                      &descriptorPoolCreateInfo, 
                                      nullptr, 
                                      &m_DescriptorPool) == VK_SUCCESS);

        
    }

    void RendererContext::CreateDescriptorSets()
    {
        std::vector descriptorSetLayouts(m_PerFrameData.size(), 
                                         m_CameraDescriptorSetLayout);

        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
        descriptorSetAllocateInfo.sType = 
                                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.descriptorPool = m_DescriptorPool;
        descriptorSetAllocateInfo.descriptorSetCount = 
                                                descriptorSetLayouts.size();
        descriptorSetAllocateInfo.pSetLayouts = descriptorSetLayouts.data();

        m_DescriptorSets.resize(m_PerFrameData.size());
        assert(vkAllocateDescriptorSets(m_LogicalDevice->GetVulkanDevice(), 
                                        &descriptorSetAllocateInfo, 
                                        m_DescriptorSets.data()) == VK_SUCCESS);

        for (size_t i = 0; i < m_PerFrameData.size(); i++)
        {
            const PerFrameData& perframeData = m_PerFrameData.at(i);
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = 
                            perframeData.CameraUniformBuffer->GetVulkanBuffer();
            bufferInfo.range = sizeof(CameraData);
            bufferInfo.offset = 0;

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = m_TestImage->GetVulkanImageView();
            imageInfo.sampler = m_TestImageSampler->GetVulkanSampler();

            std::array<VkWriteDescriptorSet, 2> writeDescriptorSets{};

            VkWriteDescriptorSet writeDescriptorSet{};
            writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSet.dstSet = m_DescriptorSets.at(i);
            writeDescriptorSet.dstBinding = 0;
            writeDescriptorSet.dstArrayElement = 0;

            writeDescriptorSet.descriptorType = 
                                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeDescriptorSet.descriptorCount = 1;
            writeDescriptorSet.pBufferInfo = &bufferInfo;
            writeDescriptorSets[0] = writeDescriptorSet;

            VkWriteDescriptorSet textureWriteDescriptorSet{};
            textureWriteDescriptorSet.sType = 
                                        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            textureWriteDescriptorSet.dstSet = m_DescriptorSets.at(i);
            textureWriteDescriptorSet.dstBinding = 1;
            textureWriteDescriptorSet.dstArrayElement = 0;
            textureWriteDescriptorSet.descriptorType = 
                                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            textureWriteDescriptorSet.descriptorCount = 1;
            textureWriteDescriptorSet.pImageInfo = &imageInfo;
            writeDescriptorSets[1] = textureWriteDescriptorSet;


            vkUpdateDescriptorSets(m_LogicalDevice->GetVulkanDevice(), 
                                   writeDescriptorSets.size(), 
                                   writeDescriptorSets.data(), 0, nullptr);
        }
    }

    void RendererContext::CreateTexture()
    {
        int width, height, channels;
        stbi_uc* imageData = stbi_load("assets/test.png", &width, &height, 
                                       &channels, STBI_rgb_alpha);

        assert(imageData != nullptr);

        VkDeviceSize imageSize = width * height * 4;

        GPUBuffer stagingBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, imageSize, 
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        void* data = stagingBuffer.MapMemory();
        memcpy(data, imageData, imageSize);
        stagingBuffer.UnmapMemory();

        stbi_image_free(imageData);

        ImageCreateInfo imageCreateInfo{};
        imageCreateInfo.Width = width;
        imageCreateInfo.Height = height;
        imageCreateInfo.Format = VK_FORMAT_R8G8B8A8_SRGB;
        imageCreateInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageCreateInfo.MemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        imageCreateInfo.AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

        m_TestImage = new Image(imageCreateInfo);
        m_TestImage->TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        CopyBufferToImage(stagingBuffer.GetVulkanBuffer(), m_TestImage->GetVulkanImage(), width, height);
        m_TestImage->TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        SamplerCreateInfo samplerCreateInfo{
            .MagFilter = TextureFilter::Nearest,
            .MinFilter = TextureFilter::Nearest,
            .MipmapFilter = TextureFilter::Nearest,
            .AddressModeU = TextureAddressMode::Repeat,
            .AddressModeV = TextureAddressMode::Repeat,
            .AnisotropyEnable = true,
        };

        m_TestImageSampler = new Sampler(samplerCreateInfo);
    }

    void RendererContext::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling imageTiling,
        VkImageUsageFlags imageUsage, VkMemoryPropertyFlags memoryProperties, VkImage& image,
        VkDeviceMemory& imageMemory)
    {
        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.extent.width = width;
        imageCreateInfo.extent.height = height;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.tiling = imageTiling;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.usage = imageUsage;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;

        const auto& queueFamilyIndices = m_PhysicalDevice->GetQueueFamilyIndices();
        std::array queueFamilyIndicesArr =
        {
            queueFamilyIndices.GraphicsFamily.value(),
            queueFamilyIndices.TransferFamily.value(),
        };

        imageCreateInfo.queueFamilyIndexCount = queueFamilyIndicesArr.size();
        imageCreateInfo.pQueueFamilyIndices = queueFamilyIndicesArr.data();
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        assert(vkCreateImage(m_LogicalDevice->GetVulkanDevice(), &imageCreateInfo, nullptr, &image) == VK_SUCCESS);

        VkMemoryRequirements imageMemoryRequirements;
        vkGetImageMemoryRequirements(m_LogicalDevice->GetVulkanDevice(), image, &imageMemoryRequirements);

        VkMemoryAllocateInfo imageMemoryAllocateInfo{};
        imageMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        imageMemoryAllocateInfo.allocationSize = imageMemoryRequirements.size;
        imageMemoryAllocateInfo.memoryTypeIndex = m_PhysicalDevice->FindMemoryType(imageMemoryRequirements.memoryTypeBits, memoryProperties);

        assert(vkAllocateMemory(m_LogicalDevice->GetVulkanDevice(), &imageMemoryAllocateInfo, nullptr, &imageMemory) == VK_SUCCESS);

        assert(vkBindImageMemory(m_LogicalDevice->GetVulkanDevice(), image, imageMemory, 0) == VK_SUCCESS);
    }

    void RendererContext::TransitionImageLayout( VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkImageMemoryBarrier memoryBarrier{};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        memoryBarrier.image = image;
        memoryBarrier.oldLayout = oldLayout;
        memoryBarrier.newLayout = newLayout;
        memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        memoryBarrier.subresourceRange.baseArrayLayer = 0;
        memoryBarrier.subresourceRange.baseMipLevel = 0;
        memoryBarrier.subresourceRange.layerCount = 1;
        memoryBarrier.subresourceRange.levelCount = 1;
        memoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

        if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            commandBuffer = AllocateCommandBuffer(m_TransientGraphicsCommandPool);
        else
            commandBuffer = AllocateCommandBuffer(m_TransientTransferCommandPool);

        BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        VkPipelineStageFlags srcStageFlags = 0;
        VkPipelineStageFlags dstStageFlags = 0;

        if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            srcStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;

            memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            srcStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

            memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        }
        else
        {
            assert(false);
        }

        vkCmdPipelineBarrier(commandBuffer, srcStageFlags, dstStageFlags, 
            0, 0, nullptr, 
            0, nullptr, 1, &memoryBarrier);
        EndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        if(newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            assert(vkQueueSubmit(m_LogicalDevice->GetTransferQueue(), 1, &submitInfo, VK_NULL_HANDLE) == VK_SUCCESS);
            assert(vkQueueWaitIdle(m_LogicalDevice->GetTransferQueue()) == VK_SUCCESS);
        }
        else
        {
            assert(vkQueueSubmit(m_LogicalDevice->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE) == VK_SUCCESS);
            assert(vkQueueWaitIdle(m_LogicalDevice->GetGraphicsQueue()) == VK_SUCCESS);
        }

        if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            vkFreeCommandBuffers(m_LogicalDevice->GetVulkanDevice(), m_TransientGraphicsCommandPool, 1, &commandBuffer);
        else
            vkFreeCommandBuffers(m_LogicalDevice->GetVulkanDevice(), m_TransientTransferCommandPool, 1, &commandBuffer);
    }

    void RendererContext::CopyBufferToImage(VkBuffer source, VkImage destination, uint32_t width, uint32_t height)
    {
        VkCommandBuffer copyCommandBuffer = AllocateCommandBuffer(m_TransientTransferCommandPool);
        BeginCommandBuffer(copyCommandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        VkBufferImageCopy bufferImageCopy{};
        bufferImageCopy.bufferImageHeight = 0;
        bufferImageCopy.bufferOffset = 0;
        bufferImageCopy.bufferRowLength = 0;
        bufferImageCopy.imageExtent = {  width, height, 1 };
        bufferImageCopy.imageOffset = { 0, 0, 0 };
        bufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferImageCopy.imageSubresource.baseArrayLayer = 0;
        bufferImageCopy.imageSubresource.layerCount = 1;
        bufferImageCopy.imageSubresource.mipLevel = 0;

        vkCmdCopyBufferToImage(copyCommandBuffer, source, destination, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy);

        assert(vkEndCommandBuffer(copyCommandBuffer) == VK_SUCCESS);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &copyCommandBuffer;
        assert(vkQueueSubmit(m_LogicalDevice->GetTransferQueue(), 1, &submitInfo, VK_NULL_HANDLE) == VK_SUCCESS);
        vkQueueWaitIdle(m_LogicalDevice->GetTransferQueue());
        vkFreeCommandBuffers(m_LogicalDevice->GetVulkanDevice(), m_TransientTransferCommandPool, 1, &copyCommandBuffer);
    }

    void RendererContext::BeginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags usageFlags)
    {
        VkCommandBufferBeginInfo commandBufferBeginInfo{};
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandBufferBeginInfo.flags = usageFlags;

        assert(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) == VK_SUCCESS);
    }

    void RendererContext::EndCommandBuffer(VkCommandBuffer commandBuffer)
    {
        assert(vkEndCommandBuffer(commandBuffer) == VK_SUCCESS);
    }

    void RendererContext::CreateImageView(const VkImage& image, VkFormat image_format, VkImageAspectFlags image_aspect_flags, VkImageView& image_view)
    {
        VkImageViewCreateInfo image_view_create_info{};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image = image;
        image_view_create_info.subresourceRange.aspectMask = image_aspect_flags;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.layerCount = 1;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.format = image_format;
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;

        assert(vkCreateImageView(m_LogicalDevice->GetVulkanDevice(), &image_view_create_info, nullptr, &image_view) == VK_SUCCESS);
    }

    void RendererContext::CreateDepthResources()
    {
        std::array desiredDepthFormats = {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
        };

        VkFormat depthFormat = VK_FORMAT_UNDEFINED;
        for (auto desiredDepthFormat : desiredDepthFormats)
        {
            VkFormatProperties format_properties;
            vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice->GetPhysicalDevice(), desiredDepthFormat, &format_properties);

            if((format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            {
                depthFormat = desiredDepthFormat;
                break;
            }
            /*else if((format_properties.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            {
                depth_format = desired_depth_format;
                break;
            }*/
        }

        assert(depthFormat != VK_FORMAT_UNDEFINED);

        auto swapchain_extent = m_Swapchain->GetExtent();
        ImageCreateInfo depthImageCreateInfo{};
        depthImageCreateInfo.Width = swapchain_extent.width;
        depthImageCreateInfo.Height = swapchain_extent.height;
        depthImageCreateInfo.Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthImageCreateInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
        depthImageCreateInfo.Format = depthFormat;
        depthImageCreateInfo.MemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        depthImageCreateInfo.AspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
        
        m_DepthImage = new Image(depthImageCreateInfo);
    }

    void RendererContext::AddCube(const glm::mat4& transformMatrix)
    {
        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{  0.5f, -0.5f, 0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = { 0.0f, 1.0f }, });
        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{  0.5f,  0.5f, 0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {0.0f, 0.0f}, });
        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{ -0.5f,  0.5f, 0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {1.0f, 0.0f}, });
        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{ -0.5f, -0.5f, 0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {1.0f, 1.0f}, });

        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{ -0.5f, -0.5f, -0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {0.0f, 1.0f}, });
        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{ -0.5f,  0.5f, -0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {0.0f, 0.0f}, });
        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{  0.5f,  0.5f, -0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {1.0f, 0.0f}, });
        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{  0.5f, -0.5f, -0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {1.0f, 1.0f}, });

        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{ -0.5f, -0.5f,  0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {0.0f, 1.0f}, });
        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{ -0.5f,  0.5f,  0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {0.0f, 0.0f}, });
        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{ -0.5f,  0.5f, -0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {1.0f, 0.0f}, });
        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{ -0.5f, -0.5f, -0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {1.0f, 1.0f}, });

        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{ 0.5f, -0.5f, -0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {0.0f, 1.0f}, });
        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{ 0.5f,  0.5f, -0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {0.0f, 0.0f}, });
        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{ 0.5f,  0.5f,  0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {1.0f, 0.0f}, });
        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{ 0.5f, -0.5f,  0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {1.0f, 1.0f}, });

        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{  0.5f, -0.5f,  0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {0.0f, 1.0f}, });
        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{ -0.5f, -0.5f,  0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {0.0f, 0.0f}, });
        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{ -0.5f, -0.5f, -0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {1.0f, 0.0f}, });
        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{  0.5f, -0.5f, -0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {1.0f, 1.0f}, });

        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{  0.5f, 0.5f, -0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {0.0f, 1.0f}, });
        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{ -0.5f, 0.5f, -0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {0.0f, 0.0f}, });
        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{ -0.5f, 0.5f,  0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {1.0f, 0.0f}, });
        m_Vertices.push_back({ .Position = transformMatrix * glm::vec4{  0.5f, 0.5f,  0.5f, 1.0f }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {1.0f, 1.0f}, });

        size_t previousSize = m_Indices.size();
        m_Indices.resize( previousSize + 36);
        for (uint32_t i = 0; i < 6; ++i)
        {
            m_Indices[previousSize + (i * 6 + 0)] = currentIndex + 0;
            m_Indices[previousSize + (i * 6 + 1)] = currentIndex + 1;
            m_Indices[previousSize + (i * 6 + 2)] = currentIndex + 2;
            m_Indices[previousSize + (i * 6 + 3)] = currentIndex + 2;
            m_Indices[previousSize + (i * 6 + 4)] = currentIndex + 3;
            m_Indices[previousSize + (i * 6 + 5)] = currentIndex + 0;
            currentIndex +=  4;
        }

    }
}
