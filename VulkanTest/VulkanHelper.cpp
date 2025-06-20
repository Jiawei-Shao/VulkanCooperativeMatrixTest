#include "VulkanHelper.h"

#include <fstream>
#include <sstream>

namespace {
    std::string FormatDriverVersion(uint32_t vendorID, uint32_t driverVersion) {
        std::ostringstream stream;
        stream << driverVersion << "(";

        if (vendorID == 0x8086) {
            stream << (driverVersion >> 14) << "." << (driverVersion & 0x3FFF);
        }
        else if (vendorID == 0x10DE) {
            stream << (driverVersion >> 22) << "." << (driverVersion >> 14 & 0xFF)
                << "." << (driverVersion >> 6 & 0xFF) << "." << (driverVersion & 0x3F);
        }

        stream << ")";
        return stream.str();
    }

    const char* GetCooperativeMatrixTypeString(VkComponentTypeKHR componentType) {
        switch (componentType) {
            case VK_COMPONENT_TYPE_FLOAT16_KHR:
                return "f16";
            case VK_COMPONENT_TYPE_FLOAT32_KHR:
                return "f32";
            case VK_COMPONENT_TYPE_SINT8_KHR:
                return "s8";
            case VK_COMPONENT_TYPE_UINT8_KHR:
                return "u8";
            case VK_COMPONENT_TYPE_UINT32_KHR:
                return "u32";
            case VK_COMPONENT_TYPE_SINT32_KHR:
                return "s32";
            default:
                return "";
        }
    }
}  // anonymous namespace

// Helper Functions

int32_t FindMemoryTypeIndex(
    const VkPhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties,
    const VkMemoryRequirements memoryRequirements,
    VkMemoryPropertyFlags requiredMemoryProperties) {
    for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < physicalDeviceMemoryProperties.memoryTypeCount; ++memoryTypeIndex) {
        if (!((1 << memoryTypeIndex) & memoryRequirements.memoryTypeBits)) {
            continue;
        }
        if (physicalDeviceMemoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & requiredMemoryProperties) {
            return static_cast<int32_t>(memoryTypeIndex);
        }
    }
    printf("Failed to find a proper memory type index.\n");
    return -1;
}

void RecordSetImageLayout(
    VkCommandBuffer commandBuffer,
    VkImage image,
    VkPipelineStageFlagBits srcStageMask,
    VkPipelineStageFlagBits dstStageMask,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkImageAspectFlags aspectMask,
    uint32_t baseArrayLayer,
    uint32_t layerCount,
    uint32_t baseMipLevel,
    uint32_t levelCount) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
    barrier.subresourceRange.layerCount = layerCount;
    barrier.subresourceRange.baseMipLevel = baseMipLevel;
    barrier.subresourceRange.levelCount = levelCount;
    barrier.srcAccessMask = 0;

    vkCmdPipelineBarrier(
        commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void RecordBufferBarrier(
    VkCommandBuffer commandBuffer,
    VkBuffer buffer,
    VkPipelineStageFlagBits srcStage,
    VkPipelineStageFlagBits dstStage,
    VkAccessFlags srcAccessMask,
    VkAccessFlags dstAccessMask,
    VkDeviceSize size) {
    VkBufferMemoryBarrier bufferMemoryBarrier = {};
    bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferMemoryBarrier.pNext = nullptr;
    bufferMemoryBarrier.srcAccessMask = srcAccessMask;
    bufferMemoryBarrier.dstAccessMask = dstAccessMask;
    bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferMemoryBarrier.buffer = buffer;
    bufferMemoryBarrier.offset = 0;
    bufferMemoryBarrier.size = size;
    vkCmdPipelineBarrier(
        commandBuffer, srcStage, dstStage, 0, 0, nullptr, 1, &bufferMemoryBarrier, 0, nullptr);
}

void PrintCooperativeMatrixProperty(VkCooperativeMatrixPropertiesKHR property) {
    printf("AType: %s BType: %s CType: %s M: %d N: %d K: %d\n",
        GetCooperativeMatrixTypeString(property.AType),
        GetCooperativeMatrixTypeString(property.BType),
        GetCooperativeMatrixTypeString(property.CType),
        property.MSize, property.NSize, property.KSize);
}

// VulkanBuffer

VulkanBuffer::VulkanBuffer(
    const VulkanRuntime& vulkanRuntime,
    VkDeviceSize size,
    VkBufferUsageFlags usageBits,
    VkMemoryPropertyFlags memoryFlagBits)
    : mDevice(vulkanRuntime.GetLogicalDevice()) {
    VkBufferCreateInfo bufferDesc = {};
    bufferDesc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferDesc.usage = usageBits;
    bufferDesc.size = size;
    bufferDesc.flags = 0;
    bufferDesc.pNext = nullptr;
    bufferDesc.queueFamilyIndexCount = 0;
    bufferDesc.pQueueFamilyIndices = nullptr;
    bufferDesc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK_RESULT(vkCreateBuffer(mDevice, &bufferDesc, nullptr, &mBuffer));

    VkMemoryRequirements bufferMemoryRequirements = {};
    vkGetBufferMemoryRequirements(mDevice, mBuffer, &bufferMemoryRequirements);
    VkMemoryAllocateInfo bufferMemoryAllocateInfo = {};
    bufferMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    bufferMemoryAllocateInfo.pNext = nullptr;
    bufferMemoryAllocateInfo.allocationSize = bufferMemoryRequirements.size;
    bufferMemoryAllocateInfo.memoryTypeIndex = vulkanRuntime.GetMemoryType(
        bufferMemoryRequirements.memoryTypeBits, memoryFlagBits);
    VK_CHECK_RESULT(vkAllocateMemory(mDevice, &bufferMemoryAllocateInfo, nullptr, &mMemory));

    vkBindBufferMemory(mDevice, mBuffer, mMemory, 0);

    mSize = bufferMemoryAllocateInfo.allocationSize;
}

VulkanBuffer::~VulkanBuffer() {
    if (mBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(mDevice, mBuffer, nullptr);
    }
    if (mMemory != VK_NULL_HANDLE) {
        vkFreeMemory(mDevice, mMemory, nullptr);
    }
}

VkBuffer VulkanBuffer::GetVkBuffer() const {
    return mBuffer;
}

VkDeviceMemory VulkanBuffer::GetVkDeviceMemory() const {
    return mMemory;
}

VkDeviceSize VulkanBuffer::GetSize() const {
    return mSize;
}

// VulkanSwapchain

VulkanSwapchain::VulkanSwapchain(
    const VulkanRuntime& vulkanRuntime, VulkanSwapchain* oldSwapchain)
  : mLogicalDevice(vulkanRuntime.GetLogicalDevice()),
    mQueue(vulkanRuntime.GetQueue()),
    mCurrentSwapchainIndex(0) {
    VkPhysicalDevice physicalDevice = vulkanRuntime.GetPhysicalDevice();
    VkSurfaceKHR surface = vulkanRuntime.GetSurface();

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities));
    uint32_t presentModeCount;
    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr));
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data()));
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (VkPresentModeKHR mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = mode;
            break;
        }
        if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            presentMode = mode;
        }
    }

    uint32_t swapchainImageCount = surfaceCapabilities.minImageCount + 1;
    VkSurfaceTransformFlagBitsKHR preTransformFlags = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (auto& compositeAlphaFlag : compositeAlphaFlags) {
        if (surfaceCapabilities.supportedCompositeAlpha & compositeAlphaFlag) {
            compositeAlpha = compositeAlphaFlag;
            break;
        };
    }

    uint32_t surfaceFormatCounts;
    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCounts, nullptr));
    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCounts);
    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCounts, surfaceFormats.data()));
    mSwapchainImageFormat = surfaceFormats[0].format;
    VkColorSpaceKHR swapchainColorSpace = surfaceFormats[0].colorSpace;
    for (VkSurfaceFormatKHR surfaceFormat : surfaceFormats) {
        if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
            surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            mSwapchainImageFormat = surfaceFormat.format;
            swapchainColorSpace = surfaceFormat.colorSpace;
            break;
        }
    }

    mSwapchainExtent = surfaceCapabilities.currentExtent;

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.pNext = nullptr;
    swapchainCreateInfo.surface = surface;
    swapchainCreateInfo.minImageCount = swapchainImageCount;
    swapchainCreateInfo.imageFormat = mSwapchainImageFormat;
    swapchainCreateInfo.imageColorSpace = swapchainColorSpace;
    swapchainCreateInfo.imageExtent = mSwapchainExtent;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.preTransform = preTransformFlags;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.queueFamilyIndexCount = 0;
    swapchainCreateInfo.pQueueFamilyIndices = nullptr;
    swapchainCreateInfo.presentMode = presentMode;
    swapchainCreateInfo.oldSwapchain = (oldSwapchain != nullptr) ? oldSwapchain->mSwapchain : VK_NULL_HANDLE;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.compositeAlpha = compositeAlpha;
    if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    VK_CHECK_RESULT(
        vkCreateSwapchainKHR(mLogicalDevice, &swapchainCreateInfo, nullptr, &mSwapchain));

    uint32_t imageCount;
    VK_CHECK_RESULT(vkGetSwapchainImagesKHR(mLogicalDevice, mSwapchain, &imageCount, nullptr));
    mSwapchainImages.resize(imageCount);
    VK_CHECK_RESULT(vkGetSwapchainImagesKHR(mLogicalDevice, mSwapchain, &imageCount, mSwapchainImages.data()));
    mSwapchainImageViews.resize(imageCount);

    VkCommandBuffer commandBuffer = vulkanRuntime.CreateAndBeginCommandBuffer();
    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo swapchainViewInfo = {};
        swapchainViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        swapchainViewInfo.pNext = nullptr;
        swapchainViewInfo.image = mSwapchainImages[i];
        swapchainViewInfo.format = mSwapchainImageFormat;
        swapchainViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        swapchainViewInfo.components = {
            VK_COMPONENT_SWIZZLE_R,
            VK_COMPONENT_SWIZZLE_G,
            VK_COMPONENT_SWIZZLE_B,
            VK_COMPONENT_SWIZZLE_A
        };
        swapchainViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        swapchainViewInfo.subresourceRange.baseMipLevel = 0;
        swapchainViewInfo.subresourceRange.levelCount = 1;
        swapchainViewInfo.subresourceRange.baseArrayLayer = 0;
        swapchainViewInfo.subresourceRange.layerCount = 1;
        swapchainViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        swapchainViewInfo.flags = 0;
        VK_CHECK_RESULT(
            vkCreateImageView(mLogicalDevice, &swapchainViewInfo, nullptr, &mSwapchainImageViews[i]));

        RecordSetImageLayout(
            commandBuffer, mSwapchainImages[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }

    vulkanRuntime.EndAndFreeCommandBuffer(commandBuffer);


    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = nullptr;
    semaphoreCreateInfo.flags = 0;
    VK_CHECK_RESULT(
        vkCreateSemaphore(mLogicalDevice, &semaphoreCreateInfo, nullptr, &mPresentCompleteSemaphore));
}

VulkanSwapchain::~VulkanSwapchain() {
    vkQueueWaitIdle(mQueue);

    vkDestroySemaphore(mLogicalDevice, mPresentCompleteSemaphore, nullptr);
    for (VkImageView swapchainImageView : mSwapchainImageViews) {
        vkDestroyImageView(mLogicalDevice, swapchainImageView, nullptr);
    }

    vkDestroySwapchainKHR(mLogicalDevice, mSwapchain, nullptr);
}

VkResult VulkanSwapchain::AcquireNextSwapchainImage(uint32_t* currentSwapchainIndex) {
    VkResult result = vkAcquireNextImageKHR(
        mLogicalDevice, mSwapchain, UINT64_MAX, mPresentCompleteSemaphore,
        VK_NULL_HANDLE, &mCurrentSwapchainIndex);

    *currentSwapchainIndex = mCurrentSwapchainIndex;
    return result;
}

VkResult VulkanSwapchain::Present(VkSemaphore renderCompleteSemaphore) {
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &mSwapchain;
    presentInfo.pImageIndices = &mCurrentSwapchainIndex;
    presentInfo.pWaitSemaphores = &renderCompleteSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    return vkQueuePresentKHR(mQueue, &presentInfo);
}

VkSemaphore VulkanSwapchain::GetPresentCompleteSemaphore() const {
    return mPresentCompleteSemaphore;
}

uint32_t VulkanSwapchain::GetCurrentSwapchainIndex() const {
    return mCurrentSwapchainIndex;
}

VkSwapchainKHR VulkanSwapchain::GetVkSwapchain() const {
    return mSwapchain;
}

VkExtent2D VulkanSwapchain::GetSwapchainExtent() const {
    return mSwapchainExtent;
}

const std::vector<VkImage>& VulkanSwapchain::GetSwapchainImages() const {
    return mSwapchainImages;
}

const std::vector<VkImageView>& VulkanSwapchain::GetSwapchainImageViews() const {
    return mSwapchainImageViews;
}

VkFormat VulkanSwapchain::GetSwapchainFormat() const {
    return mSwapchainImageFormat;
}

// VulkanRuntime

VulkanRuntime::VulkanRuntime(HWND hwnd) : mHwnd(hwnd) {
    const char* kAppName = "Vulkan Application";
    constexpr uint32_t kAPIVersion = VK_API_VERSION_1_3;

    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = kAppName;
    applicationInfo.pEngineName = kAppName;
    applicationInfo.apiVersion = kAPIVersion;

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    const char* instanceExtensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    };
    instanceCreateInfo.enabledExtensionCount = ARRAYSIZE(instanceExtensions);
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions;
    if (vkCreateInstance(&instanceCreateInfo, nullptr, &mInstance) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance!" << std::endl
            << "Minimum Vulkan version required: 1.3" << std::endl
            << "Required instance extensions: " << std::endl;
        for (const char* requiredExtensionName : instanceExtensions) {
            std::cerr << "  " << requiredExtensionName << std::endl;
        }
        exit(1);
    }

    uint32_t gpuCount = 0;
    VK_CHECK_RESULT(vkEnumeratePhysicalDevices(mInstance, &gpuCount, nullptr));
    assert(gpuCount > 0);

    std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
    VK_CHECK_RESULT(vkEnumeratePhysicalDevices(mInstance, &gpuCount, physicalDevices.data()));
    VkPhysicalDevice chosenGPU = VK_NULL_HANDLE;
    for (VkPhysicalDevice physicalDevice : physicalDevices) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data());
    }
    if (chosenGPU == VK_NULL_HANDLE) {
        for (VkPhysicalDevice physicalDevice : physicalDevices) {
            VkPhysicalDeviceProperties physicalDeviceProperties;
            vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
            if (physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                chosenGPU = physicalDevice;
                break;
            }
            if (physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU &&
                physicalDeviceProperties.vendorID == 0x8086) {
                chosenGPU = physicalDevice;
            }
        }
    }  
    if (chosenGPU == VK_NULL_HANDLE) {
        std::cerr << "Cannot find Vulkan physical device!" << std::endl;
        exit(1);
    }
    mPhysicalDevice = chosenGPU;
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mPhysicalDeviceMemoryProperties);

    mPhysicalDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    vkGetPhysicalDeviceProperties2(mPhysicalDevice, &mPhysicalDeviceProperties2);

    std::cout << GetDeviceInfo() << std::endl;

    VkWin32SurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = hwnd;
    createInfo.hinstance = GetModuleHandle(nullptr);
    VK_CHECK_RESULT(vkCreateWin32SurfaceKHR(mInstance, &createInfo, nullptr, &mSurface));

    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, nullptr);
    assert(queueFamilyCount > 0);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, queueFamilyProperties.data());
    std::vector<VkBool32> supportsPresent(queueFamilyCount);
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice, i, mSurface, &supportsPresent[i]);
    }
    uint32_t queueFamilyIndex;
    for (queueFamilyIndex = 0; queueFamilyIndex < static_cast<uint32_t>(queueFamilyProperties.size()); queueFamilyIndex++) {
        if ((queueFamilyProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
            supportsPresent[queueFamilyIndex]) {
            break;
        }
    }
    const float kQueuePriority = 0.f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &kQueuePriority;

    VkPhysicalDeviceCooperativeMatrixFeaturesKHR coopMatFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR,
        NULL,
        VK_TRUE, // cooperativeMatrix
        VK_FALSE, // cooperativeMatrixRobustBufferAccess
    };

    VkPhysicalDeviceVulkan11Features vulkan11Features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, &coopMatFeatures };
    vulkan11Features.storageBuffer16BitAccess = VK_TRUE;

    VkPhysicalDeviceVulkan12Features vulkan12Features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, &vulkan11Features };
    vulkan12Features.bufferDeviceAddress = VK_TRUE;
    vulkan12Features.shaderFloat16 = VK_TRUE;
    vulkan12Features.shaderInt8 = VK_TRUE;
    vulkan12Features.vulkanMemoryModel = VK_TRUE;
    vulkan12Features.vulkanMemoryModelDeviceScope = VK_TRUE;
    vulkan12Features.storageBuffer8BitAccess = VK_TRUE;

    VkPhysicalDeviceVulkan13Features vulkan13Features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, &vulkan12Features };
    vulkan13Features.maintenance4 = VK_TRUE;

    std::vector<const char*> requiredDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME,
    };

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();
    deviceCreateInfo.pNext = &vulkan13Features;
    if (vkCreateDevice(mPhysicalDevice, &deviceCreateInfo, nullptr, &mLogicalDevice) != VK_SUCCESS) {
        std::cerr << "Failed to create VkDevice!" << std::endl;
        exit(1);
    }

    VkCommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK_RESULT(
        vkCreateCommandPool(mLogicalDevice, &commandPoolCreateInfo, nullptr, &mCommandPool));

    vkGetDeviceQueue(mLogicalDevice, queueFamilyIndex, 0, &mQueue);

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = nullptr;
    semaphoreCreateInfo.flags = 0;
    VK_CHECK_RESULT(
        vkCreateSemaphore(mLogicalDevice, &semaphoreCreateInfo, nullptr, &mRenderCompleteSemaphore));
    // pCommandbuffers and pWaitSemaphores are not set until we want to call vkQueueSubmit().
    mSubmitInfo = {};
    mSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    mSubmitInfo.pWaitDstStageMask = &kSubmitPipelineStages;
    mSubmitInfo.waitSemaphoreCount = 1;
    mSubmitInfo.signalSemaphoreCount = 1;
    mSubmitInfo.pSignalSemaphores = &mRenderCompleteSemaphore;
}

VulkanSwapchain VulkanRuntime::RecreateSwapchain(VulkanSwapchain* oldSwapchain) {
    return VulkanSwapchain(*this, oldSwapchain);
}

VkPipelineShaderStageCreateInfo VulkanRuntime::LoadShader(const char* fileName, VkShaderStageFlagBits stage) {
    std::ifstream is(fileName, std::ios::binary | std::ios::in | std::ios::ate);

    VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
    shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfo.stage = stage;
    shaderStageCreateInfo.pName = "main";

    if (is.is_open())
    {
        size_t size = is.tellg();
        is.seekg(0, std::ios::beg);
        std::vector<char> shaderCode(size);
        is.read(shaderCode.data(), size);
        is.close();

        assert(size > 0);

        VkShaderModule shaderModule;
        VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
        shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleCreateInfo.codeSize = size;
        shaderModuleCreateInfo.pCode = reinterpret_cast<uint32_t*>(shaderCode.data());

        VK_CHECK_RESULT(vkCreateShaderModule(mLogicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule));
        shaderStageCreateInfo.module = shaderModule;
    }
    else
    {
        std::cerr << "Error: Could not open shader file \"" << fileName << "\"" << "\n";
    }

    return shaderStageCreateInfo;
}

VkRenderPass VulkanRuntime::CreateRenderPass(VkFormat colorFormat) const {
    VkAttachmentDescription attachment = {};
    attachment.format = colorFormat;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorReference = {};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;
    subpassDescription.pDepthStencilAttachment = nullptr;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;
    subpassDescription.pResolveAttachments = nullptr;

    VkSubpassDependency dependencies[2];
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &attachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDescription;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = dependencies;

    VkRenderPass renderPass;
    VK_CHECK_RESULT(vkCreateRenderPass(mLogicalDevice, &renderPassInfo, nullptr, &renderPass));
    return renderPass;
}


uint32_t VulkanRuntime::GetMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags memoryPropertyFlags) const {
    bool foundMemoryType = false;
    for (uint32_t i = 0; i < mPhysicalDeviceMemoryProperties.memoryTypeCount; ++i) {
        if ((memoryTypeBits & 1) == 1) {
            if ((mPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags) {
                foundMemoryType = true;
                return i;
            }
        }
        memoryTypeBits >>= 1;
    }
    assert(foundMemoryType);
    return -1;
}

std::string VulkanRuntime::GetDeviceInfo() const {
    std::ostringstream stream;
    stream << mPhysicalDeviceProperties2.properties.deviceName << " ("
        << "DeviceID: 0x" << std::hex << std::uppercase << mPhysicalDeviceProperties2.properties.deviceID << " "
        << "VendorID: 0x" << std::hex << std::uppercase << mPhysicalDeviceProperties2.properties.vendorID << "\n"
        << "Driver version: " 
        << FormatDriverVersion(mPhysicalDeviceProperties2.properties.vendorID, 
                               mPhysicalDeviceProperties2.properties.driverVersion) << "\n"
        << "is_discrete_gpu: " << std::boolalpha 
        << (mPhysicalDeviceProperties2.properties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) << "\n";
    return stream.str();
}

VkCommandBuffer VulkanRuntime::CreateAndBeginCommandBuffer() const {
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = mCommandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer;
    VK_CHECK_RESULT(vkAllocateCommandBuffers(mLogicalDevice, &commandBufferAllocateInfo, &commandBuffer));

    VkCommandBufferBeginInfo commandBufferBeginInfo = {};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

    return commandBuffer;
}

void VulkanRuntime::EndAndFreeCommandBuffer(VkCommandBuffer commandBuffer) const {
    vkEndCommandBuffer(commandBuffer);
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;
    VK_CHECK_RESULT(vkCreateFence(mLogicalDevice, &fenceInfo, nullptr, &fence));
    VK_CHECK_RESULT(vkQueueSubmit(mQueue, 1, &submitInfo, fence));
    constexpr uint64_t kFenceTimeoutInNS = 0xFFFFFFFFFFFFFFFF;
    VK_CHECK_RESULT(vkWaitForFences(mLogicalDevice, 1, &fence, VK_TRUE, kFenceTimeoutInNS));
    vkDestroyFence(mLogicalDevice, fence, nullptr);

    FreeCommandBuffer(commandBuffer);
}

void VulkanRuntime::FreeCommandBuffer(VkCommandBuffer commandBuffer) const {
    vkFreeCommandBuffers(mLogicalDevice, mCommandPool, 1, &commandBuffer);
}

void VulkanRuntime::QueueSubmit(const std::vector<VkCommandBuffer>& commandBuffers, VkSemaphore waitSemaphore) {
    mSubmitInfo.pWaitSemaphores = &waitSemaphore;
    mSubmitInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
    mSubmitInfo.pCommandBuffers = commandBuffers.data();

    VK_CHECK_RESULT(vkQueueSubmit(mQueue, 1, &mSubmitInfo, VK_NULL_HANDLE));
}

VkDevice VulkanRuntime::GetLogicalDevice() const {
    return mLogicalDevice;
}

VkQueue VulkanRuntime::GetQueue() const {
    return mQueue;
}

VkPhysicalDevice VulkanRuntime::GetPhysicalDevice() const {
    return mPhysicalDevice;
}

VkSurfaceKHR VulkanRuntime::GetSurface() const {
    return mSurface;
}

VkSemaphore VulkanRuntime::GetRenderCompleteSemaphore() const {
    return mRenderCompleteSemaphore;
}

std::vector<VkCooperativeMatrixPropertiesKHR>
VulkanRuntime::GetCooperativeMatrixProperties() const {
    uint32_t configCount = 0;
    PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR>(
            vkGetInstanceProcAddr(mInstance, "vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR"));
    vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR(mPhysicalDevice, &configCount, nullptr);

    if (configCount == 0) {
        return {};
    }
    VkCooperativeMatrixPropertiesKHR property = {};
    property.sType = VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_KHR;
    std::vector<VkCooperativeMatrixPropertiesKHR> cooperativeMatrixProperties(configCount, property);
    vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR(
        mPhysicalDevice, &configCount, cooperativeMatrixProperties.data());
    return cooperativeMatrixProperties;
}

VulkanBuffer VulkanRuntime::CreateBuffer(
    VkDeviceSize size, VkBufferUsageFlags usageBits,
    VkMemoryPropertyFlags memoryFlagBits) {
    return VulkanBuffer(*this, size, usageBits, memoryFlagBits);
}