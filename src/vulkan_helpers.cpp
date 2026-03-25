#include "vulkan_helpers.h"

#include <fstream>
#include <stdexcept>

void CheckVk(VkResult result, std::string_view context)
{
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error(
            std::string(context) + " failed with VkResult " + std::to_string(result)
        );
    }
}

std::vector<std::byte> ReadBinaryFile(std::string_view path)
{
    std::ifstream file(path.data(), std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file: " + std::string(path));
    }

    const std::streamsize size = file.tellg();
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    return bytes;
}

std::uint32_t FindMemoryType(
    VkPhysicalDevice physicalDevice,
    std::uint32_t typeBits,
    VkMemoryPropertyFlags properties
)
{
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (std::uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index)
    {
        const bool typeMatches = (typeBits & (1u << index)) != 0u;
        const bool propertyMatches =
            (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties;
        if (typeMatches && propertyMatches)
        {
            return index;
        }
    }

    throw std::runtime_error("Failed to find compatible Vulkan memory type");
}

BufferResource CreateBuffer(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    bool mapMemory
)
{
    BufferResource resource;

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    CheckVk(vkCreateBuffer(device, &bufferInfo, nullptr, &resource.buffer), "vkCreateBuffer");

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device, resource.buffer, &requirements);

    VkMemoryAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = FindMemoryType(physicalDevice, requirements.memoryTypeBits, properties),
    };
    CheckVk(vkAllocateMemory(device, &allocateInfo, nullptr, &resource.memory), "vkAllocateMemory");
    CheckVk(vkBindBufferMemory(device, resource.buffer, resource.memory, 0), "vkBindBufferMemory");

    if (mapMemory)
    {
        CheckVk(vkMapMemory(device, resource.memory, 0, size, 0, &resource.mapped), "vkMapMemory");
    }

    return resource;
}

void DestroyBuffer(VkDevice device, BufferResource& buffer)
{
    if (buffer.mapped != nullptr)
    {
        vkUnmapMemory(device, buffer.memory);
        buffer.mapped = nullptr;
    }

    if (buffer.buffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, buffer.buffer, nullptr);
        buffer.buffer = VK_NULL_HANDLE;
    }

    if (buffer.memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, buffer.memory, nullptr);
        buffer.memory = VK_NULL_HANDLE;
    }
}

ImageResource CreateImage2D(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    std::uint32_t width,
    std::uint32_t height,
    VkFormat format,
    VkImageUsageFlags usage
)
{
    ImageResource resource;

    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    CheckVk(vkCreateImage(device, &imageInfo, nullptr, &resource.image), "vkCreateImage");

    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(device, resource.image, &requirements);

    VkMemoryAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = FindMemoryType(
            physicalDevice,
            requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        ),
    };
    CheckVk(vkAllocateMemory(device, &allocateInfo, nullptr, &resource.memory), "vkAllocateMemory");
    CheckVk(vkBindImageMemory(device, resource.image, resource.memory, 0), "vkBindImageMemory");

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = resource.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    CheckVk(vkCreateImageView(device, &viewInfo, nullptr, &resource.view), "vkCreateImageView");
    return resource;
}

void DestroyImage(VkDevice device, ImageResource& image)
{
    if (image.view != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, image.view, nullptr);
        image.view = VK_NULL_HANDLE;
    }
    if (image.image != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, image.image, nullptr);
        image.image = VK_NULL_HANDLE;
    }
    if (image.memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, image.memory, nullptr);
        image.memory = VK_NULL_HANDLE;
    }
}
