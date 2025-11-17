#include "PipelineCache.hpp"
#include <stdexcept>
#include <iostream>

namespace StarryEngine {

    PipelineCache::PipelineCache(VkDevice device) : mDevice(device) {
    }

    PipelineCache::~PipelineCache() {
        cleanup();
    }

    bool PipelineCache::initialize() {
        VkPipelineCacheCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

        VkResult result = vkCreatePipelineCache(mDevice, &createInfo, nullptr, &mPipelineCache);
        return result == VK_SUCCESS;
    }

    void PipelineCache::cleanup() {
        if (mPipelineCache != VK_NULL_HANDLE) {
            vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);
            mPipelineCache = VK_NULL_HANDLE;
        }

        // 清理所有管线
        for (auto& pipeline : mGraphicsPipelines) {
            vkDestroyPipeline(mDevice, pipeline.second, nullptr);
        }
        for (auto& pipeline : mComputePipelines) {
            vkDestroyPipeline(mDevice, pipeline.second, nullptr);
        }
        for (auto& layout : mPipelineLayouts) {
            vkDestroyPipelineLayout(mDevice, layout.second, nullptr);
        }

        mGraphicsPipelines.clear();
        mComputePipelines.clear();
        mPipelineLayouts.clear();
    }

    VkPipeline PipelineCache::getGraphicsPipeline(const std::string& name) {
        auto it = mGraphicsPipelines.find(name);
        return it != mGraphicsPipelines.end() ? it->second : VK_NULL_HANDLE;
    }

    VkPipeline PipelineCache::getComputePipeline(const std::string& name) {
        auto it = mComputePipelines.find(name);
        return it != mComputePipelines.end() ? it->second : VK_NULL_HANDLE;
    }

    VkPipelineLayout PipelineCache::getPipelineLayout(const std::string& name) {
        auto it = mPipelineLayouts.find(name);
        return it != mPipelineLayouts.end() ? it->second : VK_NULL_HANDLE;
    }

    bool PipelineCache::registerGraphicsPipeline(const std::string& name, const VkGraphicsPipelineCreateInfo& createInfo) {
        VkPipeline pipeline;
        VkResult result = vkCreateGraphicsPipelines(mDevice, mPipelineCache, 1, &createInfo, nullptr, &pipeline);

        if (result == VK_SUCCESS) {
            mGraphicsPipelines[name] = pipeline;
            return true;
        }

        return false;
    }

    bool PipelineCache::registerComputePipeline(const std::string& name, const VkComputePipelineCreateInfo& createInfo) {
        VkPipeline pipeline;
        VkResult result = vkCreateComputePipelines(mDevice, mPipelineCache, 1, &createInfo, nullptr, &pipeline);

        if (result == VK_SUCCESS) {
            mComputePipelines[name] = pipeline;
            return true;
        }

        return false;
    }

} // namespace StarryEngine