#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include <vulkan/vulkan.h>

struct BufferResource
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void* mapped = nullptr;
};

struct ImageResource
{
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

void CheckVk(VkResult result, std::string_view context);
std::vector<std::byte> ReadBinaryFile(std::string_view path);
std::uint32_t FindMemoryType(
    VkPhysicalDevice physicalDevice,
    std::uint32_t typeBits,
    VkMemoryPropertyFlags properties
);

BufferResource CreateBuffer(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    bool mapMemory
);
void DestroyBuffer(VkDevice device, BufferResource& buffer);

ImageResource CreateImage2D(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    std::uint32_t width,
    std::uint32_t height,
    VkFormat format,
    VkImageUsageFlags usage
);
void DestroyImage(VkDevice device, ImageResource& image);
