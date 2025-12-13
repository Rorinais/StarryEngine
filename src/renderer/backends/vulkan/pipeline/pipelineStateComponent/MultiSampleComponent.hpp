#pragma once
#include "../interface/TypedPipelineComponent.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <stdexcept>
#include <set>
#include <unordered_map>

namespace StarryEngine {
    class MultiSampleComponent : public
        TypedPipelineComponent<MultiSampleComponent, PipelineComponentType::MULTISAMPLE> {
    public:
        MultiSampleComponent(const std::string& name);
        MultiSampleComponent& reset();

        MultiSampleComponent& setRasterizationSamples(VkSampleCountFlagBits samples);
        MultiSampleComponent& enableSampleShading(VkBool32 enable);
        MultiSampleComponent& setMinSampleShading(float minSampleShading);
        MultiSampleComponent& setSampleMask(const std::vector<VkSampleMask>& sampleMask);
        MultiSampleComponent& enableAlphaToCoverage(VkBool32 enable);
        MultiSampleComponent& enableAlphaToOne(VkBool32 enable);

        void apply(VkGraphicsPipelineCreateInfo& pipelineInfo) override {
            updateCreateInfo();
            pipelineInfo.pMultisampleState = &mCreateInfo;
        }

        std::string getDescription() const override;
        bool isValid() const override;

    private:
        VkPipelineMultisampleStateCreateInfo mCreateInfo{};
        std::vector<VkSampleMask> mSampleMask;

        void updateCreateInfo();
    };
} // namespace StarryEngine