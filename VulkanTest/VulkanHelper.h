#pragma once

#ifndef VULKAN_HELPER_H_
#define VULKAN_HELPER_H_

#include <assert.h>
#include <iostream>
#include <string>
#include <vector>

#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/vulkan.h"

#define VK_CHECK_RESULT(f)                                                                                       \
{                                                                                                                \
    VkResult res = (f);                                                                                          \
    if (res != VK_SUCCESS)                                                                                       \
    {                                                                                                            \
        std::cerr << "Fatal : VkResult is \"" << res << "\" in " << __FILE__<< " at line " << __LINE__ << ": "   \
            << #f << std::endl;                                                                                  \
        assert(res == VK_SUCCESS);                                                                               \
    }                                                                                                            \
}

int32_t FindMemoryTypeIndex(
    const VkPhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties,
    const VkMemoryRequirements memoryRequirements,
    VkMemoryPropertyFlags requiredMemoryProperties);

void RecordSetImageLayout(
    VkCommandBuffer commandBuffer,
    VkImage image,
    VkPipelineStageFlagBits srcStageMask,
    VkPipelineStageFlagBits dstStageMask,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    uint32_t baseArrayLayer = 0,
    uint32_t layerCount = 1,
    uint32_t baseMipLevel = 0,
    uint32_t levelCount = 1);

void RecordBufferBarrier(
    VkCommandBuffer commandBuffer,
    VkBuffer buffer,
    VkPipelineStageFlagBits srcStage,
    VkPipelineStageFlagBits dstStage,
    VkAccessFlags srcAccessMask,
    VkAccessFlags dstAccessMask,
    VkDeviceSize size);

void PrintCooperativeMatrixProperty(VkCooperativeMatrixPropertiesKHR property);

class VulkanRuntime;

class VulkanBuffer {
  public:
    ~VulkanBuffer();
    VkBuffer GetVkBuffer() const;
    VkDeviceMemory GetVkDeviceMemory() const;
    VkDeviceSize GetSize() const;

  private:
     friend VulkanRuntime;
     VulkanBuffer(
         const VulkanRuntime& vulkanRuntime,
         VkDeviceSize size,
         VkBufferUsageFlags usageBits,
         VkMemoryPropertyFlags memoryFlagBits);

    VkDevice mDevice;
    VkBuffer mBuffer = VK_NULL_HANDLE;
    VkDeviceMemory mMemory = VK_NULL_HANDLE;
    VkDeviceSize mSize = 0;
};

class VulkanSwapchain {
  public:
    VulkanSwapchain(const VulkanRuntime& vulkanRuntime, VulkanSwapchain* oldSwapchain);
    ~VulkanSwapchain();

    VkResult AcquireNextSwapchainImage(uint32_t* currentSwapchainIndex);
    VkResult Present(VkSemaphore renderCompleteSemaphore);

    VkSemaphore GetPresentCompleteSemaphore() const;
    uint32_t GetCurrentSwapchainIndex() const;
    VkSwapchainKHR GetVkSwapchain() const;
    VkExtent2D GetSwapchainExtent() const;

    const std::vector<VkImage>& GetSwapchainImages() const;
    const std::vector<VkImageView>& GetSwapchainImageViews() const;
    VkFormat GetSwapchainFormat() const;

  private:
    VkDevice mLogicalDevice;
    VkSwapchainKHR mSwapchain;
    VkQueue mQueue;

    VkSemaphore mPresentCompleteSemaphore;
    std::vector<VkImage> mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;

    VkFormat mSwapchainImageFormat;
    VkExtent2D mSwapchainExtent;
    uint32_t mCurrentSwapchainIndex;
};

class VulkanRuntime {
  public:
    explicit VulkanRuntime(HWND hwnd);
    VkDevice GetLogicalDevice() const;
    VkPhysicalDevice GetPhysicalDevice() const;
    VkSurfaceKHR GetSurface() const;

    VkRenderPass CreateRenderPass(VkFormat colorFormat) const;
    VkPipelineShaderStageCreateInfo LoadShader(const char* filename, VkShaderStageFlagBits stage);
    VkCommandBuffer CreateAndBeginCommandBuffer() const;
    void FreeCommandBuffer(VkCommandBuffer commandBuffer) const;
    void EndAndFreeCommandBuffer(VkCommandBuffer commandBuffer) const;
    void QueueSubmit(const std::vector<VkCommandBuffer>& commandBuffers, VkSemaphore waitSemaphore);

    VkSemaphore GetRenderCompleteSemaphore() const;
    VulkanSwapchain RecreateSwapchain(VulkanSwapchain* oldSwapchain);

    std::string GetDeviceInfo() const;

    VkQueue GetQueue() const;

    uint32_t GetMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags memoryPropertyFlags) const;

    std::vector<VkCooperativeMatrixPropertiesKHR> GetCooperativeMatrixProperties() const;

    VulkanBuffer CreateBuffer(
        VkDeviceSize size, VkBufferUsageFlags usageBits,
        VkMemoryPropertyFlags memoryFlagBits);

  private:
    VkInstance mInstance;
    VkPhysicalDevice mPhysicalDevice;
    VkPhysicalDeviceType mGPUType = VK_PHYSICAL_DEVICE_TYPE_OTHER;
    VkPhysicalDeviceProperties2 mPhysicalDeviceProperties2;
    VkPhysicalDeviceMemoryProperties mPhysicalDeviceMemoryProperties;

    VkDevice mLogicalDevice;
    VkCommandPool mCommandPool;

    VkSubmitInfo mSubmitInfo;
    VkQueue mQueue;
    VkSemaphore mRenderCompleteSemaphore;

    HWND mHwnd;
    VkSurfaceKHR mSurface;

    const VkPipelineStageFlags kSubmitPipelineStages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
};


#endif
