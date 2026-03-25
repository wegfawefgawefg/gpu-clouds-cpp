#include "vulkan_renderer.h"

#include <algorithm>
#include <array>
#include <stdexcept>

#include <SDL3/SDL_vulkan.h>
#include <backends/imgui_impl_vulkan.h>

namespace
{
constexpr VkFormat kRenderTargetFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat kPresentTargetFormat = VK_FORMAT_R8G8B8A8_UNORM;
constexpr std::size_t kOverlayBufferBytes = sizeof(std::uint32_t) * kOverlayPixelCount;
constexpr const char* kCloudShaderPath = GPU_CLOUDS_SHADER_PATH;
constexpr const char* kPresentShaderPath = GPU_PRESENT_SHADER_PATH;

VkShaderModule CreateShaderModule(VkDevice device, std::string_view path)
{
    const std::vector<std::byte> shaderBytes = ReadBinaryFile(path);
    VkShaderModuleCreateInfo moduleInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shaderBytes.size(),
        .pCode = reinterpret_cast<const std::uint32_t*>(shaderBytes.data()),
    };

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    CheckVk(
        vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule),
        "vkCreateShaderModule"
    );
    return shaderModule;
}
} // namespace

VulkanRenderer::~VulkanRenderer()
{
    Shutdown();
}

void VulkanRenderer::Initialize(SDL_Window* window)
{
    m_window = window;
    CreateInstance();
    CreateSurface();
    PickPhysicalDevice();
    CreateDevice();
    CreateCommandObjects();
    CreateSyncObjects();
    CreateStaticBuffers();
    CreateDescriptorObjects();
    CreatePresentDescriptorObjects();
    CreateComputePipeline();
    CreatePresentPipeline();
    CreateImguiDescriptorPool();
}

void VulkanRenderer::Shutdown()
{
    if (m_device == VK_NULL_HANDLE)
    {
        return;
    }

    vkDeviceWaitIdle(m_device);
    DestroySwapchain();

    if (m_imguiRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(m_device, m_imguiRenderPass, nullptr);
        m_imguiRenderPass = VK_NULL_HANDLE;
    }
    if (m_imguiDescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_device, m_imguiDescriptorPool, nullptr);
        m_imguiDescriptorPool = VK_NULL_HANDLE;
    }

    if (m_pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    if (m_shaderModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(m_device, m_shaderModule, nullptr);
        m_shaderModule = VK_NULL_HANDLE;
    }

    if (m_presentPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device, m_presentPipeline, nullptr);
        m_presentPipeline = VK_NULL_HANDLE;
    }
    if (m_presentPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_device, m_presentPipelineLayout, nullptr);
        m_presentPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_presentShaderModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(m_device, m_presentShaderModule, nullptr);
        m_presentShaderModule = VK_NULL_HANDLE;
    }

    if (m_descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_presentDescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_device, m_presentDescriptorPool, nullptr);
        m_presentDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_presentDescriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(m_device, m_presentDescriptorSetLayout, nullptr);
        m_presentDescriptorSetLayout = VK_NULL_HANDLE;
    }

    DestroyImage(m_device, m_presentTarget);
    DestroyImage(m_device, m_renderTarget);
    DestroyBuffer(m_device, m_overlayBuffer);
    DestroyBuffer(m_device, m_paramsBuffer);

    if (m_frameFence != VK_NULL_HANDLE)
    {
        vkDestroyFence(m_device, m_frameFence, nullptr);
        m_frameFence = VK_NULL_HANDLE;
    }
    if (m_renderFinishedSemaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(m_device, m_renderFinishedSemaphore, nullptr);
        m_renderFinishedSemaphore = VK_NULL_HANDLE;
    }
    if (m_imageAvailableSemaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(m_device, m_imageAvailableSemaphore, nullptr);
        m_imageAvailableSemaphore = VK_NULL_HANDLE;
    }
    if (m_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }

    vkDestroyDevice(m_device, nullptr);
    m_device = VK_NULL_HANDLE;

    if (m_surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::InitializeImGui()
{
    if (m_imguiInitialized)
    {
        return;
    }

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = m_instance;
    initInfo.PhysicalDevice = m_physicalDevice;
    initInfo.Device = m_device;
    initInfo.QueueFamily = m_queueFamilyIndex;
    initInfo.Queue = m_queue;
    initInfo.DescriptorPool = m_imguiDescriptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = static_cast<std::uint32_t>(m_swapchainImages.size());
    initInfo.PipelineInfoMain.RenderPass = m_imguiRenderPass;
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.CheckVkResultFn = [](VkResult result) { CheckVk(result, "imgui"); };

    if (!ImGui_ImplVulkan_Init(&initInfo))
    {
        throw std::runtime_error("ImGui_ImplVulkan_Init failed");
    }

    m_imguiInitialized = true;
}

void VulkanRenderer::ShutdownImGui()
{
    if (!m_imguiInitialized)
    {
        return;
    }

    ImGui_ImplVulkan_Shutdown();
    m_imguiInitialized = false;
}

void VulkanRenderer::Resize(
    std::uint32_t windowWidth,
    std::uint32_t windowHeight,
    std::uint32_t renderWidth,
    std::uint32_t renderHeight
)
{
    if (windowWidth == 0 || windowHeight == 0 || renderWidth == 0 || renderHeight == 0)
    {
        return;
    }

    vkDeviceWaitIdle(m_device);
    DestroySwapchain();
    CreateSwapchain(windowWidth, windowHeight);
    CreateSwapchainViews();
    CreateImguiRenderPass();
    CreateSwapchainFramebuffers();
    CreateTargets(renderWidth, renderHeight);
    UpdateDescriptorSets();

    if (m_imguiInitialized)
    {
        ImGui_ImplVulkan_SetMinImageCount(2);
        ImGui_ImplVulkan_PipelineInfo pipelineInfo = {};
        pipelineInfo.RenderPass = m_imguiRenderPass;
        pipelineInfo.Subpass = 0;
        pipelineInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        ImGui_ImplVulkan_CreateMainPipeline(&pipelineInfo);
    }
}

void VulkanRenderer::CreateInstance()
{
    Uint32 extensionCount = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (extensions == nullptr)
    {
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed");
    }

    VkApplicationInfo applicationInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "gpu-clouds-cpp",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "none",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &applicationInfo,
        .enabledExtensionCount = extensionCount,
        .ppEnabledExtensionNames = extensions,
    };
    CheckVk(vkCreateInstance(&createInfo, nullptr, &m_instance), "vkCreateInstance");
}

void VulkanRenderer::CreateSurface()
{
    if (!SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface))
    {
        throw std::runtime_error("SDL_Vulkan_CreateSurface failed");
    }
}

void VulkanRenderer::PickPhysicalDevice()
{
    std::uint32_t deviceCount = 0;
    CheckVk(
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr),
        "vkEnumeratePhysicalDevices"
    );

    std::vector<VkPhysicalDevice> devices(deviceCount);
    CheckVk(
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data()),
        "vkEnumeratePhysicalDevices"
    );

    for (VkPhysicalDevice device : devices)
    {
        m_physicalDevice = device;
        try
        {
            m_queueFamilyIndex = ChooseQueueFamily();
            return;
        }
        catch (const std::exception&)
        {
        }
    }

    throw std::runtime_error("Failed to find a Vulkan device with graphics, compute, and present");
}

void VulkanRenderer::CreateDevice()
{
    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = m_queueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    const std::array deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
    };
    CheckVk(
        vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device),
        "vkCreateDevice"
    );
    vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);
}

void VulkanRenderer::CreateCommandObjects()
{
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_queueFamilyIndex,
    };
    CheckVk(
        vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool),
        "vkCreateCommandPool"
    );

    VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    CheckVk(
        vkAllocateCommandBuffers(m_device, &allocateInfo, &m_commandBuffer),
        "vkAllocateCommandBuffers"
    );
}

void VulkanRenderer::CreateSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreInfo = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    CheckVk(
        vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphore),
        "vkCreateSemaphore"
    );
    CheckVk(
        vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphore),
        "vkCreateSemaphore"
    );

    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    CheckVk(vkCreateFence(m_device, &fenceInfo, nullptr, &m_frameFence), "vkCreateFence");
}

void VulkanRenderer::CreateStaticBuffers()
{
    m_paramsBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        sizeof(GpuFrameParams),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_overlayBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        kOverlayBufferBytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
}

void VulkanRenderer::CreateDescriptorObjects()
{
    const std::array bindings = {
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<std::uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };
    CheckVk(
        vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout),
        "vkCreateDescriptorSetLayout"
    );

    const std::array poolSizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
    };
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = static_cast<std::uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };
    CheckVk(
        vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool),
        "vkCreateDescriptorPool"
    );

    VkDescriptorSetAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_descriptorSetLayout,
    };
    CheckVk(
        vkAllocateDescriptorSets(m_device, &allocateInfo, &m_descriptorSet),
        "vkAllocateDescriptorSets"
    );
}

void VulkanRenderer::CreatePresentDescriptorObjects()
{
    const std::array bindings = {
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<std::uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };
    CheckVk(
        vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_presentDescriptorSetLayout),
        "vkCreateDescriptorSetLayout"
    );

    const std::array poolSizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
    };
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = static_cast<std::uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };
    CheckVk(
        vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_presentDescriptorPool),
        "vkCreateDescriptorPool"
    );

    VkDescriptorSetAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_presentDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_presentDescriptorSetLayout,
    };
    CheckVk(
        vkAllocateDescriptorSets(m_device, &allocateInfo, &m_presentDescriptorSet),
        "vkAllocateDescriptorSets"
    );
}

void VulkanRenderer::CreateComputePipeline()
{
    m_shaderModule = CreateShaderModule(m_device, kCloudShaderPath);

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &m_descriptorSetLayout,
    };
    CheckVk(
        vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_pipelineLayout),
        "vkCreatePipelineLayout"
    );

    VkPipelineShaderStageCreateInfo shaderStage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = m_shaderModule,
        .pName = "main",
    };
    VkComputePipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = shaderStage,
        .layout = m_pipelineLayout,
    };
    CheckVk(
        vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline),
        "vkCreateComputePipelines"
    );
}

void VulkanRenderer::CreatePresentPipeline()
{
    m_presentShaderModule = CreateShaderModule(m_device, kPresentShaderPath);

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &m_presentDescriptorSetLayout,
    };
    CheckVk(
        vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_presentPipelineLayout),
        "vkCreatePipelineLayout"
    );

    VkPipelineShaderStageCreateInfo shaderStage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = m_presentShaderModule,
        .pName = "main",
    };
    VkComputePipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = shaderStage,
        .layout = m_presentPipelineLayout,
    };
    CheckVk(
        vkCreateComputePipelines(
            m_device,
            VK_NULL_HANDLE,
            1,
            &pipelineInfo,
            nullptr,
            &m_presentPipeline
        ),
        "vkCreateComputePipelines"
    );
}

void VulkanRenderer::CreateImguiDescriptorPool()
{
    const VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE},
    };
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE,
        .poolSizeCount = 1,
        .pPoolSizes = poolSizes,
    };
    CheckVk(
        vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_imguiDescriptorPool),
        "vkCreateDescriptorPool"
    );
}

void VulkanRenderer::CreateSwapchain(std::uint32_t width, std::uint32_t height)
{
    VkSurfaceCapabilitiesKHR capabilities;
    CheckVk(
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities),
        "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"
    );

    std::uint32_t formatCount = 0;
    CheckVk(
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr),
        "vkGetPhysicalDeviceSurfaceFormatsKHR"
    );
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    CheckVk(
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            m_physicalDevice,
            m_surface,
            &formatCount,
            formats.data()
        ),
        "vkGetPhysicalDeviceSurfaceFormatsKHR"
    );

    std::uint32_t presentModeCount = 0;
    CheckVk(
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            m_physicalDevice,
            m_surface,
            &presentModeCount,
            nullptr
        ),
        "vkGetPhysicalDeviceSurfacePresentModesKHR"
    );
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    CheckVk(
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            m_physicalDevice,
            m_surface,
            &presentModeCount,
            presentModes.data()
        ),
        "vkGetPhysicalDeviceSurfacePresentModesKHR"
    );

    const VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(formats);
    const VkPresentModeKHR presentMode = ChoosePresentMode(presentModes);
    m_swapchainFormat = surfaceFormat.format;
    m_swapchainExtent.width =
        std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    m_swapchainExtent.height =
        std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    std::uint32_t imageCount = std::max(2u, capabilities.minImageCount);
    if (capabilities.maxImageCount > 0)
    {
        imageCount = std::min(imageCount, capabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = m_surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = m_swapchainExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
    };
    CheckVk(
        vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain),
        "vkCreateSwapchainKHR"
    );

    std::uint32_t swapchainImageCount = 0;
    CheckVk(
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchainImageCount, nullptr),
        "vkGetSwapchainImagesKHR"
    );
    m_swapchainImages.resize(swapchainImageCount);
    CheckVk(
        vkGetSwapchainImagesKHR(
            m_device,
            m_swapchain,
            &swapchainImageCount,
            m_swapchainImages.data()
        ),
        "vkGetSwapchainImagesKHR"
    );
}

void VulkanRenderer::CreateSwapchainViews()
{
    m_swapchainViews.resize(m_swapchainImages.size());
    for (std::size_t index = 0; index < m_swapchainImages.size(); ++index)
    {
        VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = m_swapchainImages[index],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = m_swapchainFormat,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };
        CheckVk(
            vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapchainViews[index]),
            "vkCreateImageView"
        );
    }
}

void VulkanRenderer::CreateImguiRenderPass()
{
    if (m_imguiRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(m_device, m_imguiRenderPass, nullptr);
        m_imguiRenderPass = VK_NULL_HANDLE;
    }

    VkAttachmentDescription attachment = {
        .format = m_swapchainFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference colorReference = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorReference,
    };
    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
    };
    VkRenderPassCreateInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };
    CheckVk(
        vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_imguiRenderPass),
        "vkCreateRenderPass"
    );
}

void VulkanRenderer::CreateSwapchainFramebuffers()
{
    m_swapchainFramebuffers.resize(m_swapchainViews.size());
    for (std::size_t index = 0; index < m_swapchainViews.size(); ++index)
    {
        VkImageView attachments[] = {m_swapchainViews[index]};
        VkFramebufferCreateInfo framebufferInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = m_imguiRenderPass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = m_swapchainExtent.width,
            .height = m_swapchainExtent.height,
            .layers = 1,
        };
        CheckVk(
            vkCreateFramebuffer(
                m_device,
                &framebufferInfo,
                nullptr,
                &m_swapchainFramebuffers[index]
            ),
            "vkCreateFramebuffer"
        );
    }
}

void VulkanRenderer::CreateTargets(std::uint32_t renderWidth, std::uint32_t renderHeight)
{
    m_renderTarget = CreateImage2D(
        m_physicalDevice,
        m_device,
        renderWidth,
        renderHeight,
        kRenderTargetFormat,
        VK_IMAGE_USAGE_STORAGE_BIT
    );
    m_presentTarget = CreateImage2D(
        m_physicalDevice,
        m_device,
        m_swapchainExtent.width,
        m_swapchainExtent.height,
        kPresentTargetFormat,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
    );
    m_renderWidth = renderWidth;
    m_renderHeight = renderHeight;
    m_renderTargetPrimed = false;
    m_presentTargetPrimed = false;
}

void VulkanRenderer::DestroySwapchain()
{
    for (VkFramebuffer framebuffer : m_swapchainFramebuffers)
    {
        if (framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }
    }
    m_swapchainFramebuffers.clear();

    for (VkImageView view : m_swapchainViews)
    {
        if (view != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_device, view, nullptr);
        }
    }
    m_swapchainViews.clear();

    DestroyImage(m_device, m_presentTarget);
    DestroyImage(m_device, m_renderTarget);

    if (m_swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }

    m_swapchainImages.clear();
    m_swapchainExtent = {};
    m_swapchainFormat = VK_FORMAT_UNDEFINED;
    m_renderWidth = 0;
    m_renderHeight = 0;
    m_renderTargetPrimed = false;
    m_presentTargetPrimed = false;
}

void VulkanRenderer::UpdateDescriptorSets()
{
    VkDescriptorImageInfo outputImageInfo = {
        .imageView = m_renderTarget.view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    VkDescriptorBufferInfo paramsInfo = {
        .buffer = m_paramsBuffer.buffer,
        .offset = 0,
        .range = sizeof(GpuFrameParams),
    };
    const std::array cloudWrites = {
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_descriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &outputImageInfo,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_descriptorSet,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &paramsInfo,
        },
    };
    vkUpdateDescriptorSets(
        m_device,
        static_cast<std::uint32_t>(cloudWrites.size()),
        cloudWrites.data(),
        0,
        nullptr
    );

    VkDescriptorImageInfo presentInputInfo = {
        .imageView = m_renderTarget.view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    VkDescriptorImageInfo presentOutputInfo = {
        .imageView = m_presentTarget.view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    VkDescriptorBufferInfo overlayInfo = {
        .buffer = m_overlayBuffer.buffer,
        .offset = 0,
        .range = kOverlayBufferBytes,
    };
    const std::array presentWrites = {
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_presentDescriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &presentInputInfo,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_presentDescriptorSet,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &presentOutputInfo,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_presentDescriptorSet,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &paramsInfo,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_presentDescriptorSet,
            .dstBinding = 3,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &overlayInfo,
        },
    };
    vkUpdateDescriptorSets(
        m_device,
        static_cast<std::uint32_t>(presentWrites.size()),
        presentWrites.data(),
        0,
        nullptr
    );
}

VkSurfaceFormatKHR
VulkanRenderer::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const
{
    for (const VkSurfaceFormatKHR& format : formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM)
        {
            return format;
        }
    }

    return formats.front();
}

VkPresentModeKHR VulkanRenderer::ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes
) const
{
    for (const VkPresentModeKHR mode : presentModes)
    {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

std::uint32_t VulkanRenderer::ChooseQueueFamily() const
{
    std::uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, families.data());

    for (std::uint32_t index = 0; index < queueFamilyCount; ++index)
    {
        VkBool32 presentSupported = VK_FALSE;
        CheckVk(
            vkGetPhysicalDeviceSurfaceSupportKHR(
                m_physicalDevice,
                index,
                m_surface,
                &presentSupported
            ),
            "vkGetPhysicalDeviceSurfaceSupportKHR"
        );

        const VkQueueFlags flags = families[index].queueFlags;
        const bool graphics = (flags & VK_QUEUE_GRAPHICS_BIT) != 0;
        const bool compute = (flags & VK_QUEUE_COMPUTE_BIT) != 0;
        if (graphics && compute && presentSupported == VK_TRUE)
        {
            return index;
        }
    }

    throw std::runtime_error("No compatible queue family found");
}
