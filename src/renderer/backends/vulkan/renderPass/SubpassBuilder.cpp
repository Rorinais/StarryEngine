#include "SubpassBuilder.hpp"
#include <stdexcept>

namespace StarryEngine {

    SubpassBuilder& SubpassBuilder::addColorAttachment(const std::string& name, VkImageLayout layout) {
        mColorAttachmentNames.push_back(name);
        mAttachmentLayouts[name] = layout;
        return *this;
    }

    SubpassBuilder& SubpassBuilder::addInputAttachment(const std::string& name, VkImageLayout layout) {
        mInputAttachmentNames.push_back(name);
        mAttachmentLayouts[name] = layout;
        return *this;
    }

    SubpassBuilder& SubpassBuilder::addResolveAttachment(const std::string& name, VkImageLayout layout) {
        mResolveAttachmentNames.push_back(name);
        mAttachmentLayouts[name] = layout;
        return *this;
    }

    SubpassBuilder& SubpassBuilder::setDepthStencilAttachment(const std::string& name, VkImageLayout layout) {
        mDepthStencilAttachmentName = name;
        mAttachmentLayouts[name] = layout;
        return *this;
    }

    SubpassBuilder& SubpassBuilder::addPreserveAttachment(const std::string& name) {
        mPreserveAttachmentNames.push_back(name);
        return *this;
    }

    std::unique_ptr<Subpass> SubpassBuilder::build(const std::unordered_map<std::string, uint32_t>& nameToIndexMap,
        VkPipelineBindPoint bindPoint) {
        auto subpass = std::make_unique<Subpass>();

        // 构建颜色附件引用
        for (const auto& name : mColorAttachmentNames) {
            auto it = nameToIndexMap.find(name);
            if (it == nameToIndexMap.end()) {
                throw std::runtime_error("Color attachment not found in index map: " + name);
            }
            subpass->addColorAttachmentRef(it->second, mAttachmentLayouts[name]);
        }

        // 构建输入附件引用
        for (const auto& name : mInputAttachmentNames) {
            auto it = nameToIndexMap.find(name);
            if (it == nameToIndexMap.end()) {
                throw std::runtime_error("Input attachment not found in index map: " + name);
            }
            subpass->addInputAttachmentRef(it->second, mAttachmentLayouts[name]);
        }

        // 构建解析附件引用
        for (const auto& name : mResolveAttachmentNames) {
            auto it = nameToIndexMap.find(name);
            if (it == nameToIndexMap.end()) {
                throw std::runtime_error("Resolve attachment not found in index map: " + name);
            }
            VkAttachmentReference ref{ it->second, mAttachmentLayouts[name] };
            subpass->addResolveAttachmentRef(ref);
        }

        // 构建深度模板附件引用
        if (mDepthStencilAttachmentName) {
            auto it = nameToIndexMap.find(*mDepthStencilAttachmentName);
            if (it == nameToIndexMap.end()) {
                throw std::runtime_error("Depth/stencil attachment not found in index map: " + *mDepthStencilAttachmentName);
            }
            VkAttachmentReference ref{ it->second, mAttachmentLayouts[*mDepthStencilAttachmentName] };
            subpass->setDepthStencilAttachmentRef(ref);
        }

        // 构建保留附件引用
        for (const auto& name : mPreserveAttachmentNames) {
            auto it = nameToIndexMap.find(name);
            if (it == nameToIndexMap.end()) {
                throw std::runtime_error("Preserve attachment not found in index map: " + name);
            }
            subpass->addPreserveAttachmentRef(it->second);
        }

        subpass->biuldSubpassDescription(bindPoint);
        return subpass;
    }

} // namespace StarryEngine