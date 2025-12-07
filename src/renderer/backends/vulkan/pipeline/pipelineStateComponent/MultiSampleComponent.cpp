#include "MultiSampleComponent.hpp"

namespace StarryEngine {

    MultiSampleComponent::MultiSampleComponent(const std::string& name) {
        setName(name);
        reset();  // 使用reset初始化默认值
    }

    MultiSampleComponent& MultiSampleComponent::reset() {
        // 设置Vulkan默认的多重采样状态
        mCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,  // 默认不启用多重采样
            .sampleShadingEnable = VK_FALSE,                // 禁用采样着色
            .minSampleShading = 1.0f,                       // 最小采样着色比例
            .pSampleMask = nullptr,                         // 无样本掩码
            .alphaToCoverageEnable = VK_FALSE,              // 禁用alpha到覆盖率
            .alphaToOneEnable = VK_FALSE                    // 禁用alpha到一
        };

        mSampleMask.clear();
        return *this;
    }

    MultiSampleComponent& MultiSampleComponent::setRasterizationSamples(VkSampleCountFlagBits samples) {
        mCreateInfo.rasterizationSamples = samples;
        return *this;
    }

    MultiSampleComponent& MultiSampleComponent::enableSampleShading(VkBool32 enable) {
        mCreateInfo.sampleShadingEnable = enable;
        return *this;
    }

    MultiSampleComponent& MultiSampleComponent::setMinSampleShading(float minSampleShading) {
        mCreateInfo.minSampleShading = minSampleShading;
        return *this;
    }

    MultiSampleComponent& MultiSampleComponent::setSampleMask(const std::vector<VkSampleMask>& sampleMask) {
        mSampleMask = sampleMask;
        return *this;
    }

    MultiSampleComponent& MultiSampleComponent::enableAlphaToCoverage(VkBool32 enable) {
        mCreateInfo.alphaToCoverageEnable = enable;
        return *this;
    }

    MultiSampleComponent& MultiSampleComponent::enableAlphaToOne(VkBool32 enable) {
        mCreateInfo.alphaToOneEnable = enable;
        return *this;
    }

    std::string MultiSampleComponent::getDescription() const {
        std::string desc = "Multisample State: ";

        // 采样数量
        desc += "Samples=";
        switch (mCreateInfo.rasterizationSamples) {
        case VK_SAMPLE_COUNT_1_BIT: desc += "1"; break;
        case VK_SAMPLE_COUNT_2_BIT: desc += "2"; break;
        case VK_SAMPLE_COUNT_4_BIT: desc += "4"; break;
        case VK_SAMPLE_COUNT_8_BIT: desc += "8"; break;
        case VK_SAMPLE_COUNT_16_BIT: desc += "16"; break;
        case VK_SAMPLE_COUNT_32_BIT: desc += "32"; break;
        case VK_SAMPLE_COUNT_64_BIT: desc += "64"; break;
        default: desc += "UNKNOWN"; break;
        }

        // 采样着色
        if (mCreateInfo.sampleShadingEnable) {
            desc += ", SampleShading=ENABLED";
            desc += ", MinSampleShading=" + std::to_string(mCreateInfo.minSampleShading);
        }

        // 样本掩码
        if (!mSampleMask.empty()) {
            desc += ", SampleMask=[";
            for (size_t i = 0; i < mSampleMask.size(); ++i) {
                if (i > 0) desc += ", ";
                desc += "0x" + std::to_string(mSampleMask[i]);
            }
            desc += "]";
        }

        // Alpha到覆盖率
        if (mCreateInfo.alphaToCoverageEnable) {
            desc += ", AlphaToCoverage=ENABLED";
        }

        // Alpha到一
        if (mCreateInfo.alphaToOneEnable) {
            desc += ", AlphaToOne=ENABLED";
        }

        return desc;
    }

    bool MultiSampleComponent::isValid() const {
        // 检查基础结构类型
        if (mCreateInfo.sType != VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO) {
            return false;
        }

        // 检查采样数量是否有效
        VkSampleCountFlagBits validSamples[] = {
            VK_SAMPLE_COUNT_1_BIT,
            VK_SAMPLE_COUNT_2_BIT,
            VK_SAMPLE_COUNT_4_BIT,
            VK_SAMPLE_COUNT_8_BIT,
            VK_SAMPLE_COUNT_16_BIT,
            VK_SAMPLE_COUNT_32_BIT,
            VK_SAMPLE_COUNT_64_BIT
        };

        bool validSampleCount = false;
        for (auto sampleCount : validSamples) {
            if (mCreateInfo.rasterizationSamples == sampleCount) {
                validSampleCount = true;
                break;
            }
        }
        if (!validSampleCount) {
            return false;
        }

        // 检查最小采样着色是否在有效范围内
        if (mCreateInfo.minSampleShading < 0.0f || mCreateInfo.minSampleShading > 1.0f) {
            return false;
        }

        // 检查采样掩码是否与采样数量匹配
        // 每个样本掩码包含32个样本位，所以需要的掩码数量 = ceil(采样数量 / 32)
        if (!mSampleMask.empty()) {
            uint32_t requiredMaskCount = (mCreateInfo.rasterizationSamples + 31) / 32;
            if (mSampleMask.size() < requiredMaskCount) {
                return false;
            }
        }

        // 如果启用了采样着色，需要确保minSampleShading不为0
        if (mCreateInfo.sampleShadingEnable && mCreateInfo.minSampleShading <= 0.0f) {
            return false;
        }

        return true;
    }

    void MultiSampleComponent::updateCreateInfo() {
        // 确保结构体类型正确
        mCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        mCreateInfo.pNext = nullptr;

        // 更新样本掩码指针
        mCreateInfo.pSampleMask = mSampleMask.empty() ? nullptr : mSampleMask.data();
    }

} // namespace StarryEngine