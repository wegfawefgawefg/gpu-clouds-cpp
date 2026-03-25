#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <SDL3/SDL_video.h>
#include <imgui.h>
#include <vulkan/vulkan.h>

#include "cloud_types.h"
#include "vulkan_helpers.h"

struct VulkanRenderer
{
    ~VulkanRenderer();

    void Initialize(SDL_Window* window);
    void Shutdown();

    void InitializeImGui();
    void ShutdownImGui();

    void Resize(
        std::uint32_t windowWidth,
        std::uint32_t windowHeight,
        std::uint32_t renderWidth,
        std::uint32_t renderHeight
    );
    void Render(
        const GpuFrameParams& params,
        std::span<const std::uint32_t> overlayPixels,
        ImDrawData* drawData
    );

    void CreateInstance();
    void CreateSurface();
    void PickPhysicalDevice();
    void CreateDevice();
    void CreateCommandObjects();
    void CreateSyncObjects();
    void CreateStaticBuffers();
    void CreateDescriptorObjects();
    void CreatePresentDescriptorObjects();
    void CreateComputePipeline();
    void CreatePresentPipeline();
    void CreateImguiDescriptorPool();
    void CreateSwapchain(std::uint32_t width, std::uint32_t height);
    void CreateSwapchainViews();
    void CreateImguiRenderPass();
    void CreateSwapchainFramebuffers();
    void CreateTargets(std::uint32_t renderWidth, std::uint32_t renderHeight);
    void DestroySwapchain();
    void UpdateDescriptorSets();
    void RecordCommandBuffer(std::uint32_t swapchainImageIndex, ImDrawData* drawData);

    VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes) const;
    std::uint32_t ChooseQueueFamily() const;

    SDL_Window* m_window = nullptr;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    std::uint32_t m_queueFamilyIndex = 0;
    VkQueue m_queue = VK_NULL_HANDLE;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapchainExtent = {};
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainViews;
    std::vector<VkFramebuffer> m_swapchainFramebuffers;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
    VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence m_frameFence = VK_NULL_HANDLE;

    BufferResource m_paramsBuffer;
    BufferResource m_overlayBuffer;
    ImageResource m_renderTarget;
    ImageResource m_presentTarget;

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkShaderModule m_shaderModule = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_presentDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_presentDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_presentDescriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout m_presentPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_presentPipeline = VK_NULL_HANDLE;
    VkShaderModule m_presentShaderModule = VK_NULL_HANDLE;

    VkDescriptorPool m_imguiDescriptorPool = VK_NULL_HANDLE;
    VkRenderPass m_imguiRenderPass = VK_NULL_HANDLE;

    std::uint32_t m_renderWidth = 0;
    std::uint32_t m_renderHeight = 0;
    bool m_renderTargetPrimed = false;
    bool m_presentTargetPrimed = false;
    bool m_imguiInitialized = false;
};
