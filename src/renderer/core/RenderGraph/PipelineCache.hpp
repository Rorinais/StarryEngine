#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>

class PipelineCache {
public:
    PipelineCache(VkDevice device);
    ~PipelineCache();

    bool initialize();
    void cleanup();

    VkPipeline getGraphicsPipeline(const std::string& name);
    VkPipeline getComputePipeline(const std::string& name);
    VkPipelineLayout getPipelineLayout(const std::string& name);

    bool registerGraphicsPipeline(const std::string& name, const VkGraphicsPipelineCreateInfo& createInfo);
    bool registerComputePipeline(const std::string& name, const VkComputePipelineCreateInfo& createInfo);

private:
    VkDevice mDevice;
    VkPipelineCache mPipelineCache = VK_NULL_HANDLE;

    std::unordered_map<std::string, VkPipeline> mGraphicsPipelines;
    std::unordered_map<std::string, VkPipeline> mComputePipelines;
    std::unordered_map<std::string, VkPipelineLayout> mPipelineLayouts;
};