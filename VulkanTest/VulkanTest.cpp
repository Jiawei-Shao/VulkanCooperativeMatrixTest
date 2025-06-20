#include "VulkanHelper.h"
#include "Window.h"

#include <array>

int main() {
    HWND hwnd = CreateAppWindow();
    VulkanRuntime vulkanRuntime(hwnd);

    const auto& cooperativeMatrixProperties = vulkanRuntime.GetCooperativeMatrixProperties();
    VkCooperativeMatrixPropertiesKHR uint8Property = {};
    for (const auto& property : cooperativeMatrixProperties) {
        if (property.scope != VK_SCOPE_SUBGROUP_KHR) {
            continue;
        }
        PrintCooperativeMatrixProperty(property);
        if (uint8Property.KSize == 0 && property.AType == VK_COMPONENT_TYPE_UINT8_KHR) {
            uint8Property = property;
        }
    }
    if (uint8Property.KSize == 0) {
        printf("Error: no uint8 cooperative matrix supported\n");
        return 0;
    }
    printf("\nChoose config:\n");
    PrintCooperativeMatrixProperty(uint8Property);
    printf("\n");

    VkDevice device = vulkanRuntime.GetLogicalDevice();

    VkDeviceSize uploadBufferSize = static_cast<VkDeviceSize>(
        uint8Property.KSize * uint8Property.MSize + uint8Property.KSize * uint8Property.NSize);
    VulkanBuffer uploadBuffer = vulkanRuntime.CreateBuffer(
        uploadBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    VkDeviceSize inputBufferSize1 = uint8Property.KSize * uint8Property.MSize;
    VkDeviceSize inputBufferSize2 = uint8Property.KSize * uint8Property.NSize;
    VkDeviceSize outputBufferSize = uint8Property.MSize * uint8Property.NSize * 4;
    VkDeviceSize readbackBufferSize = outputBufferSize;
    VulkanBuffer inputBuffer1 = vulkanRuntime.CreateBuffer(
        inputBufferSize1, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VulkanBuffer inputBuffer2 = vulkanRuntime.CreateBuffer(
        inputBufferSize2, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VulkanBuffer outputBuffer = vulkanRuntime.CreateBuffer(
        outputBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VulkanBuffer readbackBuffer = vulkanRuntime.CreateBuffer(
        outputBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    VkDeviceMemory uploadMemory = uploadBuffer.GetVkDeviceMemory();
    void* uploadPtr;
    VK_CHECK_RESULT(vkMapMemory(device, uploadMemory, 0, VK_WHOLE_SIZE, 0, &uploadPtr));
    uint8_t* uploadUint32Ptr = static_cast<uint8_t*>(uploadPtr);
    memset(uploadPtr, 1u, uploadBufferSize);
    vkUnmapMemory(device, uploadMemory);

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.maxSets = 3;
    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 3;
    poolCreateInfo.poolSizeCount = 1;
    poolCreateInfo.pPoolSizes = &poolSize;
    VkDescriptorPool descriptorPool;
    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool));

    std::array<VkDescriptorSetLayoutBinding, 3> bindingDescs = {};
    bindingDescs[0].binding = 0;
    bindingDescs[0].descriptorCount = 1;
    bindingDescs[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindingDescs[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindingDescs[2] = bindingDescs[1] = bindingDescs[0];
    bindingDescs[1].binding = 1;
    bindingDescs[2].binding = 2;
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindingDescs.size());
    descriptorSetLayoutCreateInfo.pBindings = bindingDescs.data();
    VkDescriptorSetLayout descriptorSetLayout;
    VK_CHECK_RESULT(
        vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout));

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;
    VkDescriptorSet descriptorSet;
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

    std::array<VkWriteDescriptorSet, 3> writeDescriptorSets = {};
    writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSets[0].dstSet = descriptorSet;
    writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorSets[0].descriptorCount = 1;
    VkDescriptorBufferInfo bufferInfo1 = {};
    bufferInfo1.buffer = inputBuffer1.GetVkBuffer();
    bufferInfo1.offset = 0;
    bufferInfo1.range = VK_WHOLE_SIZE;
    writeDescriptorSets[0].dstBinding = 0;
    writeDescriptorSets[0].pBufferInfo = &bufferInfo1;
    VkDescriptorBufferInfo bufferInfo2 = {};
    bufferInfo2.buffer = inputBuffer2.GetVkBuffer();
    bufferInfo2.offset = 0;
    bufferInfo2.range = VK_WHOLE_SIZE;
    writeDescriptorSets[1] = writeDescriptorSets[0];
    writeDescriptorSets[1].dstBinding = 1;
    writeDescriptorSets[1].pBufferInfo = &bufferInfo2;
    VkDescriptorBufferInfo bufferInfo3 = {};
    bufferInfo3.buffer = outputBuffer.GetVkBuffer();
    bufferInfo3.offset = 0;
    bufferInfo3.range = VK_WHOLE_SIZE;
    writeDescriptorSets[2] = writeDescriptorSets[0];
    writeDescriptorSets[2].dstBinding = 2;
    writeDescriptorSets[2].pBufferInfo = &bufferInfo3;
    vkUpdateDescriptorSets(
        device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(),
        0, nullptr);

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

    uint32_t constantData[] = { uint8Property.MSize, uint8Property.NSize, uint8Property.KSize };
    VkPipelineShaderStageCreateInfo shaderStageCreateInfo =
        vulkanRuntime.LoadShader("Shaders/compute_nv.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    VkSpecializationMapEntry entry = {};
    VkSpecializationMapEntry entries[] = {
        {0, sizeof(uint32_t) * 0, sizeof(uint32_t)},
        {1, sizeof(uint32_t) * 1, sizeof(uint32_t)},
        {2, sizeof(uint32_t) * 2, sizeof(uint32_t)},
    };
    VkSpecializationInfo specInfo =
    {
        ARRAYSIZE(constantData),
        entries,
        sizeof(constantData),
        constantData,
    };
    VkComputePipelineCreateInfo computePipelineCreateInfo = {};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.stage = shaderStageCreateInfo;
    computePipelineCreateInfo.stage.pSpecializationInfo = &specInfo;
    computePipelineCreateInfo.layout = pipelineLayout;
    VkPipeline computePipeline;
    VK_CHECK_RESULT(vkCreateComputePipelines(
        device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &computePipeline));

    VkCommandBuffer commandBuffer = vulkanRuntime.CreateAndBeginCommandBuffer();
    VkBufferCopy bufferCopy = {};
    bufferCopy.dstOffset = 0;
    bufferCopy.srcOffset = 0;
    bufferCopy.size = inputBuffer1.GetSize();
    vkCmdCopyBuffer(
        commandBuffer, uploadBuffer.GetVkBuffer(), inputBuffer1.GetVkBuffer(), 1, &bufferCopy);
    bufferCopy.size = inputBuffer2.GetSize();
    vkCmdCopyBuffer(
        commandBuffer, uploadBuffer.GetVkBuffer(), inputBuffer2.GetVkBuffer(), 1, &bufferCopy);

    RecordBufferBarrier(
        commandBuffer, inputBuffer1.GetVkBuffer(), VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT, inputBuffer1.GetSize());
    RecordBufferBarrier(
        commandBuffer, inputBuffer2.GetVkBuffer(), VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT, inputBuffer2.GetSize());

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, 0);
    vkCmdDispatch(commandBuffer, 1, 1, 1);

    RecordBufferBarrier(
        commandBuffer, outputBuffer.GetVkBuffer(), VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT, outputBuffer.GetSize());

    bufferCopy.size = outputBuffer.GetSize();
    vkCmdCopyBuffer(commandBuffer, outputBuffer.GetVkBuffer(), readbackBuffer.GetVkBuffer(), 1, &bufferCopy);
    vulkanRuntime.EndAndFreeCommandBuffer(commandBuffer);

    VkDeviceMemory readbackMemory = readbackBuffer.GetVkDeviceMemory();
    void* readbackPtr;
    VK_CHECK_RESULT(vkMapMemory(device, readbackMemory, 0, VK_WHOLE_SIZE, 0, &readbackPtr));

    printf("Output data (column major): \n");

    uint32_t* result = static_cast<uint32_t*>(readbackPtr);
    for (uint32_t y = 0; y < uint8Property.MSize; ++y) {
        for (uint32_t x = 0; x < uint8Property.NSize; ++x) {
            uint32_t index = y * uint8Property.NSize + x;
            printf("%d ", result[index]);
        }
        printf("\n");
    }
    printf("\n");

    return 0;
}
