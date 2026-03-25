#include "vulkan_renderer.h"

#include <array>
#include <cstring>
#include <stdexcept>

#include <backends/imgui_impl_vulkan.h>

namespace
{
constexpr std::uint32_t kCloudWorkgroupSize = 8;
constexpr std::uint32_t kPresentWorkgroupSize = 16;
constexpr std::size_t kOverlayBufferBytes = sizeof(std::uint32_t) * kOverlayPixelCount;

void TransitionImage(
    VkCommandBuffer commandBuffer,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkAccessFlags srcAccessMask,
    VkAccessFlags dstAccessMask,
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask
)
{
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask,
        dstStageMask,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );
}
} // namespace

void VulkanRenderer::Render(
    const GpuFrameParams& params,
    std::span<const std::uint32_t> overlayPixels,
    ImDrawData* drawData
)
{
    if (m_swapchain == VK_NULL_HANDLE || m_renderTarget.image == VK_NULL_HANDLE)
    {
        return;
    }

    if (overlayPixels.size_bytes() != kOverlayBufferBytes)
    {
        throw std::runtime_error("Overlay buffer size mismatch");
    }

    std::memcpy(m_paramsBuffer.mapped, &params, sizeof(params));
    std::memcpy(m_overlayBuffer.mapped, overlayPixels.data(), kOverlayBufferBytes);

    CheckVk(vkWaitForFences(m_device, 1, &m_frameFence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
    CheckVk(vkResetFences(m_device, 1, &m_frameFence), "vkResetFences");

    std::uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        m_device,
        m_swapchain,
        UINT64_MAX,
        m_imageAvailableSemaphore,
        VK_NULL_HANDLE,
        &imageIndex
    );
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return;
    }
    CheckVk(acquireResult, "vkAcquireNextImageKHR");

    CheckVk(vkResetCommandBuffer(m_commandBuffer, 0), "vkResetCommandBuffer");
    RecordCommandBuffer(imageIndex, drawData);

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &m_imageAvailableSemaphore,
        .pWaitDstStageMask = &waitStage,
        .commandBufferCount = 1,
        .pCommandBuffers = &m_commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &m_renderFinishedSemaphore,
    };
    CheckVk(vkQueueSubmit(m_queue, 1, &submitInfo, m_frameFence), "vkQueueSubmit");

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &m_renderFinishedSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,
        .pImageIndices = &imageIndex,
    };
    const VkResult presentResult = vkQueuePresentKHR(m_queue, &presentInfo);
    if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR &&
        presentResult != VK_ERROR_OUT_OF_DATE_KHR)
    {
        CheckVk(presentResult, "vkQueuePresentKHR");
    }
}

void VulkanRenderer::RecordCommandBuffer(std::uint32_t swapchainImageIndex, ImDrawData* drawData)
{
    const std::uint32_t cloudGroupsX =
        (m_renderWidth + kCloudWorkgroupSize - 1) / kCloudWorkgroupSize;
    const std::uint32_t cloudGroupsY =
        (m_renderHeight + kCloudWorkgroupSize - 1) / kCloudWorkgroupSize;
    const std::uint32_t presentGroupsX =
        (m_swapchainExtent.width + kPresentWorkgroupSize - 1) / kPresentWorkgroupSize;
    const std::uint32_t presentGroupsY =
        (m_swapchainExtent.height + kPresentWorkgroupSize - 1) / kPresentWorkgroupSize;

    VkCommandBufferBeginInfo beginInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    CheckVk(vkBeginCommandBuffer(m_commandBuffer, &beginInfo), "vkBeginCommandBuffer");

    TransitionImage(
        m_commandBuffer,
        m_renderTarget.image,
        m_renderTargetPrimed ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        0,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
    );
    TransitionImage(
        m_commandBuffer,
        m_presentTarget.image,
        m_presentTargetPrimed ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        0,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
    );
    TransitionImage(
        m_commandBuffer,
        m_swapchainImages[swapchainImageIndex],
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT
    );

    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    vkCmdBindDescriptorSets(
        m_commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_pipelineLayout,
        0,
        1,
        &m_descriptorSet,
        0,
        nullptr
    );
    vkCmdDispatch(m_commandBuffer, cloudGroupsX, cloudGroupsY, 1);

    TransitionImage(
        m_commandBuffer,
        m_renderTarget.image,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
    );

    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_presentPipeline);
    vkCmdBindDescriptorSets(
        m_commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_presentPipelineLayout,
        0,
        1,
        &m_presentDescriptorSet,
        0,
        nullptr
    );
    vkCmdDispatch(m_commandBuffer, presentGroupsX, presentGroupsY, 1);

    TransitionImage(
        m_commandBuffer,
        m_presentTarget.image,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT
    );

    VkImageBlit blit = {};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[1] = {
        static_cast<int32_t>(m_swapchainExtent.width),
        static_cast<int32_t>(m_swapchainExtent.height),
        1,
    };
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.layerCount = 1;
    blit.dstOffsets[1] = {
        static_cast<int32_t>(m_swapchainExtent.width),
        static_cast<int32_t>(m_swapchainExtent.height),
        1,
    };

    vkCmdBlitImage(
        m_commandBuffer,
        m_presentTarget.image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        m_swapchainImages[swapchainImageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &blit,
        VK_FILTER_NEAREST
    );

    TransitionImage(
        m_commandBuffer,
        m_swapchainImages[swapchainImageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    );

    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = m_imguiRenderPass,
        .framebuffer = m_swapchainFramebuffers[swapchainImageIndex],
        .renderArea = {{0, 0}, m_swapchainExtent},
    };
    vkCmdBeginRenderPass(m_commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    if (drawData != nullptr && drawData->DisplaySize.x > 0.0f && drawData->DisplaySize.y > 0.0f)
    {
        ImGui_ImplVulkan_RenderDrawData(drawData, m_commandBuffer);
    }
    vkCmdEndRenderPass(m_commandBuffer);

    CheckVk(vkEndCommandBuffer(m_commandBuffer), "vkEndCommandBuffer");
    m_renderTargetPrimed = true;
    m_presentTargetPrimed = true;
}
